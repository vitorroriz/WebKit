/*
 * Copyright (C) 2006 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#import "SearchPopupMenuMac.h"

#import "PopupMenuMac.h"
#import <wtf/FileSystem.h>
#import <wtf/text/AtomString.h>

static String defaultSearchFieldRecentSearchesStorageDirectory()
{
    NSString *appName = [[NSBundle mainBundle] bundleIdentifier];
    if (!appName)
        appName = [[NSProcessInfo processInfo] processName];

    return [[NSHomeDirectory() stringByAppendingPathComponent:@"Library/WebKit"] stringByAppendingPathComponent:appName];
}

SearchPopupMenuMac::SearchPopupMenuMac(WebCore::PopupMenuClient* client)
    : m_popup(adoptRef(new PopupMenuMac(client)))
    , m_directory(defaultSearchFieldRecentSearchesStorageDirectory())
{
    FileSystem::makeAllDirectories(m_directory);
}

SearchPopupMenuMac::~SearchPopupMenuMac()
{
}

WebCore::PopupMenu* SearchPopupMenuMac::popupMenu()
{
    return m_popup.get();
}

bool SearchPopupMenuMac::enabled()
{
    return true;
}

void SearchPopupMenuMac::saveRecentSearches(const AtomString& name, const Vector<WebCore::RecentSearch>& searchItems)
{
    WebCore::saveRecentSearchesToFile(name, searchItems, m_directory);
}

void SearchPopupMenuMac::loadRecentSearches(const AtomString& name, Vector<WebCore::RecentSearch>& searchItems)
{
    searchItems = WebCore::loadRecentSearchesFromFile(name, m_directory);
}
