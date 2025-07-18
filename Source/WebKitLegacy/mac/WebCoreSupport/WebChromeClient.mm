/*
 * Copyright (C) 2006-2025 Apple Inc. All rights reserved.
 * Copyright (C) 2008, 2010 Nokia Corporation and/or its subsidiary(-ies)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "WebChromeClient.h"

#import "DOMElementInternal.h"
#import "DOMHTMLInputElementInternal.h"
#import "DOMNodeInternal.h"
#import "PopupMenuMac.h"
#import "SearchPopupMenuMac.h"
#import "WebBasePluginPackage.h"
#import "WebDefaultUIDelegate.h"
#import "WebDelegateImplementationCaching.h"
#import "WebElementDictionary.h"
#import "WebFormDelegate.h"
#import "WebFrameInternal.h"
#import "WebFrameView.h"
#import "WebHTMLViewInternal.h"
#import "WebHistoryInternal.h"
#import "WebKitFullScreenListener.h"
#import "WebKitPrefix.h"
#import "WebNSURLRequestExtras.h"
#import "WebOpenPanelResultListener.h"
#import "WebPlugin.h"
#import "WebQuotaManager.h"
#import "WebSecurityOriginInternal.h"
#import "WebSelectionServiceController.h"
#import "WebUIDelegatePrivate.h"
#import "WebView.h"
#import "WebViewInternal.h"
#import <Foundation/Foundation.h>
#import <JavaScriptCore/ConsoleTypes.h>
#import <WebCore/Chrome.h>
#import <WebCore/ColorChooser.h>
#import <WebCore/ContextMenu.h>
#import <WebCore/ContextMenuController.h>
#import <WebCore/CookieConsentDecisionResult.h>
#import <WebCore/Cursor.h>
#import <WebCore/DataListSuggestionPicker.h>
#import <WebCore/DocumentFullscreen.h>
#import <WebCore/Element.h>
#import <WebCore/FileChooser.h>
#import <WebCore/FileIconLoader.h>
#import <WebCore/FloatRect.h>
#import <WebCore/GraphicsLayer.h>
#import <WebCore/HTMLInputElement.h>
#import <WebCore/HTMLNames.h>
#import <WebCore/HTMLPlugInImageElement.h>
#import <WebCore/HTMLVideoElement.h>
#import <WebCore/HitTestResult.h>
#import <WebCore/Icon.h>
#import <WebCore/IntPoint.h>
#import <WebCore/IntRect.h>
#import <WebCore/LocalFrame.h>
#import <WebCore/LocalFrameView.h>
#import <WebCore/ModalContainerTypes.h>
#import <WebCore/NavigationAction.h>
#import <WebCore/NotImplemented.h>
#import <WebCore/Page.h>
#import <WebCore/PlatformScreen.h>
#import <WebCore/ResourceRequest.h>
#import <WebCore/SerializedCryptoKeyWrap.h>
#import <WebCore/StorageNamespaceProvider.h>
#import <WebCore/UniversalAccessZoom.h>
#import <WebCore/Widget.h>
#import <WebCore/WindowFeatures.h>
#import <pal/spi/mac/NSViewSPI.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/RefPtr.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/Vector.h>
#import <wtf/text/WTFString.h>

#if HAVE(WEBGPU_IMPLEMENTATION)
#import <WebCore/WebGPU.h>
#import <WebCore/WebGPUCreateImpl.h>
#endif

#if HAVE(SHAPE_DETECTION_API_IMPLEMENTATION)
#import <WebCore/BarcodeDetectorImplementation.h>
#import <WebCore/FaceDetectorImplementation.h>
#import <WebCore/TextDetectorImplementation.h>
#endif

#if PLATFORM(IOS_FAMILY) && ENABLE(GEOLOCATION)
#import <WebCore/Geolocation.h>
#endif

#if ENABLE(POINTER_LOCK)
#import <WebCore/PointerLockController.h>
#endif

#if PLATFORM(IOS_FAMILY)
#import <WebCore/WAKClipView.h>
#import <WebCore/WAKWindow.h>
#import <WebCore/WebCoreThreadMessage.h>
#endif

NSString *WebConsoleMessageXMLMessageSource = @"XMLMessageSource";
NSString *WebConsoleMessageJSMessageSource = @"JSMessageSource";
NSString *WebConsoleMessageNetworkMessageSource = @"NetworkMessageSource";
NSString *WebConsoleMessageConsoleAPIMessageSource = @"ConsoleAPIMessageSource";
NSString *WebConsoleMessageStorageMessageSource = @"StorageMessageSource";
NSString *WebConsoleMessageRenderingMessageSource = @"RenderingMessageSource";
NSString *WebConsoleMessageCSSMessageSource = @"CSSMessageSource";
NSString *WebConsoleMessageAccessibilityMessageSource = @"AccessibilityMessageSource";
NSString *WebConsoleMessageSecurityMessageSource = @"SecurityMessageSource";
NSString *WebConsoleMessageContentBlockerMessageSource = @"ContentBlockerMessageSource";
NSString *WebConsoleMessageMediaMessageSource = @"MediaMessageSource";
NSString *WebConsoleMessageMediaSourceMessageSource = @"MediaSourceMessageSource";
NSString *WebConsoleMessageWebRTCMessageSource = @"WebRTCMessageSource";
NSString *WebConsoleMessageITPDebugMessageSource = @"ITPDebugMessageSource";
NSString *WebConsoleMessagePrivateClickMeasurementMessageSource = @"PrivateClickMeasurementMessageSource";
NSString *WebConsoleMessagePaymentRequestMessageSource = @"PaymentRequestMessageSource";
NSString *WebConsoleMessageOtherMessageSource = @"OtherMessageSource";

NSString *WebConsoleMessageDebugMessageLevel = @"DebugMessageLevel";
NSString *WebConsoleMessageLogMessageLevel = @"LogMessageLevel";
NSString *WebConsoleMessageInfoMessageLevel = @"InfoMessageLevel";
NSString *WebConsoleMessageWarningMessageLevel = @"WarningMessageLevel";
NSString *WebConsoleMessageErrorMessageLevel = @"ErrorMessageLevel";


#if !PLATFORM(IOS_FAMILY)
@interface NSApplication (WebNSApplicationDetails)
- (NSCursor *)_cursorRectCursor;
@end
#endif

// For compatibility with old SPI.
@interface NSView (WebOldWebKitPlugInDetails)
- (void)setIsSelected:(BOOL)isSelected;
@end

using namespace WebCore;
using namespace HTMLNames;

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebChromeClient);

WebChromeClient::WebChromeClient(WebView *webView) 
    : m_webView(webView)
{
}

void WebChromeClient::chromeDestroyed()
{
}

// These functions scale between window and WebView coordinates because JavaScript/DOM operations 
// assume that the WebView and the window share the same coordinate system.

void WebChromeClient::setWindowRect(const FloatRect& rect)
{
#if !PLATFORM(IOS_FAMILY)
    NSRect windowRect = toDeviceSpace(rect, [m_webView window]);
    [[m_webView _UIDelegateForwarder] webView:m_webView setFrame:windowRect];
#endif
}

FloatRect WebChromeClient::windowRect() const
{
#if !PLATFORM(IOS_FAMILY)
    NSRect windowRect = [[m_webView _UIDelegateForwarder] webViewFrame:m_webView];
    return toUserSpace(windowRect, [m_webView window]);
#else
    return FloatRect();
#endif
}

// FIXME: We need to add API for setting and getting this.
FloatRect WebChromeClient::pageRect() const
{
    return [m_webView frame];
}

void WebChromeClient::focus()
{
    [[m_webView _UIDelegateForwarder] webViewFocus:m_webView];
}

void WebChromeClient::unfocus()
{
    [[m_webView _UIDelegateForwarder] webViewUnfocus:m_webView];
}

bool WebChromeClient::canTakeFocus(FocusDirection) const
{
    // There's unfortunately no way to determine if we will become first responder again
    // once we give it up, so we just have to guess that we won't.
    return true;
}

void WebChromeClient::takeFocus(FocusDirection direction)
{
#if !PLATFORM(IOS_FAMILY)
    if (direction == FocusDirection::Forward) {
        // Since we're trying to move focus out of m_webView, and because
        // m_webView may contain subviews within it, we ask it for the next key
        // view of the last view in its key view loop. This makes m_webView
        // behave as if it had no subviews, which is the behavior we want.
        NSView *lastView = [m_webView _findLastViewInKeyViewLoop];
        // avoid triggering assertions if the WebView is the only thing in the key loop
        if ([m_webView _becomingFirstResponderFromOutside] && m_webView == [lastView nextValidKeyView])
            return;
        [[m_webView window] selectKeyViewFollowingView:lastView];
    } else {
        // avoid triggering assertions if the WebView is the only thing in the key loop
        if ([m_webView _becomingFirstResponderFromOutside] && m_webView == [m_webView previousValidKeyView])
            return;
        [[m_webView window] selectKeyViewPrecedingView:m_webView];
    }
#endif
}

void WebChromeClient::focusedElementChanged(Element* element)
{
    if (!is<HTMLInputElement>(element))
        return;

    auto& inputElement = downcast<HTMLInputElement>(*element);
    if (!inputElement.isText())
        return;

    CallFormDelegate(m_webView, @selector(didFocusTextField:inFrame:), kit(&inputElement), kit(inputElement.document().frame()));
}

void WebChromeClient::focusedFrameChanged(Frame*)
{
}

RefPtr<Page> WebChromeClient::createWindow(LocalFrame& frame, const String& openedMainFrameName, const WindowFeatures& features, const NavigationAction&)
{
    id delegate = [m_webView UIDelegate];
    WebView *newWebView;

#if ENABLE(FULLSCREEN_API)
    if (RefPtr document = frame.document()) {
        if (RefPtr documentFullscreen = document->fullscreenIfExists()) {
            if (documentFullscreen->fullscreenElement())
                documentFullscreen->fullyExitFullscreen();
        }
    }
#endif
    
    if ([delegate respondsToSelector:@selector(webView:createWebViewWithRequest:windowFeatures:)]) {
        auto dictFeatures = adoptNS([[NSMutableDictionary alloc] initWithObjectsAndKeys:
            @(features.wantsPopup()), @"wantsPopup",
            @(features.hasAdditionalFeatures), @"hasAdditionalFeatures",
            nil]);
        
        if (features.x)
            [dictFeatures setObject:@(*features.x) forKey:@"x"];
        if (features.y)
            [dictFeatures setObject:@(*features.y) forKey:@"y"];
        if (features.width)
            [dictFeatures setObject:@(*features.width) forKey:@"width"];
        if (features.height)
            [dictFeatures setObject:@(*features.height) forKey:@"height"];
        if (features.popup)
            [dictFeatures setObject:@(*features.dialog) forKey:@"popup"];
        if (features.menuBarVisible)
            [dictFeatures setObject:@(*features.menuBarVisible) forKey:@"menuBarVisible"];
        if (features.statusBarVisible)
            [dictFeatures setObject:@(*features.statusBarVisible) forKey:@"statusBarVisible"];
        if (features.toolBarVisible)
            [dictFeatures setObject:@(*features.toolBarVisible) forKey:@"toolBarVisible"];
        if (features.scrollbarsVisible)
            [dictFeatures setObject:@(*features.scrollbarsVisible) forKey:@"scrollbarsVisible"];
        if (features.resizable)
            [dictFeatures setObject:@(*features.resizable) forKey:@"resizable"];
        if (features.fullscreen)
            [dictFeatures setObject:@(*features.fullscreen) forKey:@"fullscreen"];
        if (features.dialog)
            [dictFeatures setObject:@(*features.dialog) forKey:@"dialog"];

        newWebView = CallUIDelegate(m_webView, @selector(webView:createWebViewWithRequest:windowFeatures:), nil, dictFeatures.get());
    } else if (features.dialog && [delegate respondsToSelector:@selector(webView:createWebViewModalDialogWithRequest:)])
        newWebView = CallUIDelegate(m_webView, @selector(webView:createWebViewModalDialogWithRequest:), nil);
    else
        newWebView = CallUIDelegate(m_webView, @selector(webView:createWebViewWithRequest:), nil);

    RefPtr newPage = core(newWebView);
    if (newPage) {
        if (!features.wantsNoOpener()) {
            m_webView.page->protectedStorageNamespaceProvider()->cloneSessionStorageNamespaceForPage(*m_webView.page, *newPage);
            newPage->mainFrame().setOpenerForWebKitLegacy(&frame);
            newPage->applyWindowFeatures(features);
        }

        auto effectiveSandboxFlags = frame.effectiveSandboxFlags();
        if (!effectiveSandboxFlags.contains(WebCore::SandboxFlag::PropagatesToAuxiliaryBrowsingContexts))
            effectiveSandboxFlags = { };
        newPage->mainFrame().updateSandboxFlags(effectiveSandboxFlags, WebCore::Frame::NotifyUIProcess::No);
        newPage->chrome().show();
        newPage->mainFrame().tree().setSpecifiedName(AtomString(openedMainFrameName));
    }

    return newPage;
}

void WebChromeClient::show()
{
    [[m_webView _UIDelegateForwarder] webViewShow:m_webView];
}

bool WebChromeClient::canRunModal() const
{
    return [[m_webView UIDelegate] respondsToSelector:@selector(webViewRunModal:)];
}

void WebChromeClient::runModal()
{
    CallUIDelegate(m_webView, @selector(webViewRunModal:));
}

void WebChromeClient::setToolbarsVisible(bool b)
{
    [[m_webView _UIDelegateForwarder] webView:m_webView setToolbarsVisible:b];
}

bool WebChromeClient::toolbarsVisible() const
{
    return CallUIDelegateReturningBoolean(NO, m_webView, @selector(webViewAreToolbarsVisible:));
}

void WebChromeClient::setStatusbarVisible(bool b)
{
    [[m_webView _UIDelegateForwarder] webView:m_webView setStatusBarVisible:b];
}

bool WebChromeClient::statusbarVisible() const
{
    return CallUIDelegateReturningBoolean(NO, m_webView, @selector(webViewIsStatusBarVisible:));
}

void WebChromeClient::setScrollbarsVisible(bool b)
{
    [[[m_webView mainFrame] frameView] setAllowsScrolling:b];
}

bool WebChromeClient::scrollbarsVisible() const
{
    return [[[m_webView mainFrame] frameView] allowsScrolling];
}

void WebChromeClient::setMenubarVisible(bool)
{
    // The menubar is always visible in Mac OS X.
    return;
}

bool WebChromeClient::menubarVisible() const
{
    // The menubar is always visible in Mac OS X.
    return true;
}

void WebChromeClient::setResizable(bool b)
{
    [[m_webView _UIDelegateForwarder] webView:m_webView setResizable:b];
}

inline static NSString *stringForMessageSource(MessageSource source)
{
    switch (source) {
    case MessageSource::XML:
        return WebConsoleMessageXMLMessageSource;
    case MessageSource::JS:
        return WebConsoleMessageJSMessageSource;
    case MessageSource::Network:
        return WebConsoleMessageNetworkMessageSource;
    case MessageSource::ConsoleAPI:
        return WebConsoleMessageConsoleAPIMessageSource;
    case MessageSource::Storage:
        return WebConsoleMessageStorageMessageSource;
    case MessageSource::Rendering:
        return WebConsoleMessageRenderingMessageSource;
    case MessageSource::CSS:
        return WebConsoleMessageCSSMessageSource;
    case MessageSource::Accessibility:
        return WebConsoleMessageAccessibilityMessageSource;
    case MessageSource::Security:
        return WebConsoleMessageSecurityMessageSource;
    case MessageSource::ContentBlocker:
        return WebConsoleMessageContentBlockerMessageSource;
    case MessageSource::Media:
        return WebConsoleMessageMediaMessageSource;
    case MessageSource::MediaSource:
        return WebConsoleMessageMediaSourceMessageSource;
    case MessageSource::WebRTC:
        return WebConsoleMessageWebRTCMessageSource;
    case MessageSource::ITPDebug:
        return WebConsoleMessageITPDebugMessageSource;
    case MessageSource::PrivateClickMeasurement:
        return WebConsoleMessagePrivateClickMeasurementMessageSource;
    case MessageSource::PaymentRequest:
        return WebConsoleMessagePaymentRequestMessageSource;
    case MessageSource::Other:
        return WebConsoleMessageOtherMessageSource;
    }
    ASSERT_NOT_REACHED();
    return @"";
}

inline static NSString *stringForMessageLevel(MessageLevel level)
{
    switch (level) {
    case MessageLevel::Debug:
        return WebConsoleMessageDebugMessageLevel;
    case MessageLevel::Log:
        return WebConsoleMessageLogMessageLevel;
    case MessageLevel::Info:
        return WebConsoleMessageInfoMessageLevel;
    case MessageLevel::Warning:
        return WebConsoleMessageWarningMessageLevel;
    case MessageLevel::Error:
        return WebConsoleMessageErrorMessageLevel;
    }
    ASSERT_NOT_REACHED();
    return @"";
}

void WebChromeClient::addMessageToConsole(MessageSource source, MessageLevel level, const String& message, unsigned lineNumber, unsigned columnNumber, const String& sourceURL)
{
#if !PLATFORM(IOS_FAMILY)
    id delegate = [m_webView UIDelegate];
#else
    if (![m_webView _allowsMessaging])
        return;

    id delegate = [m_webView _UIKitDelegate];
    // No delegate means nothing to send this data to so bail.
    if (!delegate)
        return;
#endif

    BOOL respondsToNewSelector = NO;

    SEL selector = @selector(webView:addMessageToConsole:withSource:);
    if ([delegate respondsToSelector:selector])
        respondsToNewSelector = YES;
    else {
        // The old selector only takes JSMessageSource messages.
        if (source != MessageSource::JS)
            return;
        selector = @selector(webView:addMessageToConsole:);
        if (![delegate respondsToSelector:selector])
            return;
    }

    NSString *messageSource = stringForMessageSource(source);
    auto dictionary = @{
        @"message": message.createNSString().get(),
        @"lineNumber": @(lineNumber),
        @"columnNumber": @(columnNumber),
        @"sourceURL": sourceURL.createNSString().get(),
        @"MessageSource": messageSource,
        @"MessageLevel": stringForMessageLevel(level),
    };

#if PLATFORM(IOS_FAMILY)
    [[[m_webView _UIKitDelegateForwarder] asyncForwarder] webView:m_webView addMessageToConsole:dictionary withSource:messageSource];
    UNUSED_VARIABLE(respondsToNewSelector);
#else
    if (respondsToNewSelector)
        CallUIDelegate(m_webView, selector, dictionary, messageSource);
    else
        CallUIDelegate(m_webView, selector, dictionary);
#endif
}

bool WebChromeClient::canRunBeforeUnloadConfirmPanel()
{
    return [[m_webView UIDelegate] respondsToSelector:@selector(webView:runBeforeUnloadConfirmPanelWithMessage:initiatedByFrame:)];
}

bool WebChromeClient::runBeforeUnloadConfirmPanel(String&& message, LocalFrame& frame)
{
    return CallUIDelegateReturningBoolean(true, m_webView, @selector(webView:runBeforeUnloadConfirmPanelWithMessage:initiatedByFrame:), message.createNSString().get(), kit(&frame));
}

void WebChromeClient::closeWindow()
{
    // We need to remove the parent WebView from WebViewSets here, before it actually
    // closes, to make sure that JavaScript code that executes before it closes
    // can't find it. Otherwise, window.open will select a closed WebView instead of 
    // opening a new one <rdar://problem/3572585>.

    // We also need to stop the load to prevent further parsing or JavaScript execution
    // after the window has torn down <rdar://problem/4161660>.
  
    // FIXME: This code assumes that the UI delegate will respond to a webViewClose
    // message by actually closing the WebView. Safari guarantees this behavior, but other apps might not.
    // This approach is an inherent limitation of not making a close execute immediately
    // after a call to window.close.

    [m_webView setGroupName:nil];
    [m_webView stopLoading:nil];
    [m_webView _closeWindow];
}

void WebChromeClient::runJavaScriptAlert(LocalFrame& frame, const String& message)
{
    id delegate = [m_webView UIDelegate];
    SEL selector = @selector(webView:runJavaScriptAlertPanelWithMessage:initiatedByFrame:);
    if ([delegate respondsToSelector:selector]) {
        CallUIDelegate(m_webView, selector, message.createNSString().get(), kit(&frame));
        return;
    }

    // Call the old version of the delegate method if it is implemented.
    selector = @selector(webView:runJavaScriptAlertPanelWithMessage:);
    if ([delegate respondsToSelector:selector]) {
        CallUIDelegate(m_webView, selector, message.createNSString().get());
        return;
    }
}

bool WebChromeClient::runJavaScriptConfirm(LocalFrame& frame, const String& message)
{
    id delegate = [m_webView UIDelegate];
    SEL selector = @selector(webView:runJavaScriptConfirmPanelWithMessage:initiatedByFrame:);
    if ([delegate respondsToSelector:selector])
        return CallUIDelegateReturningBoolean(NO, m_webView, selector, message.createNSString().get(), kit(&frame));

    // Call the old version of the delegate method if it is implemented.
    selector = @selector(webView:runJavaScriptConfirmPanelWithMessage:);
    if ([delegate respondsToSelector:selector])
        return CallUIDelegateReturningBoolean(NO, m_webView, selector, message.createNSString().get());

    return NO;
}

bool WebChromeClient::runJavaScriptPrompt(LocalFrame& frame, const String& prompt, const String& defaultText, String& result)
{
    id delegate = [m_webView UIDelegate];
    SEL selector = @selector(webView:runJavaScriptTextInputPanelWithPrompt:defaultText:initiatedByFrame:);
    RetainPtr defaultString = defaultText.createNSString();
    if ([delegate respondsToSelector:selector]) {
        result = (NSString *)CallUIDelegate(m_webView, selector, prompt.createNSString().get(), defaultString.get(), kit(&frame));
        return !result.isNull();
    }

    // Call the old version of the delegate method if it is implemented.
    selector = @selector(webView:runJavaScriptTextInputPanelWithPrompt:defaultText:);
    if ([delegate respondsToSelector:selector]) {
        result = (NSString *)CallUIDelegate(m_webView, selector, prompt.createNSString().get(), defaultString.get());
        return !result.isNull();
    }

    result = [[WebDefaultUIDelegate sharedUIDelegate] webView:m_webView runJavaScriptTextInputPanelWithPrompt:prompt.createNSString().get() defaultText:defaultString.get() initiatedByFrame:kit(&frame)];
    return !result.isNull();
}

void WebChromeClient::invalidateRootView(const IntRect&)
{
}

void WebChromeClient::invalidateContentsAndRootView(const IntRect& rect)
{
}

void WebChromeClient::invalidateContentsForSlowScroll(const IntRect& rect)
{
    invalidateContentsAndRootView(rect);
}

void WebChromeClient::scroll(const IntSize&, const IntRect&, const IntRect&)
{
}

IntPoint WebChromeClient::screenToRootView(const IntPoint& p) const
{
    // FIXME: Implement this.
    return p;
}

IntPoint WebChromeClient::rootViewToScreen(const IntPoint& p) const
{
    // FIXME: Implement this.
    return p;
}

IntRect WebChromeClient::rootViewToScreen(const IntRect& r) const
{
    // FIXME: Implement this.
    return r;
}

IntPoint WebChromeClient::accessibilityScreenToRootView(const IntPoint& p) const
{
    return screenToRootView(p);
}

IntRect WebChromeClient::rootViewToAccessibilityScreen(const IntRect& r) const
{
    return rootViewToScreen(r);
}

void WebChromeClient::didFinishLoadingImageForElement(HTMLImageElement&)
{
}

PlatformPageClient WebChromeClient::platformPageClient() const
{
    return 0;
}

void WebChromeClient::contentsSizeChanged(LocalFrame&, const IntSize&) const
{
}

void WebChromeClient::scrollContainingScrollViewsToRevealRect(const IntRect& r) const
{
    // FIXME: This scrolling behavior should be under the control of the embedding client,
    // perhaps in a delegate method, rather than something WebKit does unconditionally.
    NSView *coordinateView = [[[m_webView mainFrame] frameView] documentView];
    NSRect rect = r;
    for (NSView *view = m_webView; view; view = [view superview]) {
        if ([view isKindOfClass:[NSClipView class]]) {
            NSClipView *clipView = (NSClipView *)view;
            NSView *documentView = [clipView documentView];
            [documentView scrollRectToVisible:[documentView convertRect:rect fromView:coordinateView]];
        }
    }
}

// End host window methods.

bool WebChromeClient::shouldUnavailablePluginMessageBeButton(PluginUnavailabilityReason pluginUnavailabilityReason) const
{
    if (pluginUnavailabilityReason == PluginUnavailabilityReason::PluginMissing)
        return [[m_webView UIDelegate] respondsToSelector:@selector(webView:didPressMissingPluginButton:)];

    return false;
}

void WebChromeClient::unavailablePluginButtonClicked(Element& element, PluginUnavailabilityReason pluginUnavailabilityReason) const
{
    ASSERT(element.hasTagName(objectTag) || element.hasTagName(embedTag) || element.hasTagName(appletTag));

    ASSERT(pluginUnavailabilityReason == PluginUnavailabilityReason::PluginMissing);
    CallUIDelegate(m_webView, @selector(webView:didPressMissingPluginButton:), kit(&element));
}

void WebChromeClient::mouseDidMoveOverElement(const HitTestResult& result, OptionSet<WebCore::PlatformEventModifier> modifiers, const String& toolTip, TextDirection)
{
    auto element = adoptNS([[WebElementDictionary alloc] initWithHitTestResult:result]);
    [m_webView _mouseDidMoveOverElement:element.get() modifierFlags:modifiers.toRaw()];
    setToolTip(toolTip);
}

void WebChromeClient::setToolTip(const String& toolTip)
{
    NSView<WebDocumentView> *documentView = [[[m_webView _selectedOrMainFrame] frameView] documentView];
    if ([documentView isKindOfClass:[WebHTMLView class]])
        [(WebHTMLView *)documentView _setToolTip:toolTip.createNSString().get()];
}

void WebChromeClient::print(LocalFrame& frame, const StringWithDirection&)
{
    WebFrame *webFrame = kit(&frame);
    if ([[m_webView UIDelegate] respondsToSelector:@selector(webView:printFrame:)])
        CallUIDelegate(m_webView, @selector(webView:printFrame:), webFrame);
    else
        CallUIDelegate(m_webView, @selector(webView:printFrameView:), [webFrame frameView]);
}

void WebChromeClient::exceededDatabaseQuota(LocalFrame& frame, const String& databaseName, DatabaseDetails)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS

    auto webOrigin = adoptNS([[WebSecurityOrigin alloc] _initWithWebCoreSecurityOrigin:&frame.document()->securityOrigin()]);
    CallUIDelegate(m_webView, @selector(webView:frame:exceededDatabaseQuotaForSecurityOrigin:database:), kit(&frame), webOrigin.get(), databaseName.createNSString().get());

    END_BLOCK_OBJC_EXCEPTIONS
}

RefPtr<ColorChooser> WebChromeClient::createColorChooser(ColorChooserClient& client, const Color& initialColor)
{
    // FIXME: Implement <input type='color'> for WK1 (Bug 119094).
    ASSERT_NOT_REACHED();
    return nullptr;
}

RefPtr<DataListSuggestionPicker> WebChromeClient::createDataListSuggestionPicker(DataListSuggestionsClient& client)
{
    ASSERT_NOT_REACHED();
    return nullptr;
}

RefPtr<DateTimeChooser> WebChromeClient::createDateTimeChooser(DateTimeChooserClient&)
{
    ASSERT_NOT_REACHED();
    return nullptr;
}

void WebChromeClient::setTextIndicator(const WebCore::TextIndicatorData& indicatorData) const
{
}

void WebChromeClient::updateTextIndicator(const WebCore::TextIndicatorData& indicatorData) const
{
}

#if ENABLE(POINTER_LOCK)
void WebChromeClient::requestPointerLock(CompletionHandler<void(WebCore::PointerLockRequestResult)>&& completionHandler)
{
#if PLATFORM(MAC)
    if (![m_webView page]) {
        completionHandler(WebCore::PointerLockRequestResult::Failure);
        return;
    }

    CGDisplayHideCursor(CGMainDisplayID());
    CGAssociateMouseAndMouseCursorPosition(false);
    [m_webView page]->pointerLockController().didAcquirePointerLock();
    
    completionHandler(WebCore::PointerLockRequestResult::Success);
#else
    completionHandler(WebCore::PointerLockRequestResult::Failure);
#endif
}

void WebChromeClient::requestPointerUnlock(CompletionHandler<void(bool)>&& completionHandler)
{
#if PLATFORM(MAC)
    CGAssociateMouseAndMouseCursorPosition(true);
    CGDisplayShowCursor(CGMainDisplayID());
    if ([m_webView page]) {
        [m_webView page]->pointerLockController().didLosePointerLock();
        completionHandler(true);
        return;
    }
#endif
    completionHandler(false);
}
#endif

void WebChromeClient::runOpenPanel(LocalFrame&, FileChooser& chooser)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    BOOL allowMultipleFiles = chooser.settings().allowsMultipleFiles;
    auto listener = adoptNS([[WebOpenPanelResultListener alloc] initWithChooser:chooser]);
    id delegate = [m_webView UIDelegate];
    if ([delegate respondsToSelector:@selector(webView:runOpenPanelForFileButtonWithResultListener:allowMultipleFiles:)])
        CallUIDelegate(m_webView, @selector(webView:runOpenPanelForFileButtonWithResultListener:allowMultipleFiles:), listener.get(), allowMultipleFiles);
    else if ([delegate respondsToSelector:@selector(webView:runOpenPanelForFileButtonWithResultListener:)])
        CallUIDelegate(m_webView, @selector(webView:runOpenPanelForFileButtonWithResultListener:), listener.get());
    else
        [listener cancel];
    END_BLOCK_OBJC_EXCEPTIONS
}

void WebChromeClient::showShareSheet(ShareDataWithParsedURL&&, CompletionHandler<void(bool)>&&)
{
}

void WebChromeClient::loadIconForFiles(const Vector<String>& filenames, FileIconLoader& iconLoader)
{
    iconLoader.iconLoaded(createIconForFiles(filenames));
}

RefPtr<Icon> WebChromeClient::createIconForFiles(const Vector<String>& filenames)
{
    return Icon::createIconForFiles(filenames);
}

#if !PLATFORM(IOS_FAMILY)

void WebChromeClient::setCursor(const WebCore::Cursor& cursor)
{
    // FIXME: Would be nice to share this code with WebKit2's PageClientImpl.

    if ([NSApp _cursorRectCursor])
        return;

    if (!m_webView)
        return;

    NSWindow *window = [m_webView window];
    if (!window)
        return;

    if ([window windowNumber] != [NSWindow windowNumberAtPoint:[NSEvent mouseLocation] belowWindowWithWindowNumber:0])
        return;

    NSCursor *platformCursor = cursor.platformCursor();
    if ([NSCursor currentCursor] == platformCursor)
        return;

    [platformCursor set];
}

void WebChromeClient::setCursorHiddenUntilMouseMoves(bool hiddenUntilMouseMoves)
{
    [NSCursor setHiddenUntilMouseMoves:hiddenUntilMouseMoves];
}

#endif

KeyboardUIMode WebChromeClient::keyboardUIMode()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    return [m_webView _keyboardUIMode];
    END_BLOCK_OBJC_EXCEPTIONS
    return KeyboardAccessDefault;
}

NSResponder *WebChromeClient::firstResponder()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    return [[m_webView _UIDelegateForwarder] webViewFirstResponder:m_webView];
    END_BLOCK_OBJC_EXCEPTIONS
    return nil;
}

void WebChromeClient::makeFirstResponder(NSResponder *responder)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [m_webView _pushPerformingProgrammaticFocus];
    [[m_webView _UIDelegateForwarder] webView:m_webView makeFirstResponder:responder];
    [m_webView _popPerformingProgrammaticFocus];
    END_BLOCK_OBJC_EXCEPTIONS
}

void WebChromeClient::enableSuddenTermination()
{
#if !PLATFORM(IOS_FAMILY)
    [[NSProcessInfo processInfo] enableSuddenTermination];
#endif
}

void WebChromeClient::disableSuddenTermination()
{
#if !PLATFORM(IOS_FAMILY)
    [[NSProcessInfo processInfo] disableSuddenTermination];
#endif
}

#if !PLATFORM(IOS_FAMILY)
void WebChromeClient::elementDidFocus(WebCore::Element& element, const WebCore::FocusOptions&)
{
    CallUIDelegate(m_webView, @selector(webView:formDidFocusNode:), kit(&element));
}

void WebChromeClient::elementDidBlur(WebCore::Element& element)
{
    CallUIDelegate(m_webView, @selector(webView:formDidBlurNode:), kit(&element));
}
#endif

bool WebChromeClient::selectItemWritingDirectionIsNatural()
{
    return false;
}

bool WebChromeClient::selectItemAlignmentFollowsMenuWritingDirection()
{
    return true;
}

RefPtr<WebCore::PopupMenu> WebChromeClient::createPopupMenu(WebCore::PopupMenuClient& client) const
{
#if !PLATFORM(IOS_FAMILY)
    return adoptRef(*new PopupMenuMac(&client));
#else
    return nullptr;
#endif
}

RefPtr<WebCore::SearchPopupMenu> WebChromeClient::createSearchPopupMenu(WebCore::PopupMenuClient& client) const
{
#if !PLATFORM(IOS_FAMILY)
    return adoptRef(*new SearchPopupMenuMac(&client));
#else
    return nullptr;
#endif
}

bool WebChromeClient::shouldPaintEntireContents() const
{
#if PLATFORM(IOS_FAMILY)
    return false;
#else
    NSView *documentView = [[[m_webView mainFrame] frameView] documentView];
    return [documentView layer];
#endif
}

void WebChromeClient::attachRootGraphicsLayer(LocalFrame& frame, GraphicsLayer* graphicsLayer)
{
#if !PLATFORM(MAC)
    UNUSED_PARAM(frame);
    UNUSED_PARAM(graphicsLayer);
#else
    BEGIN_BLOCK_OBJC_EXCEPTIONS

    NSView *documentView = [[kit(&frame) frameView] documentView];
    if (![documentView isKindOfClass:[WebHTMLView class]]) {
        // We should never be attaching when we don't have a WebHTMLView.
        ASSERT(!graphicsLayer);
        return;
    }

    WebHTMLView *webHTMLView = (WebHTMLView *)documentView;
    if (graphicsLayer)
        [webHTMLView attachRootLayer:graphicsLayer->platformLayer()];
    else
        [webHTMLView detachRootLayer];
    END_BLOCK_OBJC_EXCEPTIONS
#endif
}

void WebChromeClient::attachViewOverlayGraphicsLayer(GraphicsLayer*)
{
    // FIXME: If we want view-relative page overlays in Legacy WebKit, this would be the place to hook them up.
}

void WebChromeClient::setNeedsOneShotDrawingSynchronization()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [m_webView _setNeedsOneShotDrawingSynchronization:YES];
    END_BLOCK_OBJC_EXCEPTIONS
}

void WebChromeClient::triggerRenderingUpdate()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [m_webView _scheduleUpdateRendering];
    END_BLOCK_OBJC_EXCEPTIONS
}

#if ENABLE(VIDEO)

bool WebChromeClient::canEnterVideoFullscreen(HTMLVideoElement&, WebCore::HTMLMediaElementEnums::VideoFullscreenMode) const
{
#if !PLATFORM(IOS_FAMILY) || HAVE(AVKIT)
    return true;
#else
    return false;
#endif
}

bool WebChromeClient::supportsVideoFullscreen(HTMLMediaElementEnums::VideoFullscreenMode)
{
#if !PLATFORM(IOS_FAMILY) || HAVE(AVKIT)
    return true;
#else
    return false;
#endif
}

#if ENABLE(VIDEO_PRESENTATION_MODE)

void WebChromeClient::setMockVideoPresentationModeEnabled(bool enabled)
{
    m_mockVideoPresentationModeEnabled = enabled;
}

void WebChromeClient::enterVideoFullscreenForVideoElement(HTMLVideoElement& videoElement, HTMLMediaElementEnums::VideoFullscreenMode mode, bool standby)
{
    ASSERT_UNUSED(standby, !standby);
    ASSERT(mode != HTMLMediaElementEnums::VideoFullscreenModeNone);
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    if (m_mockVideoPresentationModeEnabled)
        videoElement.didBecomeFullscreenElement();
    else
        [m_webView _enterVideoFullscreenForVideoElement:&videoElement mode:mode];
    END_BLOCK_OBJC_EXCEPTIONS
}

void WebChromeClient::exitVideoFullscreenForVideoElement(WebCore::HTMLVideoElement& videoElement, CompletionHandler<void(bool)>&& completionHandler)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    if (m_mockVideoPresentationModeEnabled)
        videoElement.didStopBeingFullscreenElement();
    else
        [m_webView _exitVideoFullscreen];
    END_BLOCK_OBJC_EXCEPTIONS
    completionHandler(true);
}

void WebChromeClient::exitVideoFullscreenToModeWithoutAnimation(HTMLVideoElement& videoElement, HTMLMediaElementEnums::VideoFullscreenMode targetMode)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    if (m_mockVideoPresentationModeEnabled)
        videoElement.didStopBeingFullscreenElement();
    else
        [m_webView _exitVideoFullscreen];
    END_BLOCK_OBJC_EXCEPTIONS
}

#endif // ENABLE(VIDEO_PRESENTATION_MODE)

#endif // ENABLE(VIDEO)

#if ENABLE(VIDEO) && PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE)

void WebChromeClient::setUpPlaybackControlsManager(HTMLMediaElement& element)
{
    [m_webView _setUpPlaybackControlsManagerForMediaElement:element];
}

void WebChromeClient::clearPlaybackControlsManager()
{
    [m_webView _clearPlaybackControlsManager];
}

void WebChromeClient::mediaEngineChanged(WebCore::HTMLMediaElement&)
{
    [m_webView _playbackControlsMediaEngineChanged];
}

#endif

#if ENABLE(FULLSCREEN_API)

bool WebChromeClient::supportsFullScreenForElement(const Element& element, bool withKeyboard)
{
    SEL selector = @selector(webView:supportsFullScreenForElement:withKeyboard:);
    if ([[m_webView UIDelegate] respondsToSelector:selector])
        return CallUIDelegateReturningBoolean(false, m_webView, selector, kit(const_cast<WebCore::Element*>(&element)), withKeyboard);
#if !PLATFORM(IOS_FAMILY)
    return [m_webView _supportsFullScreenForElement:const_cast<WebCore::Element*>(&element) withKeyboard:withKeyboard];
#else
    return NO;
#endif
}

// FIXME: Remove this when rdar://144645925 is resolved.
void WebChromeClient::enterFullScreenForElement(Element& element, HTMLMediaElementEnums::VideoFullscreenMode, CompletionHandler<void(ExceptionOr<void>)>&& willEnterFullscreen, CompletionHandler<bool(bool)>&& didEnterFullscreen)
{
    SEL selector = @selector(webView:enterFullScreenForElement:listener:);
    if ([[m_webView UIDelegate] respondsToSelector:selector]) {
        auto listener = adoptNS([[WebKitFullScreenListener alloc] initWithElement:&element initialCompletionHandler:WTFMove(willEnterFullscreen) finalCompletionHandler:[didEnterFullscreen = WTFMove(didEnterFullscreen)] (bool result) mutable {
            didEnterFullscreen(result);
        }]);
        CallUIDelegate(m_webView, selector, kit(&element), listener.get());
    }
#if !PLATFORM(IOS_FAMILY)
    else
        [m_webView _enterFullScreenForElement:&element willEnterFullscreen:WTFMove(willEnterFullscreen) didEnterFullscreen:[didEnterFullscreen = WTFMove(didEnterFullscreen)] (bool result) mutable {
            didEnterFullscreen(result);
        }];
#endif
}

void WebChromeClient::exitFullScreenForElement(Element* element, CompletionHandler<void()>&& completionHandler)
{
    SEL selector = @selector(webView:exitFullScreenForElement:listener:);
    if ([[m_webView UIDelegate] respondsToSelector:selector]) {
        auto listener = adoptNS([[WebKitFullScreenListener alloc] initWithElement:element initialCompletionHandler:[completionHandler = WTFMove(completionHandler)] (auto) mutable {
            completionHandler();
        } finalCompletionHandler:nullptr]);
        CallUIDelegate(m_webView, selector, kit(element), listener.get());
    }
#if !PLATFORM(IOS_FAMILY)
    else
        [m_webView _exitFullScreenForElement:element completionHandler:WTFMove(completionHandler)];
#endif
}

#endif // ENABLE(FULLSCREEN_API)

#if ENABLE(SERVICE_CONTROLS)

void WebChromeClient::handleSelectionServiceClick(WebCore::FrameIdentifier, WebCore::FrameSelection& selection, const Vector<String>& telephoneNumbers, const WebCore::IntPoint& point)
{
    [m_webView _selectionServiceController].handleSelectionServiceClick(selection, telephoneNumbers, point);
}

bool WebChromeClient::hasRelevantSelectionServices(bool isTextOnly) const
{
    return [m_webView _selectionServiceController].hasRelevantSelectionServices(isTextOnly);
}

#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET) && !PLATFORM(IOS_FAMILY)

void WebChromeClient::addPlaybackTargetPickerClient(PlaybackTargetClientContextIdentifier contextId)
{
    [m_webView _addPlaybackTargetPickerClient:contextId];
}

void WebChromeClient::removePlaybackTargetPickerClient(PlaybackTargetClientContextIdentifier contextId)
{
    [m_webView _removePlaybackTargetPickerClient:contextId];
}

void WebChromeClient::showPlaybackTargetPicker(PlaybackTargetClientContextIdentifier contextId, const WebCore::IntPoint& location, bool hasVideo)
{
    [m_webView _showPlaybackTargetPicker:contextId location:location hasVideo:hasVideo];
}

void WebChromeClient::playbackTargetPickerClientStateDidChange(PlaybackTargetClientContextIdentifier contextId, MediaProducerMediaStateFlags state)
{
    [m_webView _playbackTargetPickerClientStateDidChange:contextId state:state];
}

void WebChromeClient::setMockMediaPlaybackTargetPickerEnabled(bool enabled)
{
    [m_webView _setMockMediaPlaybackTargetPickerEnabled:enabled];
}

void WebChromeClient::setMockMediaPlaybackTargetPickerState(const String& name, MediaPlaybackTargetContext::MockState state)
{
    [m_webView _setMockMediaPlaybackTargetPickerName:name.createNSString().get() state:state];
}

void WebChromeClient::mockMediaPlaybackTargetPickerDismissPopup()
{
    [m_webView _mockMediaPlaybackTargetPickerDismissPopup];
}
#endif

#if PLATFORM(MAC)
void WebChromeClient::changeUniversalAccessZoomFocus(const WebCore::IntRect& viewRect, const WebCore::IntRect& selectionRect)
{
    WebCore::changeUniversalAccessZoomFocus(viewRect, selectionRect);
}
#endif

#if HAVE(WEBGPU_IMPLEMENTATION)
RefPtr<WebCore::WebGPU::GPU> WebChromeClient::createGPUForWebGPU() const
{
    return nullptr;
}
#endif

RefPtr<WebCore::ShapeDetection::BarcodeDetector> WebChromeClient::createBarcodeDetector(const WebCore::ShapeDetection::BarcodeDetectorOptions& barcodeDetectorOptions) const
{
#if HAVE(SHAPE_DETECTION_API_IMPLEMENTATION)
    return WebCore::ShapeDetection::BarcodeDetectorImpl::create(barcodeDetectorOptions);
#else
    return nullptr;
#endif
}

void WebChromeClient::getBarcodeDetectorSupportedFormats(CompletionHandler<void(Vector<WebCore::ShapeDetection::BarcodeFormat>&&)>&& completionHandler) const
{
#if HAVE(SHAPE_DETECTION_API_IMPLEMENTATION)
    WebCore::ShapeDetection::BarcodeDetectorImpl::getSupportedFormats(WTFMove(completionHandler));
#else
    completionHandler({ });
#endif
}

RefPtr<WebCore::ShapeDetection::FaceDetector> WebChromeClient::createFaceDetector(const WebCore::ShapeDetection::FaceDetectorOptions& faceDetectorOptions) const
{
#if HAVE(SHAPE_DETECTION_API_IMPLEMENTATION)
    return WebCore::ShapeDetection::FaceDetectorImpl::create(faceDetectorOptions);
#else
    return nullptr;
#endif
}

RefPtr<WebCore::ShapeDetection::TextDetector> WebChromeClient::createTextDetector() const
{
#if HAVE(SHAPE_DETECTION_API_IMPLEMENTATION)
    return WebCore::ShapeDetection::TextDetectorImpl::create();
#else
    return nullptr;
#endif
}

void WebChromeClient::registerBlobPathForTesting(const String&, CompletionHandler<void()>&& completion)
{
    completion();
}

void WebChromeClient::requestCookieConsent(CompletionHandler<void(CookieConsentDecisionResult)>&& completion)
{
    completion(CookieConsentDecisionResult::NotSupported);
}
