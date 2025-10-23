/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#import "NSFontPanelTesting.h"

#if PLATFORM(MAC)

#import "AppKitSPI.h"
#import <objc/runtime.h>

@interface NSBox (NSFontEffectsBox)

- (void)_addAttribute:(NSString *)attribute value:(id)value;
- (void)_removeAttribute:(NSString *)attribute;
- (void)_clearAttributes;

@end

@implementation NSBox (NSFontEffectsBox)

- (void)_addAttribute:(NSString *)attribute value:(id)value
{
    void* result = nullptr;
    object_getInstanceVariable(self, "_attributesToAdd", &result);
    if (result) {
        [static_cast<NSMutableDictionary *>(result) setObject:value forKey:attribute];
        return;
    }

    RetainPtr dictionary = adoptNS([NSMutableDictionary new]);
    [dictionary setObject:value forKey:attribute];
    object_setInstanceVariableWithStrongDefault(self, "_attributesToAdd", dictionary.get());
}

- (void)_removeAttribute:(NSString *)attribute
{
    void* result = nullptr;
    object_getInstanceVariable(self, "_attributesToRemove", &result);
    if (result) {
        [static_cast<NSMutableArray *>(result) addObject:attribute];
        return;
    }

    RetainPtr array = [NSMutableArray arrayWithObject:attribute];
    object_setInstanceVariableWithStrongDefault(self, "_attributesToRemove", array.get());
}

- (void)_clearAttributes
{
    object_setInstanceVariableWithStrongDefault(self, "_attributesToAdd", nil);
    object_setInstanceVariableWithStrongDefault(self, "_attributesToRemove", nil);
}

@end

static NSView *findSubviewOfClass(NSView *view, Class targetClass)
{
    if ([view isKindOfClass:targetClass])
        return view;

    for (NSView *subview in view.subviews) {
        if (NSView *targetView = findSubviewOfClass(subview, targetClass))
            return targetView;
    }
    return nil;
}

@implementation NSFontPanel (TestWebKitAPI)

- (NSFontEffectsBox *)fontEffectsBox
{
    void* result = nullptr;
    object_getInstanceVariable(self, "_fontEffectsBox", &result);
    return static_cast<NSFontEffectsBox *>(result);
}

- (NSPopUpButton *)underlineToolbarButton
{
    return (NSPopUpButton *)[self _toolbarItemWithIdentifier:@"NSFontPanelUnderlineToolbarItem"].view;
}

- (NSPopUpButton *)strikeThroughToolbarButton
{
    return (NSPopUpButton *)[self _toolbarItemWithIdentifier:@"NSFontPanelStrikethroughToolbarItem"].view;
}

- (NSColorWell *)foregroundColorToolbarColorWell
{
    return (NSColorWell *)[self _toolbarItemWithIdentifier:@"NSFontPanelTextColorToolbarItem"].view;
}

- (void)setUnderlineStyle:(NSUnderlineStyle)style
{
    [[self fontEffectsBox] _addAttribute:NSUnderlineStyleAttributeName value:@(style)];
}

- (void)setStrikethroughStyle:(NSUnderlineStyle)style
{
    [[self fontEffectsBox] _addAttribute:NSStrikethroughStyleAttributeName value:@(style)];
}

- (void)setTextShadow:(NSShadow *)shadow
{
    RetainPtr fontEffectsBox = [self fontEffectsBox];
    if (shadow)
        [fontEffectsBox _addAttribute:NSShadowAttributeName value:shadow];
    else
        [fontEffectsBox _removeAttribute:NSShadowAttributeName];
}

- (void)commitAttributeChanges
{
    [self _didChangeAttributes];
    [self.fontEffectsBox _clearAttributes];
}

- (void)_didChangeAttributes
{
    NSFontManager *fontManager = NSFontManager.sharedFontManager;
    if (self == [fontManager fontPanel:NO])
        [fontManager.target changeAttributes:self.fontEffectsBox];
}

- (NSSlider *)shadowBlurSlider
{
    return (NSSlider *)findSubviewOfClass([self _toolbarItemWithIdentifier:@"NSFontPanelShadowBlurToolbarItem"].view, NSSlider.class);
}

- (NSSlider *)shadowOpacitySlider
{
    return (NSSlider *)findSubviewOfClass([self _toolbarItemWithIdentifier:@"NSFontPanelShadowOpacityToolbarItem"].view, NSSlider.class);
}

- (NSSlider *)shadowLengthSlider
{
    return (NSSlider *)findSubviewOfClass([self _toolbarItemWithIdentifier:@"NSFontPanelShadowOffsetToolbarItem"].view, NSSlider.class);
}

- (NSButton *)shadowToggleButton
{
    return (NSButton *)[self _toolbarItemWithIdentifier:@"NSFontPanelShadowToggleToolbarItem"].view;
}

- (BOOL)hasShadow
{
    return self.shadowToggleButton.state == NSControlStateValueOn;
}

- (double)shadowLength
{
    return self.shadowLengthSlider.doubleValue;
}

- (void)setShadowLength:(double)shadowLength
{
    self.shadowLengthSlider.doubleValue = shadowLength;
}

- (double)shadowOpacity
{
    return self.shadowOpacitySlider.doubleValue;
}

- (void)setShadowOpacity:(double)shadowOpacity
{
    self.shadowOpacitySlider.doubleValue = shadowOpacity;
    if (self.shadowToggleButton.state == NSControlStateValueOn)
        [self _didChangeAttributes];
}

- (double)shadowBlur
{
    return self.shadowBlurSlider.doubleValue;
}

- (void)setShadowBlur:(double)shadowBlur
{
    self.shadowBlurSlider.doubleValue = shadowBlur;
    if (self.shadowToggleButton.state == NSControlStateValueOn)
        [self _didChangeAttributes];
}

- (NSToolbarItem *)_toolbarItemWithIdentifier:(NSString *)itemIdentifier
{
    for (NSToolbarItem *item in self.toolbar.items) {
        if ([item.itemIdentifier isEqualToString:itemIdentifier])
            return item;
    }
    return nil;
}

- (BOOL)hasUnderline
{
    NSMenuItem *singleUnderlineMenuItem = [self.underlineToolbarButton itemAtIndex:2];
    return singleUnderlineMenuItem.state == NSControlStateValueOn;
}

- (BOOL)hasStrikeThrough
{
    NSMenuItem *singleStrikeThroughMenuItem = [self.strikeThroughToolbarButton itemAtIndex:2];
    return singleStrikeThroughMenuItem.state == NSControlStateValueOn;
}

- (NSColor *)foregroundColor
{
    return self.foregroundColorToolbarColorWell.color;
}

@end

#endif // PLATFORM(MAC)
