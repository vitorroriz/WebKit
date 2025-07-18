/*
   Copyright (C) 2000-2001 Dawit Alemayehu <adawit@kde.org>
   Copyright (C) 2006 Alexey Proskuryakov <ap@webkit.org>
   Copyright (C) 2007-2024 Apple Inc. All rights reserved.
   Copyright (C) 2010 Patrick Gansterer <paroga@paroga.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License (LGPL)
   version 2 as published by the Free Software Foundation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

   This code is based on the java implementation in HTTPClient
   package by Ronald Tschalär Copyright (C) 1996-1999.
*/

#include "config.h"
#include <wtf/text/Base64.h>

#include <limits.h>
#include <wtf/SIMDUTF.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringCommon.h>

namespace WTF {

constexpr const char nonAlphabet = -1;

constexpr unsigned encodeMapSize = 64;
constexpr unsigned decodeMapSize = 128;

static constexpr std::array<char, encodeMapSize> base64EncMap {
    0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
    0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 0x50,
    0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
    0x59, 0x5A, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66,
    0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E,
    0x6F, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76,
    0x77, 0x78, 0x79, 0x7A, 0x30, 0x31, 0x32, 0x33,
    0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x2B, 0x2F
};

static constexpr std::array<char, decodeMapSize> base64DecMap {
    nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet,
    nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet,
    nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet,
    nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet,
    nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet,
    nonAlphabet, nonAlphabet, nonAlphabet, 0x3E, nonAlphabet, nonAlphabet, nonAlphabet, 0x3F,
    0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B,
    0x3C, 0x3D, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet,
    nonAlphabet, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
    0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E,
    0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
    0x17, 0x18, 0x19, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet,
    nonAlphabet, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20,
    0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
    0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30,
    0x31, 0x32, 0x33, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet
};

static constexpr std::array<char, encodeMapSize> base64URLEncMap {
    0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
    0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 0x50,
    0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
    0x59, 0x5A, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66,
    0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E,
    0x6F, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76,
    0x77, 0x78, 0x79, 0x7A, 0x30, 0x31, 0x32, 0x33,
    0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x2D, 0x5F
};

static constexpr std::array<char, decodeMapSize> base64URLDecMap {
    nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet,
    nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet,
    nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet,
    nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet,
    nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet,
    nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, 0x3E, nonAlphabet, nonAlphabet,
    0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B,
    0x3C, 0x3D, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet,
    nonAlphabet, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
    0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E,
    0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
    0x17, 0x18, 0x19, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, 0x3F,
    nonAlphabet, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20,
    0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
    0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30,
    0x31, 0x32, 0x33, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet
};

static inline simdutf::base64_options toSIMDUTFEncodeOptions(OptionSet<Base64EncodeOption> options)
{
    if (options.contains(Base64EncodeOption::URL)) {
        if (options.contains(Base64EncodeOption::OmitPadding))
            return simdutf::base64_url;
        return simdutf::base64_url_with_padding;
    }
    if (options.contains(Base64EncodeOption::OmitPadding))
        return simdutf::base64_default_no_padding;
    return simdutf::base64_default;
}

template<typename CharacterType> static void base64EncodeInternal(std::span<const uint8_t> inputDataBuffer, std::span<CharacterType> destinationDataBuffer, OptionSet<Base64EncodeOption> options)
{
    ASSERT(destinationDataBuffer.size() > 0);
    ASSERT(calculateBase64EncodedSize(inputDataBuffer.size(), options) == destinationDataBuffer.size());

    if constexpr (sizeof(CharacterType) == 1) {
        size_t bytesWritten = simdutf::binary_to_base64(std::bit_cast<const char*>(inputDataBuffer.data()), inputDataBuffer.size(), std::bit_cast<char*>(destinationDataBuffer.data()), toSIMDUTFEncodeOptions(options));
        ASSERT_UNUSED(bytesWritten, bytesWritten == destinationDataBuffer.size());
        return;
    }

    auto encodeMap = options.contains(Base64EncodeOption::URL) ? base64URLEncMap : base64EncMap;

    unsigned sidx = 0;
    unsigned didx = 0;

    if (inputDataBuffer.size() > 1) {
        while (sidx < inputDataBuffer.size() - 2) {
            destinationDataBuffer[didx++] = encodeMap[ (inputDataBuffer[sidx    ] >> 2) & 077];
            destinationDataBuffer[didx++] = encodeMap[((inputDataBuffer[sidx + 1] >> 4) & 017) | ((inputDataBuffer[sidx    ] << 4) & 077)];
            destinationDataBuffer[didx++] = encodeMap[((inputDataBuffer[sidx + 2] >> 6) & 003) | ((inputDataBuffer[sidx + 1] << 2) & 077)];
            destinationDataBuffer[didx++] = encodeMap[  inputDataBuffer[sidx + 2]       & 077];
            sidx += 3;
        }
    }

    if (sidx < inputDataBuffer.size()) {
        destinationDataBuffer[didx++] = encodeMap[(inputDataBuffer[sidx] >> 2) & 077];
        if (sidx < inputDataBuffer.size() - 1) {
            destinationDataBuffer[didx++] = encodeMap[((inputDataBuffer[sidx + 1] >> 4) & 017) | ((inputDataBuffer[sidx] << 4) & 077)];
            destinationDataBuffer[didx++] = encodeMap[ (inputDataBuffer[sidx + 1] << 2) & 077];
        } else
            destinationDataBuffer[didx++] = encodeMap[ (inputDataBuffer[sidx    ] << 4) & 077];
    }

    if (!options.contains(Base64EncodeOption::OmitPadding)) {
        while (didx < destinationDataBuffer.size())
            destinationDataBuffer[didx++] = '=';
    }

    ASSERT(didx == destinationDataBuffer.size());
}

template<typename CharacterType> static void base64EncodeInternal(std::span<const std::byte> input, std::span<CharacterType> destinationDataBuffer, OptionSet<Base64EncodeOption> options)
{
    base64EncodeInternal(asBytes(input), destinationDataBuffer, options);
}

static Vector<uint8_t> base64EncodeInternal(std::span<const std::byte> input, OptionSet<Base64EncodeOption> options)
{
    auto destinationLength = calculateBase64EncodedSize(input.size(), options);
    if (!destinationLength)
        return { };

    Vector<uint8_t> destinationVector(destinationLength);
    base64EncodeInternal(input, std::span(destinationVector), options);
    return destinationVector;
}

void base64Encode(std::span<const std::byte> input, std::span<char16_t> destination, OptionSet<Base64EncodeOption> options)
{
    if (!destination.size())
        return;
    base64EncodeInternal(input, destination, options);
}

void base64Encode(std::span<const std::byte> input, std::span<LChar> destination, OptionSet<Base64EncodeOption> options)
{
    if (!destination.size())
        return;
    base64EncodeInternal(input, destination, options);
}

Vector<uint8_t> base64EncodeToVector(std::span<const std::byte> input, OptionSet<Base64EncodeOption> options)
{
    return base64EncodeInternal(input, options);
}

String base64EncodeToString(std::span<const std::byte> input, OptionSet<Base64EncodeOption> options)
{
    return makeString(base64Encoded(input, options));
}

String base64EncodeToStringReturnNullIfOverflow(std::span<const std::byte> input, OptionSet<Base64EncodeOption> options)
{
    return tryMakeString(base64Encoded(input, options));
}

unsigned calculateBase64EncodedSize(unsigned inputLength, OptionSet<Base64EncodeOption> options)
{
    if (inputLength > maximumBase64EncoderInputBufferSize)
        return 0;

    return simdutf::base64_length_from_binary(inputLength, toSIMDUTFEncodeOptions(options));
}

template<typename T, typename Malloc = VectorBufferMalloc>
static std::optional<Vector<uint8_t, 0, CrashOnOverflow, 16, Malloc>> base64DecodeInternal(std::span<const T> inputDataBuffer, OptionSet<Base64DecodeOption> options)
{
    if (!inputDataBuffer.size())
        return Vector<uint8_t, 0, CrashOnOverflow, 16, Malloc> { };

    auto decodeMap = options.contains(Base64DecodeOption::URL) ? base64URLDecMap : base64DecMap;
    auto validatePadding = options.contains(Base64DecodeOption::ValidatePadding);
    auto ignoreWhitespace = options.contains(Base64DecodeOption::IgnoreWhitespace);

    Vector<uint8_t, 0, CrashOnOverflow, 16, Malloc> destination(inputDataBuffer.size());

    unsigned equalsSignCount = 0;
    unsigned destinationLength = 0;
    for (unsigned idx = 0; idx < inputDataBuffer.size(); ++idx) {
        unsigned ch = inputDataBuffer[idx];
        if (ch == '=') {
            ++equalsSignCount;
            // There should never be more than 2 padding characters.
            if (validatePadding && equalsSignCount > 2) {
                return std::nullopt;
            }
        } else {
            char decodedCharacter = ch < decodeMapSize ? decodeMap[ch] : nonAlphabet;
            if (decodedCharacter != nonAlphabet) {
                if (equalsSignCount)
                    return std::nullopt;
                destination[destinationLength++] = decodedCharacter;
            } else if (!ignoreWhitespace || !isASCIIWhitespace(ch))
                return std::nullopt;
        }
    }

    // Make sure we shrink back the Vector before returning. destinationLength may be shorter than expected
    // in case of error or in case of ignored spaces.
    if (destinationLength < destination.size())
        destination.shrink(destinationLength);

    if (!destinationLength) {
        if (equalsSignCount)
            return std::nullopt;
        return Vector<uint8_t, 0, CrashOnOverflow, 16, Malloc> { };
    }

    // The should be no padding if length is a multiple of 4.
    // We use (destinationLength + equalsSignCount) instead of length because we don't want to account for ignored characters (i.e. spaces).
    if (validatePadding && equalsSignCount && (destinationLength + equalsSignCount) % 4)
        return std::nullopt;

    // Valid data is (n * 4 + [0,2,3]) characters long.
    if ((destinationLength % 4) == 1)
        return std::nullopt;
    
    // 4-byte to 3-byte conversion
    destinationLength -= (destinationLength + 3) / 4;
    if (!destinationLength)
        return std::nullopt;

    unsigned sidx = 0;
    unsigned didx = 0;
    if (destinationLength > 1) {
        while (didx < destinationLength - 2) {
            destination[didx    ] = (((destination[sidx    ] << 2) & 255) | ((destination[sidx + 1] >> 4) & 003));
            destination[didx + 1] = (((destination[sidx + 1] << 4) & 255) | ((destination[sidx + 2] >> 2) & 017));
            destination[didx + 2] = (((destination[sidx + 2] << 6) & 255) |  (destination[sidx + 3]       & 077));
            sidx += 4;
            didx += 3;
        }
    }

    if (didx < destinationLength)
        destination[didx] = (((destination[sidx    ] << 2) & 255) | ((destination[sidx + 1] >> 4) & 003));

    if (++didx < destinationLength)
        destination[didx] = (((destination[sidx + 1] << 4) & 255) | ((destination[sidx + 2] >> 2) & 017));

    if (destinationLength < destination.size())
        destination.shrink(destinationLength);
    return destination;
}

std::optional<Vector<uint8_t>> base64Decode(std::span<const std::byte> input, OptionSet<Base64DecodeOption> options)
{
    if (input.size() > std::numeric_limits<unsigned>::max())
        return std::nullopt;
    return base64DecodeInternal(asBytes(input), options);
}

std::optional<Vector<uint8_t>> base64Decode(StringView input, OptionSet<Base64DecodeOption> options)
{
    if (input.is8Bit())
        return base64DecodeInternal(input.span8(), options);
    return base64DecodeInternal(input.span16(), options);
}

String base64DecodeToString(StringView input, OptionSet<Base64DecodeOption> options)
{
    auto toString = [&] (auto optionalBuffer) {
        if (!optionalBuffer)
            return nullString();
        return String::adopt(WTFMove(*optionalBuffer));
    };

    if (input.is8Bit())
        return toString(base64DecodeInternal<LChar, StringImplMalloc>(input.span8(), options));
    return toString(base64DecodeInternal<char16_t, StringImplMalloc>(input.span16(), options));
}

static inline simdutf::base64_options toSIMDUTFDecodeOptions(Alphabet alphabet)
{
    switch (alphabet) {
    case Alphabet::Base64:
        return simdutf::base64_default;
    case Alphabet::Base64URL:
        return simdutf::base64_url;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

static inline simdutf::last_chunk_handling_options toSIMDUTFLastChunkHandling(LastChunkHandling lastChunkHandling)
{
    switch (lastChunkHandling) {
    case LastChunkHandling::Loose:
        return simdutf::last_chunk_handling_options::loose;
    case LastChunkHandling::Strict:
        return simdutf::last_chunk_handling_options::strict;
    case LastChunkHandling::StopBeforePartial:
        return simdutf::last_chunk_handling_options::stop_before_partial;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

template<typename CharacterType>
static inline size_t fixSIMDUTFStopBeforePartialReadLength(std::span<const CharacterType> span, size_t readLengthFromSIMDUTF)
{
    // Work around simdutf bug for stop-before-partial read length.
    // FIXME: Remove once fixed in simdutf.

    // First go backwards to find the last non-whitespace character read.
    size_t idx = readLengthFromSIMDUTF;
    while (idx && isASCIIWhitespace(span[idx - 1]))
        idx--;

    // This is the correct read length if the input doesn't end in trailing whitespace.
    size_t fixedReadLength = idx;

    // If the input ends in trailing whitespace, the correct read length is the input size.
    while (idx < span.size() && isASCIIWhitespace(span[idx]))
        idx++;
    if (idx == span.size())
        fixedReadLength = span.size();

    return fixedReadLength;
}

template<typename CharacterType>
static std::tuple<FromBase64ShouldThrowError, size_t, size_t> fromBase64Impl(std::span<const CharacterType> span, std::span<uint8_t> output, Alphabet alphabet, LastChunkHandling lastChunkHandling)
{
    using UTFType = std::conditional_t<sizeof(CharacterType) == 1, char, char16_t>;

    size_t outputLength = output.size();
    constexpr bool decodeUpToBadChar = true;
    auto result = simdutf::base64_to_binary_safe(std::bit_cast<const UTFType*>(span.data()), span.size(), std::bit_cast<char*>(output.data()), outputLength, toSIMDUTFDecodeOptions(alphabet), toSIMDUTFLastChunkHandling(lastChunkHandling), decodeUpToBadChar);
    switch (result.error) {
    case simdutf::error_code::OUTPUT_BUFFER_TOO_SMALL:
        return { FromBase64ShouldThrowError::No, result.count, outputLength };

    case simdutf::error_code::SUCCESS: {
        size_t read;
        if (lastChunkHandling == LastChunkHandling::StopBeforePartial) [[unlikely]]
            read = fixSIMDUTFStopBeforePartialReadLength(span, result.count);
        else
            read = span.size();
        return { FromBase64ShouldThrowError::No, read, outputLength };
    }

    default:
        return { FromBase64ShouldThrowError::Yes, result.count, outputLength };
    }
}

std::tuple<FromBase64ShouldThrowError, size_t, size_t> fromBase64(StringView string, std::span<uint8_t> output, Alphabet alphabet, LastChunkHandling lastChunkHandling)
{
    if (string.is8Bit())
        return fromBase64Impl(string.span8(), output, alphabet, lastChunkHandling);
    return fromBase64Impl(string.span16(), output, alphabet, lastChunkHandling);
}

size_t maxLengthFromBase64(StringView string)
{
    size_t length = string.length();
    if (string.is8Bit())
        return simdutf::maximal_binary_length_from_base64(std::bit_cast<const char*>(string.span8().data()), length);
    return simdutf::maximal_binary_length_from_base64(std::bit_cast<const char16_t*>(string.span16().data()), length);
}

} // namespace WTF
