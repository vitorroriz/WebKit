/*
 *  Copyright (C) 2010-2023 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#pragma once

#include <array>
#include <wtf/Atomics.h>
#include <wtf/HashFunctions.h>
#include <wtf/IterationStatus.h>
#include <wtf/MathExtras.h>
#include <wtf/PrintStream.h>
#include <wtf/StdIntExtras.h>
#include <wtf/StdLibExtras.h>
#include <string.h>
#include <type_traits>

namespace WTF {

template<size_t size>
using BitSetWordType = std::conditional_t<(size <= 32 && sizeof(UCPURegister) > sizeof(uint32_t)), uint32_t, UCPURegister>;

template<size_t bitSetSize, typename PassedWordType = BitSetWordType<bitSetSize>>
class BitSet final {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(BitSet);
    
public:
    using WordType = PassedWordType;

    static_assert(sizeof(WordType) <= sizeof(UCPURegister), "WordType must not be bigger than the CPU atomic word size");
    constexpr BitSet() = default;

    static constexpr size_t size()
    {
        return bitSetSize;
    }

    constexpr bool get(size_t, Dependency = Dependency()) const;
    constexpr void set(size_t);
    constexpr void set(size_t, bool);
    constexpr bool testAndSet(size_t); // Returns the previous bit value.
    constexpr bool testAndClear(size_t); // Returns the previous bit value.
    constexpr bool concurrentTestAndSet(size_t, Dependency = Dependency()); // Returns the previous bit value.
    constexpr bool concurrentTestAndClear(size_t, Dependency = Dependency()); // Returns the previous bit value.
    constexpr size_t nextPossiblyUnset(size_t) const;
    constexpr void clear(size_t);
    constexpr void clearAll();
    constexpr void setAll();
    constexpr void invert();
    constexpr int64_t findRunOfZeros(size_t runLength) const;
    constexpr size_t count(size_t start = 0) const;
    constexpr bool isEmpty() const;
    constexpr bool isFull() const;
    
    constexpr void merge(const BitSet&);
    constexpr void filter(const BitSet&);
    constexpr void exclude(const BitSet&);

    constexpr void concurrentFilter(const BitSet&);
    
    constexpr bool subsumes(const BitSet&) const;
    
    // If the lambda returns an IterationStatus, we use it. The lambda can also return
    // void, in which case, we'll iterate every set bit.
    template<typename Func>
    constexpr ALWAYS_INLINE void forEachSetBit(const Func&) const;

    template<typename Func>
    constexpr ALWAYS_INLINE void forEachSetBit(size_t startIndex, const Func&) const;

    constexpr size_t findBit(size_t startIndex, bool value) const;

    class iterator {
        WTF_DEPRECATED_MAKE_FAST_ALLOCATED(iterator);
    public:
        constexpr iterator()
            : m_bitSet(nullptr)
            , m_index(0)
        {
        }

        constexpr iterator(const BitSet& bitSet, size_t index)
            : m_bitSet(&bitSet)
            , m_index(index)
        {
        }
        
        constexpr size_t operator*() const { return m_index; }
        
        iterator& operator++()
        {
            m_index = m_bitSet->findBit(m_index + 1, true);
            return *this;
        }
        
        constexpr bool operator==(const iterator& other) const
        {
            return m_index == other.m_index;
        }
        
    private:
        const BitSet* m_bitSet;
        size_t m_index;
    };
    
    // Use this to iterate over set bits.
    constexpr iterator begin() const LIFETIME_BOUND { return iterator(*this, findBit(0, true)); }
    constexpr iterator end() const LIFETIME_BOUND { return iterator(*this, bitSetSize); }
    
    constexpr void mergeAndClear(BitSet&);
    constexpr void setAndClear(BitSet&);

    void setEachNthBit(size_t n, size_t start = 0, size_t end = bitSetSize);

    constexpr bool operator==(const BitSet&) const;

    constexpr void operator|=(const BitSet&);
    constexpr void operator&=(const BitSet&);
    constexpr void operator^=(const BitSet&);

    constexpr unsigned hash() const;

    void dump(PrintStream& out) const;

    std::span<WordType> storage() { return bits; }
    std::span<const WordType> storage() const { return bits; }

    constexpr size_t storageLengthInBytes() { return sizeof(bits); }

    std::span<uint8_t> storageBytes() { return unsafeMakeSpan(reinterpret_cast<uint8_t*>(bits.data()), storageLengthInBytes()); }
    std::span<const uint8_t> storageBytes() const { return unsafeMakeSpan(reinterpret_cast<const uint8_t*>(bits.data()), storageLengthInBytes()); }

private:
    void cleanseLastWord();

    static constexpr unsigned wordSize = sizeof(WordType) * 8;
    static constexpr unsigned words = (bitSetSize + wordSize - 1) / wordSize;

    // the literal '1' is of type signed int.  We want to use an unsigned
    // version of the correct size when doing the calculations because if
    // WordType is larger than int, '1 << 31' will first be sign extended
    // and then casted to unsigned, meaning that set(31) when WordType is
    // a 64 bit unsigned int would give 0xffff8000
    static constexpr WordType one = 1;

    std::array<WordType, words> bits { };
};

template<size_t bitSetSize, typename WordType>
inline constexpr bool BitSet<bitSetSize, WordType>::get(size_t n, Dependency dependency) const
{
    return !!(dependency.consume(this)->bits[n / wordSize] & (one << (n % wordSize)));
}

template<size_t bitSetSize, typename WordType>
ALWAYS_INLINE constexpr void BitSet<bitSetSize, WordType>::set(size_t n)
{
    bits[n / wordSize] |= (one << (n % wordSize));
}

template<size_t bitSetSize, typename WordType>
ALWAYS_INLINE constexpr void BitSet<bitSetSize, WordType>::set(size_t n, bool value)
{
    if (value)
        set(n);
    else
        clear(n);
}

template<size_t bitSetSize, typename WordType>
inline constexpr bool BitSet<bitSetSize, WordType>::testAndSet(size_t n)
{
    WordType mask = one << (n % wordSize);
    size_t index = n / wordSize;
    bool previousValue = bits[index] & mask;
    bits[index] |= mask;
    return previousValue;
}

template<size_t bitSetSize, typename WordType>
inline constexpr bool BitSet<bitSetSize, WordType>::testAndClear(size_t n)
{
    WordType mask = one << (n % wordSize);
    size_t index = n / wordSize;
    bool previousValue = bits[index] & mask;
    bits[index] &= ~mask;
    return previousValue;
}

template<size_t bitSetSize, typename WordType>
ALWAYS_INLINE constexpr bool BitSet<bitSetSize, WordType>::concurrentTestAndSet(size_t n, Dependency dependency)
{
    WordType mask = one << (n % wordSize);
    size_t index = n / wordSize;
WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
    WordType* data = dependency.consume(bits.data()) + index;
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
    // transactionRelaxed() returns true if the bit was changed. If the bit was changed,
    // then the previous bit must have been false since we're trying to set it. Hence,
    // the result of transactionRelaxed() is the inverse of our expected result.
    return !std::bit_cast<Atomic<WordType>*>(data)->transactionRelaxed(
        [&] (WordType& value) -> bool {
            if (value & mask)
                return false;
            
            value |= mask;
            return true;
        });
}

template<size_t bitSetSize, typename WordType>
ALWAYS_INLINE constexpr bool BitSet<bitSetSize, WordType>::concurrentTestAndClear(size_t n, Dependency dependency)
{
    WordType mask = one << (n % wordSize);
    size_t index = n / wordSize;
WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
    WordType* data = dependency.consume(bits.data()) + index;
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
    // transactionRelaxed() returns true if the bit was changed. If the bit was changed,
    // then the previous bit must have been true since we're trying to clear it. Hence,
    // the result of transactionRelaxed() matches our expected result.
    return std::bit_cast<Atomic<WordType>*>(data)->transactionRelaxed(
        [&] (WordType& value) -> bool {
            if (!(value & mask))
                return false;
            
            value &= ~mask;
            return true;
        });
}

template<size_t bitSetSize, typename WordType>
inline constexpr void BitSet<bitSetSize, WordType>::clear(size_t n)
{
    bits[n / wordSize] &= ~(one << (n % wordSize));
}

template<size_t bitSetSize, typename WordType>
inline constexpr void BitSet<bitSetSize, WordType>::clearAll()
{
    zeroSpan(std::span { bits });
}

template<size_t bitSetSize, typename WordType>
inline void BitSet<bitSetSize, WordType>::cleanseLastWord()
{
    if constexpr (!!(bitSetSize % wordSize)) {
        constexpr size_t remainingBits = bitSetSize % wordSize;
        constexpr WordType mask = (static_cast<WordType>(1) << remainingBits) - 1;
        bits[words - 1] &= mask;
    }
}

template<size_t bitSetSize, typename WordType>
inline constexpr void BitSet<bitSetSize, WordType>::setAll()
{
    memsetSpan(std::span { bits }, 0xFF);
    cleanseLastWord();
}

template<size_t bitSetSize, typename WordType>
inline constexpr void BitSet<bitSetSize, WordType>::invert()
{
    for (size_t i = 0; i < words; ++i)
        bits[i] = ~bits[i];
    cleanseLastWord();
}

template<size_t bitSetSize, typename WordType>
inline constexpr size_t BitSet<bitSetSize, WordType>::nextPossiblyUnset(size_t start) const
{
    if (!~bits[start / wordSize])
        return ((start / wordSize) + 1) * wordSize;
    return start + 1;
}

template<size_t bitSetSize, typename WordType>
inline constexpr int64_t BitSet<bitSetSize, WordType>::findRunOfZeros(size_t runLength) const
{
    if (!runLength) 
        runLength = 1; 
     
    if (runLength > bitSetSize)
        return -1;

    for (size_t i = 0; i <= (bitSetSize - runLength) ; i++) {
        bool found = true; 
        for (size_t j = i; j <= (i + runLength - 1) ; j++) { 
            if (get(j)) {
                found = false; 
                break;
            }
        }
        if (found)  
            return i; 
    }
    return -1;
}

template<size_t bitSetSize, typename WordType>
inline constexpr size_t BitSet<bitSetSize, WordType>::count(size_t start) const
{
    size_t result = 0;
    for ( ; (start % wordSize); ++start) {
        if (get(start))
            ++result;
    }
    for (size_t i = start / wordSize; i < words; ++i)
        result += WTF::bitCount(bits[i]);
    return result;
}

template<size_t bitSetSize, typename WordType>
inline constexpr bool BitSet<bitSetSize, WordType>::isEmpty() const
{
    for (size_t i = 0; i < words; ++i)
        if (bits[i])
            return false;
    return true;
}

template<size_t bitSetSize, typename WordType>
inline constexpr bool BitSet<bitSetSize, WordType>::isFull() const
{
    for (size_t i = 0; i < words; ++i)
        if (~bits[i]) {
            if constexpr (!!(bitSetSize % wordSize)) {
                if (i == words - 1) {
                    constexpr size_t remainingBits = bitSetSize % wordSize;
                    constexpr WordType mask = (static_cast<WordType>(1) << remainingBits) - 1;
                    if ((bits[i] & mask) == mask)
                        return true;
                }
            }
            return false;
        }
    return true;
}

template<size_t bitSetSize, typename WordType>
inline constexpr void BitSet<bitSetSize, WordType>::merge(const BitSet& other)
{
    for (size_t i = 0; i < words; ++i)
        bits[i] |= other.bits[i];
}

template<size_t bitSetSize, typename WordType>
inline constexpr void BitSet<bitSetSize, WordType>::filter(const BitSet& other)
{
    for (size_t i = 0; i < words; ++i)
        bits[i] &= other.bits[i];
}

template<size_t bitSetSize, typename WordType>
inline constexpr void BitSet<bitSetSize, WordType>::exclude(const BitSet& other)
{
    for (size_t i = 0; i < words; ++i)
        bits[i] &= ~other.bits[i];
}

template<size_t bitSetSize, typename WordType>
inline constexpr void BitSet<bitSetSize, WordType>::concurrentFilter(const BitSet& other)
{
    for (size_t i = 0; i < words; ++i) {
        for (;;) {
            WordType otherBits = other.bits[i];
            if (!otherBits) {
                bits[i] = 0;
                break;
            }
            WordType oldBits = bits[i];
            WordType filteredBits = oldBits & otherBits;
            if (oldBits == filteredBits)
                break;
            if (atomicCompareExchangeWeakRelaxed(&bits[i], oldBits, filteredBits))
                break;
        }
    }
}

template<size_t bitSetSize, typename WordType>
inline constexpr bool BitSet<bitSetSize, WordType>::subsumes(const BitSet& other) const
{
    for (size_t i = 0; i < words; ++i) {
        WordType myBits = bits[i];
        WordType otherBits = other.bits[i];
        if ((myBits | otherBits) != myBits)
            return false;
    }
    return true;
}

template<size_t bitSetSize, typename WordType>
template<typename Func>
ALWAYS_INLINE constexpr void BitSet<bitSetSize, WordType>::forEachSetBit(const Func& func) const
{
    WTF::forEachSetBit(std::span { bits }, func);
}

template<size_t bitSetSize, typename WordType>
template<typename Func>
ALWAYS_INLINE constexpr void BitSet<bitSetSize, WordType>::forEachSetBit(size_t startIndex, const Func& func) const
{
    WTF::forEachSetBit(std::span { bits }, startIndex, func);
}

template<size_t bitSetSize, typename WordType>
inline constexpr size_t BitSet<bitSetSize, WordType>::findBit(size_t startIndex, bool value) const
{
    WordType skipValue = -(static_cast<WordType>(value) ^ 1);
    size_t wordIndex = startIndex / wordSize;
    size_t startIndexInWord = startIndex - wordIndex * wordSize;
    
    while (wordIndex < words) {
        WordType word = bits[wordIndex];
        if (word != skipValue) {
            size_t index = startIndexInWord;
            if (findBitInWord(word, index, wordSize, value))
                return wordIndex * wordSize + index;
        }
        
        wordIndex++;
        startIndexInWord = 0;
    }
    
    return bitSetSize;
}

template<size_t bitSetSize, typename WordType>
inline constexpr void BitSet<bitSetSize, WordType>::mergeAndClear(BitSet& other)
{
    for (size_t i = 0; i < words; ++i) {
        bits[i] |= other.bits[i];
        other.bits[i] = 0;
    }
}

template<size_t bitSetSize, typename WordType>
inline constexpr void BitSet<bitSetSize, WordType>::setAndClear(BitSet& other)
{
    for (size_t i = 0; i < words; ++i) {
        bits[i] = other.bits[i];
        other.bits[i] = 0;
    }
}

template<size_t bitSetSize, typename WordType>
inline void BitSet<bitSetSize, WordType>::setEachNthBit(size_t n, size_t start, size_t end)
{
    ASSERT(start <= end);
    ASSERT(end <= bitSetSize);

    size_t wordIndex = start / wordSize;
    size_t endWordIndex = end / wordSize;
    size_t index = start - wordIndex * wordSize;
    while (wordIndex < endWordIndex) {
        while (index < wordSize) {
            bits[wordIndex] |= (one << index);
            index += n;
        }
        index -= wordSize;
        wordIndex++;
    }

    size_t endIndex = end - endWordIndex * wordSize;
    while (index < endIndex) {
        bits[wordIndex] |= (one << index);
        index += n;
    }

    cleanseLastWord();
}

template<size_t bitSetSize, typename WordType>
inline constexpr bool BitSet<bitSetSize, WordType>::operator==(const BitSet& other) const
{
    for (size_t i = 0; i < words; ++i) {
        if (bits[i] != other.bits[i])
            return false;
    }
    return true;
}

template<size_t bitSetSize, typename WordType>
inline constexpr void BitSet<bitSetSize, WordType>::operator|=(const BitSet& other)
{
    for (size_t i = 0; i < words; ++i)
        bits[i] |= other.bits[i];
}

template<size_t bitSetSize, typename WordType>
inline constexpr void BitSet<bitSetSize, WordType>::operator&=(const BitSet& other)
{
    for (size_t i = 0; i < words; ++i)
        bits[i] &= other.bits[i];
}

template<size_t bitSetSize, typename WordType>
inline constexpr void BitSet<bitSetSize, WordType>::operator^=(const BitSet& other)
{
    for (size_t i = 0; i < words; ++i)
        bits[i] ^= other.bits[i];
}

template<size_t bitSetSize, typename WordType>
inline constexpr unsigned BitSet<bitSetSize, WordType>::hash() const
{
    unsigned result = 0;
    for (size_t i = 0; i < words; ++i)
        result ^= IntHash<WordType>::hash(bits[i]);
    return result;
}

template<size_t bitSetSize, typename WordType>
inline void BitSet<bitSetSize, WordType>::dump(PrintStream& out) const
{
    for (size_t i = 0; i < size(); ++i)
        out.print(get(i) ? "1" : "-");
}

} // namespace WTF

// We can't do "using WTF::BitSet;" here because there is a function in the macOS SDK named BitSet() already.
