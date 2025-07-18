/*
 * Copyright (C) 2013, 2015 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#pragma once

#include <ranges>
#include <wtf/CommaPrinter.h>
#include <wtf/PrintStream.h>
#include <wtf/StringPrintStream.h>

namespace WTF {

template<typename T>
class ListDump {
public:
    ListDump(const T& list, ASCIILiteral comma)
        : m_list(list)
        , m_comma(comma)
    {
    }
    
    void dump(PrintStream& out) const
    {
        for (auto& item : m_list)
            out.print(m_comma, item);
    }

private:
    const T& m_list;
    CommaPrinter m_comma;
};

template<typename T>
class PointerListDump {
public:
    PointerListDump(const T& list, ASCIILiteral comma)
        : m_list(list)
        , m_comma(comma)
    {
    }
    
    void dump(PrintStream& out) const
    {
        for (auto* item : m_list)
            out.print(m_comma, pointerDump(item));
    }

private:
    const T& m_list;
    CommaPrinter m_comma;
};

template<typename T>
class MapDump {
public:
    MapDump(const T& map, ASCIILiteral arrow, ASCIILiteral comma)
        : m_map(map)
        , m_arrow(arrow)
        , m_comma(comma)
    {
    }
    
    void dump(PrintStream& out) const
    {
        for (auto iter = m_map.begin(); iter != m_map.end(); ++iter)
            out.print(m_comma, iter->key, m_arrow, iter->value);
    }
    
private:
    const T& m_map;
    ASCIILiteral m_arrow;
    CommaPrinter m_comma;
};

template<typename T>
ListDump<T> listDump(const T& list, ASCIILiteral comma = ", "_s)
{
    return ListDump<T>(list, comma);
}

template<typename T>
PointerListDump<T> pointerListDump(const T& list, ASCIILiteral comma = ", "_s)
{
    return PointerListDump<T>(list, comma);
}

template<typename T, typename Comparator>
CString sortedListDump(const T& list, const Comparator& comparator, ASCIILiteral comma = ", "_s)
{
    Vector<typename T::ValueType> myList;
    myList.appendRange(list.begin(), list.end());
    std::ranges::sort(myList, comparator);
    StringPrintStream out;
    CommaPrinter commaPrinter(comma);
    for (unsigned i = 0; i < myList.size(); ++i)
        out.print(commaPrinter, myList[i]);
    return out.toCString();
}

template<typename T>
CString sortedListDump(const T& list, ASCIILiteral comma = ", "_s)
{
    return sortedListDump(list, std::less<>(), comma);
}

template<typename T>
MapDump<T> mapDump(const T& map, ASCIILiteral arrow = "=>"_s, ASCIILiteral comma = ", "_s)
{
    return MapDump<T>(map, arrow, comma);
}

template<typename T, typename Comparator>
CString sortedMapDump(const T& map, const Comparator& comparator, ASCIILiteral arrow = "=>"_s, ASCIILiteral comma = ", "_s)
{
    Vector<typename T::KeyType> keys;
    for (auto iter = map.begin(); iter != map.end(); ++iter)
        keys.append(iter->key);
    std::ranges::sort(keys, comparator);
    StringPrintStream out;
    CommaPrinter commaPrinter(comma);
    for (unsigned i = 0; i < keys.size(); ++i)
        out.print(commaPrinter, keys[i], arrow, map.get(keys[i]));
    return out.toCString();
}

template<typename T, typename U>
class ListDumpInContext {
public:
    ListDumpInContext(const T& list, U* context, ASCIILiteral comma)
        : m_list(list)
        , m_context(context)
        , m_comma(comma)
    {
    }
    
    void dump(PrintStream& out) const
    {
        for (auto& item : m_list)
            out.print(m_comma, inContext(item, m_context));
    }

private:
    const T& m_list;
    U* m_context;
    CommaPrinter m_comma;
};

template<typename T, typename U>
ListDumpInContext<T, U> listDumpInContext(
    const T& list, U* context, ASCIILiteral comma = ", "_s)
{
    return ListDumpInContext<T, U>(list, context, comma);
}

} // namespace WTF

using WTF::listDump;
using WTF::listDumpInContext;
using WTF::mapDump;
using WTF::pointerListDump;
using WTF::sortedListDump;
using WTF::sortedMapDump;
