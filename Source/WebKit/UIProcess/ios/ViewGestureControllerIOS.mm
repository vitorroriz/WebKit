/*
 * Copyright (C) 2013, 2014 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "ViewGestureController.h"

#if PLATFORM(IOS_FAMILY)

#import "APINavigation.h"
#import "DrawingAreaProxy.h"
#import "UIKitSPI.h"
#import "ViewGestureControllerMessages.h"
#import "ViewGestureGeometryCollectorMessages.h"
#import "ViewSnapshotStore.h"
#import "WKBackForwardListItemInternal.h"
#import "WKWebViewInternal.h"
#import "WebBackForwardList.h"
#import "WebPageGroup.h"
#import "WebPageMessages.h"
#import "WebPageProxy.h"
#import "WebProcessProxy.h"
#import <UIKit/UIScreenEdgePanGestureRecognizer.h>
#import <WebCore/ColorCocoa.h>
#import <WebCore/IOSurface.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <wtf/WeakObjCPtr.h>

@interface WKSwipeTransitionController : NSObject <_UINavigationInteractiveTransitionBaseDelegate>
- (instancetype)initWithViewGestureController:(WebKit::ViewGestureController*)gestureController gestureRecognizerView:(UIView *)gestureRecognizerView;
- (void)invalidate;

- (_UINavigationInteractiveTransitionBase *)transitionForDirection:(WebKit::ViewGestureController::SwipeDirection)direction;
@end

@implementation WKSwipeTransitionController
{
    WebKit::ViewGestureController *_gestureController;
    RetainPtr<_UINavigationInteractiveTransitionBase> _backTransitionController;
    RetainPtr<_UINavigationInteractiveTransitionBase> _forwardTransitionController;
    WeakObjCPtr<UIView> _gestureRecognizerView;
}

static const float swipeSnapshotRemovalRenderTreeSizeTargetFraction = 0.5;

- (instancetype)initWithViewGestureController:(WebKit::ViewGestureController*)gestureController gestureRecognizerView:(UIView *)gestureRecognizerView
{
    self = [super init];
    if (self) {
        _gestureController = gestureController;
        _gestureRecognizerView = gestureRecognizerView;

        _backTransitionController = adoptNS([_UINavigationInteractiveTransitionBase alloc]);
        _backTransitionController = [_backTransitionController initWithGestureRecognizerView:gestureRecognizerView animator:nil delegate:self];
        
        _forwardTransitionController = adoptNS([_UINavigationInteractiveTransitionBase alloc]);
        _forwardTransitionController = [_forwardTransitionController initWithGestureRecognizerView:gestureRecognizerView animator:nil delegate:self];
        [_forwardTransitionController setShouldReverseTranslation:YES];

#if HAVE(CONTENT_SWIPE_GESTURE_RECOGNIZER)
        [[_backTransitionController contentSwipeGestureRecognizer] setEnabled:NO];
        [[_forwardTransitionController contentSwipeGestureRecognizer] setEnabled:NO];
#endif
    }
    return self;
}

- (void)invalidate
{
    _gestureController = nullptr;
}

- (WebKit::ViewGestureController::SwipeDirection)directionForTransition:(_UINavigationInteractiveTransitionBase *)transition
{
    return transition == _backTransitionController ? WebKit::ViewGestureController::SwipeDirection::Back : WebKit::ViewGestureController::SwipeDirection::Forward;
}

- (_UINavigationInteractiveTransitionBase *)transitionForDirection:(WebKit::ViewGestureController::SwipeDirection)direction
{
    return direction == WebKit::ViewGestureController::SwipeDirection::Back ? _backTransitionController.get() : _forwardTransitionController.get();
}

- (void)startInteractiveTransition:(_UINavigationInteractiveTransitionBase *)transition
{
    _gestureController->beginSwipeGesture(transition, [self directionForTransition:transition]);
}

- (BOOL)shouldBeginInteractiveTransition:(_UINavigationInteractiveTransitionBase *)transition
{
    if (_gestureController->hasActiveSwipeGesture())
        return NO;

    using enum WebKit::ViewGestureController::DeferToConflictingGestures;
    auto deferToConflictingGestures = transition.gestureRecognizer.state == UIGestureRecognizerStateFailed ? Yes : No;
    return _gestureController->canSwipeInDirection([self directionForTransition:transition], deferToConflictingGestures);
}

- (BOOL)interactiveTransition:(_UINavigationInteractiveTransitionBase *)transition gestureRecognizer:(UIGestureRecognizer *)gestureRecognizer shouldRecognizeSimultaneouslyWithGestureRecognizer:(UIGestureRecognizer *)otherGestureRecognizer
{
    return [otherGestureRecognizer isKindOfClass:[UITapGestureRecognizer class]];
}

- (BOOL)interactiveTransition:(_UINavigationInteractiveTransitionBase *)transition gestureRecognizer:(UIGestureRecognizer *)gestureRecognizer shouldReceiveTouch:(UITouch *)touch
{
    return YES;
}

static Class interactiveTransitionGestureRecognizerClass()
{
#if HAVE(UI_PARALLAX_TRANSITION_GESTURE_RECOGNIZER)
ALLOW_NEW_API_WITHOUT_GUARDS_BEGIN
    return [_UIParallaxTransitionPanGestureRecognizer class];
ALLOW_NEW_API_WITHOUT_GUARDS_END
#else
    return [UIScreenEdgePanGestureRecognizer class];
#endif
}

- (UIPanGestureRecognizer *)gestureRecognizerForInteractiveTransition:(_UINavigationInteractiveTransitionBase *)transition WithTarget:(id)target action:(SEL)action
{
    RetainPtr recognizer = adoptNS([[interactiveTransitionGestureRecognizerClass() alloc] initWithTarget:target action:action]);

    bool isLTR = [UIView userInterfaceLayoutDirectionForSemanticContentAttribute:[_gestureRecognizerView.get() semanticContentAttribute]] == UIUserInterfaceLayoutDirectionLeftToRight;

    switch ([self directionForTransition:transition]) {
    case WebKit::ViewGestureController::SwipeDirection::Back:
        [recognizer setEdges:isLTR ? UIRectEdgeLeft : UIRectEdgeRight];
        break;
    case WebKit::ViewGestureController::SwipeDirection::Forward:
        [recognizer setEdges:isLTR ? UIRectEdgeRight : UIRectEdgeLeft];
        break;
    }
    return recognizer.autorelease();
}

- (BOOL)isNavigationSwipeGestureRecognizer:(UIGestureRecognizer *)recognizer
{
    if (recognizer == [_backTransitionController gestureRecognizer] || recognizer == [_forwardTransitionController gestureRecognizer])
        return YES;

    if ([recognizer isKindOfClass:interactiveTransitionGestureRecognizerClass()])
        return recognizer.delegate == _backTransitionController || recognizer.delegate == _forwardTransitionController;

    return NO;
}

@end

namespace WebKit {

void ViewGestureController::platformTeardown()
{
    [m_swipeTransitionContext _setTransitionIsInFlight:NO];
    [m_swipeTransitionContext _setInteractor:nil];
    [m_swipeTransitionContext _setAnimator:nil];
    [m_swipeInteractiveTransitionDelegate invalidate];
}

bool ViewGestureController::isNavigationSwipeGestureRecognizer(UIGestureRecognizer *recognizer) const
{
    return [m_swipeInteractiveTransitionDelegate isNavigationSwipeGestureRecognizer:recognizer];
}

void ViewGestureController::installSwipeHandler(UIView *gestureRecognizerView, UIView *swipingView)
{
    ASSERT(!m_swipeInteractiveTransitionDelegate);
    m_swipeInteractiveTransitionDelegate = adoptNS([[WKSwipeTransitionController alloc] initWithViewGestureController:this gestureRecognizerView:gestureRecognizerView]);
    m_liveSwipeView = swipingView;
}

void ViewGestureController::beginSwipeGesture(_UINavigationInteractiveTransitionBase *transition, SwipeDirection direction)
{
    if (m_activeGestureType != ViewGestureType::None)
        return;

    RefPtr page = m_webPageProxy.get();
    if (!page)
        return;

    willBeginGesture(ViewGestureType::Swipe);

    page->recordAutomaticNavigationSnapshot();

    RefPtr<WebPageProxy> alternateBackForwardListSourcePage = m_alternateBackForwardListSourcePage.get();
    m_webPageProxyForBackForwardListForCurrentSwipe = alternateBackForwardListSourcePage ? alternateBackForwardListSourcePage.get() : page.get();

    auto& backForwardList = m_webPageProxyForBackForwardListForCurrentSwipe->backForwardList();
    RefPtr targetItem = itemForSwipeDirection(direction);
    if (!targetItem) {
        RELEASE_LOG_ERROR(ViewGestures, "Failed to find %s item when beginning swipe.", direction == SwipeDirection::Back ? "back" : "forward");
        didEndGesture();
        return;
    }

    m_webPageProxyForBackForwardListForCurrentSwipe->navigationGestureDidBegin();
    if (page.get() != m_webPageProxyForBackForwardListForCurrentSwipe)
        page->navigationGestureDidBegin();

    // Copy the snapshot from this view to the one that owns the back forward list, so that
    // swiping forward will have the correct snapshot.
    if (m_webPageProxyForBackForwardListForCurrentSwipe != page.get()) {
        if (auto* currentViewHistoryItem = page->backForwardList().currentItem())
            backForwardList.currentItem()->setSnapshot(currentViewHistoryItem->snapshot());
    }

    CGRect liveSwipeViewFrame = [m_liveSwipeView frame];

    RetainPtr<UIViewController> snapshotViewController = adoptNS([[UIViewController alloc] init]);
    m_snapshotView = adoptNS([[UIView alloc] initWithFrame:liveSwipeViewFrame]);

    // Disabling user interaction on the snapshot view lets the gestures go through the snapshot view, to the
    // actual underlying view.
    [m_snapshotView setUserInteractionEnabled:NO];

    [m_snapshotView layer].name = @"SwipeSnapshot";

    RetainPtr<UIColor> backgroundColor = [UIColor whiteColor];
    if (ViewSnapshot* snapshot = targetItem->snapshot()) {
        float deviceScaleFactor = page->deviceScaleFactor();
        WebCore::FloatSize swipeLayerSizeInDeviceCoordinates(liveSwipeViewFrame.size);
        swipeLayerSizeInDeviceCoordinates.scale(deviceScaleFactor);
        
        BOOL shouldRestoreScrollPosition = targetItem->mainFrameState()->shouldRestoreScrollPosition;
        WebCore::IntPoint currentScrollPosition = WebCore::roundedIntPoint(page->viewScrollPosition());

        if (snapshot->hasImage() && snapshot->size() == swipeLayerSizeInDeviceCoordinates && deviceScaleFactor == snapshot->deviceScaleFactor() && (shouldRestoreScrollPosition || (currentScrollPosition == snapshot->viewScrollPosition())))
            [m_snapshotView layer].contents = snapshot->asLayerContents();
        WebCore::Color coreColor = snapshot->backgroundColor();
        if (coreColor.isValid())
            backgroundColor = cocoaColor(coreColor);
    }

    [m_snapshotView setBackgroundColor:backgroundColor.get()];
    [m_snapshotView layer].contentsGravity = kCAGravityTopLeft;
    [m_snapshotView layer].contentsScale = m_liveSwipeView.window.screen.scale;
    [snapshotViewController setView:m_snapshotView.get()];

    m_transitionContainerView = adoptNS([[UIView alloc] initWithFrame:liveSwipeViewFrame]);
    [m_transitionContainerView layer].name = @"SwipeTransitionContainer";
    m_liveSwipeViewClippingView = adoptNS([[UIView alloc] initWithFrame:liveSwipeViewFrame]);
    [m_liveSwipeViewClippingView layer].name = @"LiveSwipeViewClipping";

    [m_liveSwipeViewClippingView setClipsToBounds:YES];

    [m_liveSwipeView.superview insertSubview:m_transitionContainerView.get() belowSubview:m_liveSwipeView];
    [m_transitionContainerView addSubview:m_liveSwipeViewClippingView.get()];
    [m_liveSwipeViewClippingView addSubview:m_liveSwipeView];

    RetainPtr<UIViewController> targettedViewController = adoptNS([[UIViewController alloc] init]);
    [targettedViewController setView:m_liveSwipeViewClippingView.get()];

    UINavigationControllerOperation transitionOperation = direction == SwipeDirection::Back ? UINavigationControllerOperationPop : UINavigationControllerOperationPush;
    RetainPtr<_UINavigationParallaxTransition> animationController = adoptNS([[_UINavigationParallaxTransition alloc] initWithCurrentOperation:transitionOperation]);

    m_swipeTransitionContext = adoptNS([[_UIViewControllerOneToOneTransitionContext alloc] init]);
    [m_swipeTransitionContext _setFromViewController:targettedViewController.get()];
    [m_swipeTransitionContext _setToViewController:snapshotViewController.get()];
    [m_swipeTransitionContext _setContainerView:m_transitionContainerView.get()];
    [m_swipeTransitionContext _setFromStartFrame:liveSwipeViewFrame];
    [m_swipeTransitionContext _setToEndFrame:liveSwipeViewFrame];
    [m_swipeTransitionContext _setToStartFrame:CGRectZero];
    [m_swipeTransitionContext _setFromEndFrame:CGRectZero];
    [m_swipeTransitionContext _setAnimator:animationController.get()];
    [m_swipeTransitionContext _setInteractor:transition];
    [m_swipeTransitionContext _setTransitionIsInFlight:YES];
    m_didCallWillEndSwipeGesture = false;
    m_didCallEndSwipeGesture = false;
    m_removeSnapshotImmediatelyWhenGestureEnds = false;
    [[m_swipeTransitionContext _transitionCoordinator] notifyWhenInteractionChangesUsingBlock:^(id<UIViewControllerTransitionCoordinatorContext> context) {
        if (!context.interactive)
            willEndSwipeGesture(*targetItem, context.cancelled);
    }];
    auto pageID = page->identifier();
    GestureID gestureID = m_currentGestureID;
    [m_swipeTransitionContext _setCompletionHandler:[pageID, gestureID, targetItem] (_UIViewControllerTransitionContext *context, BOOL didComplete) {
        if (auto gestureController = controllerForGesture(pageID, gestureID))
            gestureController->endSwipeGesture(targetItem.get(), context, !didComplete);
    }];

    [transition setAnimationController:animationController.get()];
    [transition startInteractiveTransition:m_swipeTransitionContext.get()];
}

void ViewGestureController::willEndSwipeGesture(WebBackForwardListItem& targetItem, bool cancelled)
{
    m_didCallWillEndSwipeGesture = true;
    m_webPageProxyForBackForwardListForCurrentSwipe->navigationGestureWillEnd(!cancelled, targetItem);

    if (cancelled)
        return;

    m_snapshotRemovalTargetRenderTreeSize = 0;
    if (ViewSnapshot* snapshot = targetItem.snapshot())
        m_snapshotRemovalTargetRenderTreeSize = snapshot->renderTreeSize() * swipeSnapshotRemovalRenderTreeSizeTargetFraction;

    m_didStartProvisionalLoad = false;
    m_pendingNavigation = m_webPageProxyForBackForwardListForCurrentSwipe->goToBackForwardItem(targetItem);

    auto* currentItem = m_webPageProxyForBackForwardListForCurrentSwipe->backForwardList().currentItem();
    // The main frame will not be navigated so hide the snapshot right away.
    if (currentItem && currentItem->itemIsClone(targetItem)) {
        removeSwipeSnapshot();
        return;
    }

    // FIXME: Should we wait for VisuallyNonEmptyLayout like we do on Mac?
    m_snapshotRemovalTracker.start(SnapshotRemovalTracker::RenderTreeSizeThreshold
        | SnapshotRemovalTracker::RepaintAfterNavigation
        | SnapshotRemovalTracker::MainFrameLoad
        | SnapshotRemovalTracker::SubresourceLoads
        | SnapshotRemovalTracker::ScrollPositionRestoration, [this] {
        this->removeSwipeSnapshot();
    });

    if (ViewSnapshot* snapshot = targetItem.snapshot()) {
        m_backgroundColorForCurrentSnapshot = snapshot->backgroundColor();
        if (RefPtr page = m_webPageProxy.get())
            page->didChangeBackgroundColor();
    }
}

void ViewGestureController::endSwipeGesture(WebBackForwardListItem* targetItem, _UIViewControllerTransitionContext *context, bool cancelled)
{
    // At least in the context of a simulated swipe, it is possible that endSwipeGesture() gets called but not willEndSwipeGesture().
    if (!m_didCallWillEndSwipeGesture)
        willEndSwipeGesture(*targetItem, cancelled);

    m_didCallEndSwipeGesture = true;

    [context _setTransitionIsInFlight:NO];
    [context _setInteractor:nil];
    [context _setAnimator:nil];
    
    [[m_transitionContainerView superview] insertSubview:m_snapshotView.get() aboveSubview:m_transitionContainerView.get()];
    [[m_transitionContainerView superview] insertSubview:m_liveSwipeView aboveSubview:m_transitionContainerView.get()];
    [m_liveSwipeViewClippingView removeFromSuperview];
    m_liveSwipeViewClippingView = nullptr;
    [m_transitionContainerView removeFromSuperview];
    m_transitionContainerView = nullptr;

    RefPtr page = m_webPageProxy.get();
    if (cancelled) {
        // removeSwipeSnapshot will clear m_webPageProxyForBackForwardListForCurrentSwipe, so hold on to it here.
        RefPtr<WebPageProxy> webPageProxyForBackForwardListForCurrentSwipe = m_webPageProxyForBackForwardListForCurrentSwipe;
        removeSwipeSnapshot();
        webPageProxyForBackForwardListForCurrentSwipe->navigationGestureDidEnd(false, *targetItem);
        if (page.get() != webPageProxyForBackForwardListForCurrentSwipe)
            page->navigationGestureDidEnd();
        return;
    }

    m_webPageProxyForBackForwardListForCurrentSwipe->navigationGestureDidEnd(true, *targetItem);
    if (page.get() != m_webPageProxyForBackForwardListForCurrentSwipe)
        page->navigationGestureDidEnd();

    if (!page->provisionalDrawingArea()) {
        removeSwipeSnapshot();
        return;
    }

    // removeSwipeSnapshot() was called between willEndSwipeGesture() and endSwipeGesture().
    // We couldn't remove it then, because the animation was still running, but now we can!
    if (m_removeSnapshotImmediatelyWhenGestureEnds) {
        removeSwipeSnapshot();
        return;
    }

    auto pageID = page->identifier();
    GestureID gestureID = m_currentGestureID;

    auto doAfterLoadStart = [this, pageID, gestureID] {
        RefPtr page = m_webPageProxy.get();
        auto* drawingArea = page ? page->provisionalDrawingArea() : nullptr;
        if (!drawingArea) {
            removeSwipeSnapshot();
            return;
        }

        page->callAfterNextPresentationUpdate([pageID, gestureID] {
            if (auto gestureController = controllerForGesture(pageID, gestureID))
                gestureController->willCommitPostSwipeTransitionLayerTree(true);
        });
        drawingArea->hideContentUntilPendingUpdate();
    };

    if (m_didStartProvisionalLoad)
        doAfterLoadStart();
    else
        m_loadCallback = WTFMove(doAfterLoadStart);
}

void ViewGestureController::setRenderTreeSize(uint64_t renderTreeSize)
{
    if (m_activeGestureType != ViewGestureType::Swipe)
        return;

    if (!m_snapshotRemovalTargetRenderTreeSize || renderTreeSize > m_snapshotRemovalTargetRenderTreeSize)
        didHitRenderTreeSizeThreshold();
}

void ViewGestureController::willCommitPostSwipeTransitionLayerTree(bool successful)
{
    if (m_activeGestureType != ViewGestureType::Swipe)
        return;

    if (!successful) {
        removeSwipeSnapshot();
        return;
    }

    didRepaintAfterNavigation();
}

void ViewGestureController::removeSwipeSnapshot()
{
    m_snapshotRemovalTracker.reset();

    if (m_activeGestureType != ViewGestureType::Swipe)
        return;

    if (!m_didCallEndSwipeGesture) {
        m_removeSnapshotImmediatelyWhenGestureEnds = true;
        return;
    }

    resetState();
}

void ViewGestureController::resetState()
{
    [m_snapshotView removeFromSuperview];
    m_snapshotView = nullptr;
    
    m_snapshotRemovalTargetRenderTreeSize = 0;

    if (m_webPageProxyForBackForwardListForCurrentSwipe) {
        m_webPageProxyForBackForwardListForCurrentSwipe->navigationGestureSnapshotWasRemoved();
        m_webPageProxyForBackForwardListForCurrentSwipe = nullptr;
    }

    m_swipeTransitionContext = nullptr;

    m_backgroundColorForCurrentSnapshot = WebCore::Color();

    m_pendingNavigation = nullptr;

    didEndGesture();
}

void ViewGestureController::reset()
{
    removeSwipeSnapshot();
    resetState();
}

bool ViewGestureController::beginSimulatedSwipeInDirectionForTesting(SwipeDirection direction)
{
    if (!canSwipeInDirection(direction, DeferToConflictingGestures::No))
        return false;

    _UINavigationInteractiveTransitionBase *transition = [m_swipeInteractiveTransitionDelegate transitionForDirection:direction];
    beginSwipeGesture(transition, direction);
    [transition _stopInteractiveTransition];

    return true;
}

bool ViewGestureController::completeSimulatedSwipeInDirectionForTesting(SwipeDirection direction)
{
    _UINavigationInteractiveTransitionBase *transition = [m_swipeInteractiveTransitionDelegate transitionForDirection:direction];
    [transition _completeStoppedInteractiveTransition];

    return true;
}

} // namespace WebKit

#endif // PLATFORM(IOS_FAMILY)
