/*
 * Copyright (C) 2008, 2010 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of its
 *     contributors may be used to endorse or promote products derived
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

#pragma once

#include <limits>
#include <unicode/utypes.h>
#include <wtf/Assertions.h>
#include <wtf/CheckedArithmetic.h>
#include <wtf/DebugHeap.h>
#include <wtf/MallocSpan.h>
#include <wtf/Noncopyable.h>

namespace WTF {

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(StringBuffer);

template <typename CharType>
class StringBuffer {
    WTF_MAKE_NONCOPYABLE(StringBuffer);
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(StringBuffer);
public:
    explicit StringBuffer(unsigned length)
        : m_length(length)
        , m_data(m_length ? static_cast<CharType*>(StringBufferMalloc::malloc(Checked<size_t>(m_length) * sizeof(CharType))) : nullptr)
    {
    }

    ~StringBuffer()
    {
        StringBufferMalloc::free(m_data);
    }

    void shrink(unsigned newLength)
    {
        ASSERT(newLength <= m_length);
        m_length = newLength;
    }

    void resize(unsigned newLength)
    {
        if (newLength > m_length)
            m_data = static_cast<CharType*>(StringBufferMalloc::realloc(m_data, Checked<size_t>(newLength) * sizeof(CharType)));
        m_length = newLength;
    }

    unsigned length() const { return m_length; }
    CharType* characters() LIFETIME_BOUND { return m_data; }
    std::span<CharType> span() LIFETIME_BOUND { return unsafeMakeSpan(m_data, m_length); }

    CharType& operator[](unsigned i) LIFETIME_BOUND { RELEASE_ASSERT(i < m_length); return m_data[i]; }

    MallocSpan<CharType, StringBufferMalloc> release()
    {
        return adoptMallocSpan<CharType, StringBufferMalloc>(unsafeMakeSpan(std::exchange(m_data, nullptr), std::exchange(m_length, 0)));
    }

private:
    unsigned m_length;
    CharType* m_data;
};

} // namespace WTF

using WTF::StringBuffer;
