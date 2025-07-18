/*
 * Copyright (C) 2014, 2016 Apple Inc. All rights reserved.
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

#include "config.h"
#include "IDBSerialization.h"

#include "IDBKeyData.h"
#include "IDBKeyPath.h"
#include "KeyedCoding.h"
#include <wtf/StdLibExtras.h>
#include <wtf/text/ParsingUtilities.h>

#if USE(GLIB)
#include <glib.h>
#include <wtf/glib/GRefPtr.h>
#endif

namespace WebCore {

enum class KeyPathType { Null, String, Array };

RefPtr<SharedBuffer> serializeIDBKeyPath(const std::optional<IDBKeyPath>& keyPath)
{
    auto encoder = KeyedEncoder::encoder();

    if (keyPath) {
        auto visitor = WTF::makeVisitor([&](const String& string) {
            encoder->encodeEnum("type"_s, KeyPathType::String);
            encoder->encodeString("string"_s, string);
        }, [&](const Vector<String>& vector) {
            encoder->encodeEnum("type"_s, KeyPathType::Array);
            encoder->encodeObjects("array"_s, vector, [](WebCore::KeyedEncoder& encoder, const String& string) {
                encoder.encodeString("string"_s, string);
            });
        });
        WTF::visit(visitor, keyPath.value());
    } else
        encoder->encodeEnum("type"_s, KeyPathType::Null);

    return encoder->finishEncoding();
}

bool deserializeIDBKeyPath(std::span<const uint8_t> data, std::optional<IDBKeyPath>& result)
{
    if (data.empty())
        return false;

    auto decoder = KeyedDecoder::decoder(data);

    KeyPathType type;
    bool succeeded = decoder->decodeEnum("type"_s, type, [](KeyPathType value) {
        return value == KeyPathType::Null || value == KeyPathType::String || value == KeyPathType::Array;
    });
    if (!succeeded)
        return false;

    switch (type) {
    case KeyPathType::Null:
        break;
    case KeyPathType::String: {
        String string;
        if (!decoder->decodeString("string"_s, string))
            return false;
        result = IDBKeyPath(WTFMove(string));
        break;
    }
    case KeyPathType::Array: {
        Vector<String> vector;
        succeeded = decoder->decodeObjects("array"_s, vector, [](KeyedDecoder& decoder, String& result) {
            return decoder.decodeString("string"_s, result);
        });
        if (!succeeded)
            return false;
        result = IDBKeyPath(WTFMove(vector));
        break;
    }
    }
    return true;
}

static bool isLegacySerializedIDBKeyData(std::span<const uint8_t> data)
{
#if USE(CF)
    // This is the magic character that begins serialized PropertyLists, and tells us whether
    // the key we're looking at is an old-style key.
    static const uint8_t legacySerializedKeyVersion = 'b';
    if (data[0] == legacySerializedKeyVersion)
        return true;
#elif USE(GLIB)
    // KeyedEncoderGLib uses a GVariant dictionary, so check if the given data is a valid GVariant dictionary.
    GRefPtr<GBytes> bytes = adoptGRef(g_bytes_new(data.data(), data.size()));
    GRefPtr<GVariant> variant = g_variant_new_from_bytes(G_VARIANT_TYPE("a{sv}"), bytes.get(), FALSE);
    return g_variant_is_normal_form(variant.get());
#else
    UNUSED_PARAM(data);
#endif
    return false;
}


/*
The IDBKeyData serialization format is as follows:
[1 byte version header][Key Buffer]

The Key Buffer serialization format is as follows:
[1 byte key type][Type specific data]

Type specific serialization formats are as follows for each of the types:
Min:
[0 bytes]

Number:
[8 bytes representing a double encoded in little endian]

Date:
[8 bytes representing a double encoded in little endian]

String:
[4 bytes representing string "length" in little endian]["length" number of 2-byte pairs representing ECMAScript 16-bit code units]

Binary:
[8 bytes representing the "size" of the binary blob]["size" bytes]

Array:
[8 bytes representing the "length" of the key array]["length" individual Key Buffer entries]

Max:
[0 bytes]
*/

static const uint8_t SIDBKeyVersion = 0x00;
enum class SIDBKeyType : uint8_t {
    Min = 0x00,
    Number = 0x20,
    Date = 0x40,
    String = 0x60,
    Binary = 0x80,
    Array = 0xA0,
    Max = 0xFF,
};

static SIDBKeyType serializedTypeForKeyType(IndexedDB::KeyType type)
{
    switch (type) {
    case IndexedDB::KeyType::Min:
        return SIDBKeyType::Min;
    case IndexedDB::KeyType::Number:
        return SIDBKeyType::Number;
    case IndexedDB::KeyType::Date:
        return SIDBKeyType::Date;
    case IndexedDB::KeyType::String:
        return SIDBKeyType::String;
    case IndexedDB::KeyType::Binary:
        return SIDBKeyType::Binary;
    case IndexedDB::KeyType::Array:
        return SIDBKeyType::Array;
    case IndexedDB::KeyType::Max:
        return SIDBKeyType::Max;
    case IndexedDB::KeyType::Invalid:
        RELEASE_ASSERT_NOT_REACHED();
    };

    RELEASE_ASSERT_NOT_REACHED();
}

#if CPU(BIG_ENDIAN) || CPU(MIDDLE_ENDIAN) || CPU(NEEDS_ALIGNED_ACCESS)
template <typename T> static void writeLittleEndian(Vector<uint8_t>& buffer, T value)
{
    for (unsigned i = 0; i < sizeof(T); i++) {
        buffer.append(value & 0xFF);
        value >>= 8;
    }
}

