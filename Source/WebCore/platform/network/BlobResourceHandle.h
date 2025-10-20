/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2025 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/BlobData.h>
#include <WebCore/FileStreamClient.h>
#include <WebCore/HTTPParsers.h>
#include <WebCore/ResourceHandle.h>
#include <wtf/Variant.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class AsyncFileStream;
class BlobData;
class FileStream;
class ResourceHandleClient;
class ResourceRequest;
class BlobDataItem;

class BlobResourceHandleBase : public FileStreamClient {
public:
    enum class Error {
        NoError = 0,
        NotFoundError = 1,
        SecurityError = 2,
        RangeError = 3,
        NotReadableError = 4,
        MethodNotAllowed = 5
    };

protected:
    WEBCORE_EXPORT BlobResourceHandleBase(bool async = true, RefPtr<BlobData>&& = nullptr);
    WEBCORE_EXPORT virtual ~BlobResourceHandleBase();

    WEBCORE_EXPORT void start();
    WEBCORE_EXPORT void readAsync();
    WEBCORE_EXPORT void closeFileIfOpen();
    bool isFileOpen() const { return m_isFileOpen; }
    void setIsFileOpen(bool isOpen) { m_isFileOpen = isOpen; }
    bool async() const { return std::holds_alternative<std::unique_ptr<AsyncFileStream>>(m_stream); }
    uint64_t totalSize() { return m_totalSize; }
    uint64_t totalRemainingSize() const { return m_totalRemainingSize; }
    uint64_t currentItemReadSize() const { return m_currentItemReadSize; }
    void setCurrentItemReadSize(uint64_t size) { m_currentItemReadSize = size; }
    void decrementTotalRemainingSizeBy(uint64_t value) { m_totalRemainingSize -= value; }
    uint64_t readItemCount() const { return m_readItemCount; }
    void incrementReadItemCount() { ++m_readItemCount; }
    uint64_t lengthOfItemBeingRead() const { return m_itemLengthList[m_readItemCount]; }
    WEBCORE_EXPORT void clearAsyncStream();
    WEBCORE_EXPORT BlobData* blobData() const;
    FileStream* syncStream() const;
    AsyncFileStream* asyncStream() const;
    Vector<uint8_t>& buffer() { return m_buffer; }
    const Vector<uint8_t>& buffer() const { return m_buffer; }

private:
    void getSizeForNext();
    std::optional<Error> seek();
    std::optional<Error> adjustAndValidateRangeBounds();
    bool consumeData(std::span<const uint8_t>);
    bool readDataAsync(const BlobDataItem&);
    void readFileAsync(const BlobDataItem&);
    void dispatchDidReceiveResponse();
    void doStart();

    virtual void didReceiveResponse(ResourceResponse&&) = 0;
    virtual void didFail(Error) = 0;
    virtual bool didReceiveData(std::span<const uint8_t>) = 0;
    virtual void didFinish() = 0;
    virtual bool erroredOrAborted() const = 0;
    virtual bool shouldAbortDispatchDidReceiveResponse() { return false; }
    virtual const ResourceRequest& firstRequest() const = 0;
    virtual void clearStream() { }

    // FileStreamClient methods.
    WEBCORE_EXPORT void didOpen(bool) final;
    WEBCORE_EXPORT void didGetSize(long long) final;
    WEBCORE_EXPORT void didRead(int) final;

    RefPtr<BlobData> m_blobData;
    // For Async or Sync loading.
    Variant<std::unique_ptr<AsyncFileStream>, std::unique_ptr<FileStream>> m_stream;
    std::optional<HTTPRange> m_range;
    Vector<uint8_t> m_buffer;
    Vector<uint64_t> m_itemLengthList;
    uint64_t m_totalSize { 0 };
    uint64_t m_totalRemainingSize { 0 };
    uint64_t m_currentItemReadSize { 0 };
    unsigned m_readItemCount { 0 };
    unsigned m_sizeItemCount { 0 };
    bool m_isFileOpen { false };
    bool m_isRangeRequest { false };
};

class BlobResourceHandle final : public BlobResourceHandleBase, public ResourceHandle  {
public:
    static Ref<BlobResourceHandle> createAsync(BlobData*, const ResourceRequest&, ResourceHandleClient*);

    static void loadResourceSynchronously(BlobData*, const ResourceRequest&, ResourceError&, ResourceResponse&, Vector<uint8_t>& data);

    using BlobResourceHandleBase::start;
    int readSync(std::span<uint8_t>);

    bool aborted() const { return m_aborted; }

    bool isBlobResourceHandle() const final { return true; }

    // FileStreamClient.
    void ref() const final { ResourceHandle::ref(); }
    void deref() const final { ResourceHandle::deref(); }

private:
    BlobResourceHandle(BlobData*, const ResourceRequest&, ResourceHandleClient*, bool async);
    virtual ~BlobResourceHandle();

    // ResourceHandle methods.
    void cancel() final;

    // BlobResourceHandleBase.
    bool didReceiveData(std::span<const uint8_t>) final;
    void didReceiveResponse(ResourceResponse&&) final;
    void didFail(Error) final;
    bool erroredOrAborted() const final { return m_aborted || m_errorCode != Error::NoError; }
    bool shouldAbortDispatchDidReceiveResponse() final;
    void didFinish() final;
    const ResourceRequest& firstRequest() const final { return ResourceHandle::firstRequest(); }

    void doStart();

    int readDataSync(const BlobDataItem&, std::span<uint8_t>);
    int readFileSync(const BlobDataItem&, std::span<uint8_t>);

    Error m_errorCode { Error::NoError };
    bool m_aborted { false };
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::BlobResourceHandle)
    static bool isType(const WebCore::ResourceHandle& handle) { return handle.isBlobResourceHandle(); }
SPECIALIZE_TYPE_TRAITS_END()