template <typename T> static bool readLittleEndian(std::span<const uint8_t>& data, T& value)
{
    if (data.size() < sizeof(value))
        return false;

    value = 0;
    for (size_t i = 0; i < sizeof(T); i++)
        value += ((T)data[i]) << (i * 8);
    skip(data, sizeof(T));
    return true;
}
#else
template <typename T> static void writeLittleEndian(Vector<uint8_t>& buffer, T value)
{
    buffer.append(asByteSpan(value));
}

template <typename T> static bool readLittleEndian(std::span<const uint8_t>& data, T& value)
{
    if (data.size() < sizeof(value))
        return false;

    value = consumeAndReinterpretCastTo<const T>(data);
    return true;
}
#endif

static void writeDouble(Vector<uint8_t>& data, double d)
{
    writeLittleEndian(data, *reinterpret_cast<uint64_t*>(&d));
}

static bool readDouble(std::span<const uint8_t>& data, double& d)
{
    return readLittleEndian(data, *reinterpret_cast<uint64_t*>(&d));
}

static void encodeKey(Vector<uint8_t>& data, const IDBKeyData& key)
{
    SIDBKeyType type = serializedTypeForKeyType(key.type());
    data.append(static_cast<uint8_t>(type));

    switch (type) {
    case SIDBKeyType::Number:
        writeDouble(data, key.number());
        break;
    case SIDBKeyType::Date:
        writeDouble(data, key.date());
        break;
    case SIDBKeyType::String: {
        auto string = key.string();
        uint32_t length = string.length();
        writeLittleEndian(data, length);

        for (size_t i = 0; i < length; ++i)
            writeLittleEndian(data, string[i]);

        break;
    }
    case SIDBKeyType::Binary: {
        auto& buffer = key.binary();
        uint64_t size = buffer.size();
        writeLittleEndian(data, size);

        auto* bufferData = buffer.data();
        ASSERT(bufferData || !size);
        if (bufferData)
            data.append(bufferData->span());

        break;
    }
    case SIDBKeyType::Array: {
        auto& array = key.array();
        uint64_t size = array.size();
        writeLittleEndian(data, size);
        for (auto& key : array)
            encodeKey(data, key);

        break;
    }
    case SIDBKeyType::Min:
    case SIDBKeyType::Max:
        break;
    }
}

RefPtr<SharedBuffer> serializeIDBKeyData(const IDBKeyData& key)
{
    Vector<uint8_t> data;
    data.append(SIDBKeyVersion);

    encodeKey(data, key);
    return SharedBuffer::create(WTFMove(data));
}

static WARN_UNUSED_RETURN bool decodeKey(std::span<const uint8_t>& data, IDBKeyData& result)
{
    if (data.empty())
        return false;

    SIDBKeyType type = static_cast<SIDBKeyType>(consume(data));
    switch (type) {
    case SIDBKeyType::Min:
        result = IDBKeyData::minimum();
        return true;
    case SIDBKeyType::Max:
        result = IDBKeyData::maximum();
        return true;
    case SIDBKeyType::Number: {
        double d;
        if (!readDouble(data, d))
            return false;

        result.setNumberValue(d);
        return true;
    }
    case SIDBKeyType::Date: {
        double d;
        if (!readDouble(data, d))
            return false;

        result.setDateValue(d);
        return true;
    }
    case SIDBKeyType::String: {
        uint32_t length;
        if (!readLittleEndian(data, length))
            return false;

        if (data.size() < length * 2)
            return false;

        Vector<char16_t> buffer;
        buffer.reserveInitialCapacity(length);
        for (size_t i = 0; i < length; i++) {
            uint16_t ch;
            if (!readLittleEndian(data, ch))
                return false;
            buffer.append(ch);
        }

        result.setStringValue(String::adopt(WTFMove(buffer)));

        return true;
    }
    case SIDBKeyType::Binary: {
        uint64_t size64;
        if (!readLittleEndian(data, size64))
            return false;

        if (data.size() < size64)
            return false;

        if (size64 > std::numeric_limits<size_t>::max())
            return false;

        size_t size = static_cast<size_t>(size64);
        Vector<uint8_t> dataVector(data);
        skip(data, size);

        result.setBinaryValue(ThreadSafeDataBuffer::create(WTFMove(dataVector)));
        return true;
    }
    case SIDBKeyType::Array: {
        uint64_t size64;
        if (!readLittleEndian(data, size64))
            return false;

        if (size64 > std::numeric_limits<size_t>::max())
            return false;

        size_t size = static_cast<size_t>(size64);
        Vector<IDBKeyData> array;
        array.reserveInitialCapacity(size);

        for (size_t i = 0; i < size; ++i) {
            IDBKeyData keyData;
            if (!decodeKey(data, keyData))
                return false;

            ASSERT(keyData.isValid());
            array.append(WTFMove(keyData));
        }

        result.setArrayValue(array);

        return true;
    }
    default:
        LOG_ERROR("decodeKey encountered unexpected type: %i", (int)type);
        return false;
    }
}

bool deserializeIDBKeyData(std::span<const uint8_t> data, IDBKeyData& result)
{
    if (data.empty())
        return false;

    if (isLegacySerializedIDBKeyData(data)) {
        auto decoder = KeyedDecoder::decoder(data);
        return IDBKeyData::decode(*decoder, result);
    }

    // Verify this is a SerializedIDBKey version we understand.
    if (consume(data) != SIDBKeyVersion)
        return false;

    if (decodeKey(data, result)) {
        // Even if we successfully decoded a key, the deserialize is only successful
        // if we actually consumed all input data.
        return data.empty();
    }

    return false;
}

} // namespace WebCore
