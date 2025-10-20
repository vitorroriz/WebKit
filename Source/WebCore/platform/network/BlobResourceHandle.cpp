/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2014-2025 Apple Inc. All rights reserved.
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

#include "config.h"

#include "BlobResourceHandle.h"

#include "AsyncFileStream.h"
#include "FileStream.h"
#include "HTTPHeaderNames.h"
#include "ParsedContentRange.h"
#include "ResourceError.h"
#include "ResourceHandleClient.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "SecurityOrigin.h"
#include "SharedBuffer.h"
#include <wtf/CompletionHandler.h>
#include <wtf/FileSystem.h>
#include <wtf/MainThread.h>
#include <wtf/Ref.h>
#include <wtf/StdLibExtras.h>
#include <wtf/URL.h>

namespace WebCore {

static const unsigned bufferSize = 512 * 1024;

static const int httpOK = 200;
static const int httpPartialContent = 206;
static constexpr auto httpOKText = "OK"_s;
static constexpr auto httpPartialContentText = "Partial Content"_s;

static constexpr auto webKitBlobResourceDomain = "WebKitBlobResource"_s;

///////////////////////////////////////////////////////////////////////////////
// BlobResourceSynchronousLoader

namespace {

class BlobResourceSynchronousLoader : public ResourceHandleClient {
public:
    BlobResourceSynchronousLoader(ResourceError&, ResourceResponse&, Vector<uint8_t>&);

    void didReceiveResponseAsync(ResourceHandle*, ResourceResponse&&, CompletionHandler<void()>&&) final;
    void didFail(ResourceHandle*, const ResourceError&) final;
    void willSendRequestAsync(ResourceHandle*, ResourceRequest&&, ResourceResponse&&, CompletionHandler<void(ResourceRequest&&)>&&) final;
#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
    void canAuthenticateAgainstProtectionSpaceAsync(ResourceHandle*, const ProtectionSpace&, CompletionHandler<void(bool)>&&) final;
#endif

private:
    ResourceError& m_error;
    ResourceResponse& m_response;
    Vector<uint8_t>& m_data;
};

BlobResourceSynchronousLoader::BlobResourceSynchronousLoader(ResourceError& error, ResourceResponse& response, Vector<uint8_t>& data)
    : m_error(error)
    , m_response(response)
    , m_data(data)
{
}

void BlobResourceSynchronousLoader::willSendRequestAsync(ResourceHandle*, ResourceRequest&& request, ResourceResponse&&, CompletionHandler<void(ResourceRequest&&)>&& completionHandler)
{
    ASSERT_NOT_REACHED();
    completionHandler(WTFMove(request));
}

#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
void BlobResourceSynchronousLoader::canAuthenticateAgainstProtectionSpaceAsync(ResourceHandle*, const ProtectionSpace&, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT_NOT_REACHED();
    completionHandler(false);
}
#endif

void BlobResourceSynchronousLoader::didReceiveResponseAsync(ResourceHandle* handle, ResourceResponse&& response, CompletionHandler<void()>&& completionHandler)
{
    // We cannot handle the size that is more than maximum integer.
    if (response.expectedContentLength() > INT_MAX) {
        m_error = ResourceError(webKitBlobResourceDomain, static_cast<int>(BlobResourceHandle::Error::NotReadableError), response.url(), "File is too large"_s);
        completionHandler();
        return;
    }

    m_response = response;

    // Read all the data.
    m_data.resize(static_cast<uint64_t>(response.expectedContentLength()));
    downcast<BlobResourceHandle>(*handle).readSync(m_data.mutableSpan());
    completionHandler();
}

void BlobResourceSynchronousLoader::didFail(ResourceHandle*, const ResourceError& error)
{
    m_error = error;
}

}

BlobResourceHandleBase::BlobResourceHandleBase(bool async, RefPtr<BlobData>&& blobData)
    : m_blobData(WTFMove(blobData))
{
    if (async)
        m_stream = makeUnique<AsyncFileStream>(*this);
    else
        m_stream = makeUnique<FileStream>();
}

BlobResourceHandleBase::~BlobResourceHandleBase() = default;

auto BlobResourceHandleBase::adjustAndValidateRangeBounds() -> std::optional<Error>
{
    if (!m_range->start) {
        if (!m_range->end)
            return Error::RangeError;
        // m_range->end indicates the last bytes to read.
        if (*m_range->end > m_totalSize) {
            m_range->start = 0;
            m_range->end = m_totalSize ? (m_totalSize - 1) : 0;
        } else {
            m_range->start = m_totalSize - *m_range->end;
            m_range->end = *m_range->start + *m_range->end - 1;
        }
    } else {
        if (*m_range->start >= m_totalSize)
            return Error::RangeError;
        if (m_range->end && *m_range->start > *m_range->end)
            return Error::RangeError;
        if (!m_range->end || *m_range->end >= m_totalSize)
            m_range->end = m_totalSize ? (m_totalSize - 1) : 0;
        else
            m_range->end = *m_range->end;
    }
    return { };
}

void BlobResourceHandleBase::closeFileIfOpen()
{
    if (m_isFileOpen) {
        m_isFileOpen = false;
        asyncStream()->close();
    }
}

void BlobResourceHandleBase::clearAsyncStream()
{
    m_stream = std::unique_ptr<AsyncFileStream>();
}

BlobData* BlobResourceHandleBase::blobData() const
{
    return m_blobData.get();
}

FileStream* BlobResourceHandleBase::syncStream() const
{
    return std::get<std::unique_ptr<FileStream>>(m_stream).get();
}

AsyncFileStream* BlobResourceHandleBase::asyncStream() const
{
    return std::get<std::unique_ptr<AsyncFileStream>>(m_stream).get();
}

///////////////////////////////////////////////////////////////////////////////
// BlobResourceHandle

Ref<BlobResourceHandle> BlobResourceHandle::createAsync(BlobData* blobData, const ResourceRequest& request, ResourceHandleClient* client)
{
    return adoptRef(*new BlobResourceHandle(blobData, request, client, true));
}

void BlobResourceHandle::loadResourceSynchronously(BlobData* blobData, const ResourceRequest& request, ResourceError& error, ResourceResponse& response, Vector<uint8_t>& data)
{
    if (!equalLettersIgnoringASCIICase(request.httpMethod(), "get"_s)) {
        error = ResourceError(webKitBlobResourceDomain, static_cast<int>(Error::MethodNotAllowed), response.url(), "Request method must be GET"_s);
        return;
    }

    BlobResourceSynchronousLoader loader(error, response, data);
    RefPtr<BlobResourceHandle> handle = adoptRef(new BlobResourceHandle(blobData, request, &loader, false));
    handle->start();
}

BlobResourceHandle::BlobResourceHandle(BlobData* blobData, const ResourceRequest& request, ResourceHandleClient* client, bool async)
    : BlobResourceHandleBase(async, blobData)
    , ResourceHandle { nullptr, request, client, false /* defersLoading */, false /* shouldContentSniff */, ContentEncodingSniffingPolicy::Default, nullptr /* sourceOrigin */, false /* isMainFrameNavigation */ }
{
}

BlobResourceHandle::~BlobResourceHandle() = default;

void BlobResourceHandle::cancel()
{
    clearAsyncStream();
    setIsFileOpen(false);

    m_aborted = true;

    ResourceHandle::cancel();
}

void BlobResourceHandleBase::start()
{
    if (!async()) {
        doStart();
        return;
    }

    // Finish this async call quickly and return.
    callOnMainThread([protectedThis = Ref { *this }]() mutable {
        protectedThis->doStart();
    });
}

void BlobResourceHandleBase::doStart()
{
    ASSERT(isMainThread());

    // Do not continue if the request is aborted or an error occurs.
    if (erroredOrAborted()) {
        clearStream();
        return;
    }

    if (!equalLettersIgnoringASCIICase(firstRequest().httpMethod(), "get"_s)) {
        didFail(Error::MethodNotAllowed);
        return;
    }

    // If the blob data is not found, fail now.
    if (!m_blobData) {
        didFail(Error::NotFoundError);
        return;
    }

    // Parse the "Range" header we care about.
    if (String range = firstRequest().httpHeaderField(HTTPHeaderName::Range); !range.isNull()) {
        m_range = parseRange(range, RangeAllowWhitespace::Yes);
        if (!m_range) {
            didFail(Error::RangeError);
            return;
        }
        m_isRangeRequest = true;
    }

    if (async())
        getSizeForNext();
    else {
        for (size_t i = 0; i < m_blobData->items().size() && !erroredOrAborted(); ++i)
            getSizeForNext();

        if (auto error = seek()) {
            didFail(*error);
            return;
        }
        dispatchDidReceiveResponse();
    }
}

void BlobResourceHandleBase::getSizeForNext()
{
    ASSERT(isMainThread());

    // Do we finish validating and counting size for all items?
    if (m_sizeItemCount >= m_blobData->items().size()) {
        if (auto error = seek()) {
            didFail(*error);
            return;
        }

        // Start reading if in asynchronous mode.
        if (async())
            dispatchDidReceiveResponse();
        return;
    }

    const BlobDataItem& item = m_blobData->items().at(m_sizeItemCount);
    switch (item.type()) {
    case BlobDataItem::Type::Data:
        didGetSize(item.length());
        break;
    case BlobDataItem::Type::File: {
        // Files know their sizes, but asking the stream to verify that the file wasn't modified.
        RefPtr file = item.file();
        if (async())
            asyncStream()->getSize(file->path(), file->expectedModificationTime());
        else
            didGetSize(syncStream()->getSize(file->path(), file->expectedModificationTime()));
        break;
    }
    default:
        ASSERT_NOT_REACHED();
    }
}

void BlobResourceHandleBase::didGetSize(long long size)
{
    ASSERT(isMainThread());

    if (erroredOrAborted()) {
        clearStream();
        return;
    }

    // If the size is -1, it means the file has been moved or changed. Fail now.
    if (size == -1) {
        didFail(Error::NotFoundError);
        return;
    }

    // The size passed back is the size of the whole file. If the underlying item is a sliced file, we need to use the slice length.
    const BlobDataItem& item = m_blobData->items().at(m_sizeItemCount);
    uint64_t updatedSize = static_cast<uint64_t>(item.length());

    // Cache the size.
    m_itemLengthList.append(updatedSize);

    // Count the size.
    m_totalSize += updatedSize;
    m_totalRemainingSize += updatedSize;
    ++m_sizeItemCount;

    // Continue with the next item.
    getSizeForNext();
}

auto BlobResourceHandleBase::seek() -> std::optional<Error>
{
    ASSERT(isMainThread());

    // Bail out if the range is not provided.
    if (!m_isRangeRequest)
        return { };

    if (auto error = adjustAndValidateRangeBounds())
        return error;

    // Skip the initial items that are not in the range.
    Checked<uint64_t> offset = *m_range->start;
    for (m_readItemCount = 0; m_readItemCount < m_blobData->items().size() && offset.value() >= lengthOfItemBeingRead(); ++m_readItemCount)
        offset -= lengthOfItemBeingRead();

    // Set the offset that need to jump to for the first item in the range.
    m_currentItemReadSize = offset.value();

    // Adjust the total remaining size in order not to go beyond the range.
    Checked<uint64_t> rangeSize = *m_range->end;
    rangeSize -= *m_range->start;
    rangeSize += 1uz;
    if (m_totalRemainingSize > rangeSize.value())
        m_totalRemainingSize = rangeSize.value();
    return { };
}

int BlobResourceHandle::readSync(std::span<uint8_t> buffer)
{
    ASSERT(isMainThread());

    ASSERT(!async());
    Ref<BlobResourceHandle> protectedThis(*this);

    int offset = 0;
    uint64_t remaining = buffer.size();
    while (remaining) {
        // Do not continue if the request is aborted or an error occurs.
        if (erroredOrAborted())
            break;

        // If there is no more remaining data to read, we are done.
        if (!totalRemainingSize() || readItemCount() >= blobData()->items().size())
            break;

        const BlobDataItem& item = blobData()->items().at(readItemCount());
        int bytesRead = 0;
        if (item.type() == BlobDataItem::Type::Data)
            bytesRead = readDataSync(item, buffer.subspan(offset));
        else if (item.type() == BlobDataItem::Type::File)
            bytesRead = readFileSync(item, buffer.subspan(offset));
        else
            ASSERT_NOT_REACHED();

        if (bytesRead > 0) {
            offset += bytesRead;
            remaining -= bytesRead;
        }
    }

    int result;
    if (erroredOrAborted())
        result = -1;
    else
        result = buffer.size() - remaining;

    if (result > 0)
        didReceiveData(buffer);

    if (!result)
        didFinish();

    return result;
}

int BlobResourceHandle::readDataSync(const BlobDataItem& item, std::span<uint8_t> buffer)
{
    ASSERT(isMainThread());

    ASSERT(!async());

    uint64_t remaining = item.length() - currentItemReadSize();
    uint64_t bytesToRead = std::min(std::min<uint64_t>(remaining, buffer.size()), totalRemainingSize());
    memcpySpan(buffer, item.protectedData()->span().subspan(item.offset() + currentItemReadSize()).first(bytesToRead));
    decrementTotalRemainingSizeBy(bytesToRead);

    setCurrentItemReadSize(currentItemReadSize() + bytesToRead);
    if (currentItemReadSize() == static_cast<uint64_t>(item.length())) {
        incrementReadItemCount();
        setCurrentItemReadSize(0);
    }

    return bytesToRead;
}

int BlobResourceHandle::readFileSync(const BlobDataItem& item, std::span<uint8_t> buffer)
{
    ASSERT(isMainThread());

    ASSERT(!async());

    if (!isFileOpen()) {
        auto bytesToRead = lengthOfItemBeingRead() - currentItemReadSize();
        if (bytesToRead > totalRemainingSize())
            bytesToRead = totalRemainingSize();
        bool success = syncStream()->openForRead(item.protectedFile()->path(), item.offset() + currentItemReadSize(), bytesToRead);
        setCurrentItemReadSize(0);
        if (!success) {
            m_errorCode = Error::NotReadableError;
            return 0;
        }

        setIsFileOpen(true);
    }

    int bytesRead = syncStream()->read(buffer);
    if (bytesRead < 0) {
        m_errorCode = Error::NotReadableError;
        return 0;
    }
    if (!bytesRead) {
        syncStream()->close();
        setIsFileOpen(false);
        incrementReadItemCount();
    } else
        decrementTotalRemainingSizeBy(bytesRead);

    return bytesRead;
}

void BlobResourceHandleBase::readAsync()
{
    ASSERT(RunLoop::isMain());

    if (erroredOrAborted())
        return;

    while (m_totalRemainingSize && m_readItemCount < m_blobData->items().size()) {
        const BlobDataItem& item = m_blobData->items().at(m_readItemCount);
        switch (item.type()) {
        case BlobDataItem::Type::Data:
            if (!readDataAsync(item))
                return; // error occurred
            break;
        case BlobDataItem::Type::File:
            readFileAsync(item);
            return;
        }
    }
    didFinish();
}

bool BlobResourceHandleBase::readDataAsync(const BlobDataItem& item)
{
    ASSERT(isMainThread());
    ASSERT(item.data());

    ASSERT(m_currentItemReadSize <= static_cast<uint64_t>(item.length()));
    uint64_t bytesToRead = static_cast<uint64_t>(item.length()) - m_currentItemReadSize;
    if (bytesToRead > m_totalRemainingSize)
        bytesToRead = m_totalRemainingSize;

    auto data = item.protectedData()->span().subspan(item.offset() + m_currentItemReadSize, bytesToRead);
    m_currentItemReadSize = 0;

    return consumeData(data);
}

void BlobResourceHandleBase::readFileAsync(const BlobDataItem& item)
{
    ASSERT(isMainThread());

    if (m_isFileOpen) {
        asyncStream()->read(buffer().mutableSpan());
        return;
    }

    uint64_t bytesToRead = lengthOfItemBeingRead() - m_currentItemReadSize;
    if (bytesToRead > m_totalRemainingSize)
        bytesToRead = static_cast<int>(m_totalRemainingSize);
    asyncStream()->openForRead(item.protectedFile()->path(), item.offset() + m_currentItemReadSize, bytesToRead);
    m_isFileOpen = true;
    m_currentItemReadSize = 0;
}

void BlobResourceHandleBase::didOpen(bool success)
{
    ASSERT(async());

    if (erroredOrAborted()) {
        clearStream();
        return;
    }

    if (!success) {
        didFail(Error::NotReadableError);
        return;
    }

    // Continue the reading.
    readAsync();
}

void BlobResourceHandleBase::didRead(int bytesRead)
{
    if (erroredOrAborted()) {
        clearStream();
        return;
    }

    if (bytesRead < 0) {
        didFail(Error::NotReadableError);
        return;
    }

    if (consumeData(m_buffer.subspan(0, bytesRead)))
        readAsync();
}

bool BlobResourceHandleBase::consumeData(std::span<const uint8_t> data)
{
    ASSERT(async());

    m_totalRemainingSize -= data.size();

    // Notify the client.
    if (!data.empty()) {
        if (!didReceiveData(data))
            return false;
    }

    if (m_isFileOpen) {
        // When the current item is a file item, the reading is completed only if bytesRead is 0.
        if (data.empty()) {
            closeFileIfOpen();

            // Move to the next item.
            ++m_readItemCount;
        }
    } else {
        // Otherwise, we read the current text item as a whole and move to the next item.
        ++m_readItemCount;
    }

    return true;
}

bool BlobResourceHandle::shouldAbortDispatchDidReceiveResponse()
{
    if (!client())
        return true;

    if (m_errorCode != Error::NoError) {
        didFail(m_errorCode);
        return true;
    }

    return false;
}

void BlobResourceHandleBase::dispatchDidReceiveResponse()
{
    ASSERT(isMainThread());

    if (shouldAbortDispatchDidReceiveResponse())
        return;

    ResourceResponse response(URL { firstRequest().url() }, extractMIMETypeFromMediaType(m_blobData->contentType()), m_totalRemainingSize, String());
    response.setHTTPStatusCode(m_isRangeRequest ? httpPartialContent : httpOK);
    response.setHTTPStatusText(m_isRangeRequest ? httpPartialContentText : httpOKText);

    response.setHTTPHeaderField(HTTPHeaderName::ContentType, m_blobData->contentType());
    response.setTextEncodingName(extractCharsetFromMediaType(m_blobData->contentType()).toString());
    response.setHTTPHeaderField(HTTPHeaderName::ContentLength, String::number(m_totalRemainingSize));
    addPolicyContainerHeaders(response, m_blobData->policyContainer());

    if (m_isRangeRequest)
        response.setHTTPHeaderField(HTTPHeaderName::ContentRange, ParsedContentRange(*m_range->start, *m_range->end, m_totalSize).headerValue());

    // FIXME: If a resource identified with a blob: URL is a File object, user agents must use that file's name attribute,
    // as if the response had a Content-Disposition header with the filename parameter set to the File's name attribute.
    // Notably, this will affect a name suggested in "File Save As".

    didReceiveResponse(WTFMove(response));
}

void BlobResourceHandle::didReceiveResponse(ResourceResponse&& response)
{
    client()->didReceiveResponseAsync(this, WTFMove(response), [this, protectedThis = Ref { *this }] {
        buffer().resize(bufferSize);
        readAsync();
    });
}

bool BlobResourceHandle::didReceiveData(std::span<const uint8_t> data)
{
    if (client())
        client()->didReceiveBuffer(this, SharedBuffer::create(data), data.size());
    return true;
}

void BlobResourceHandle::didFail(Error errorCode)
{
    if (client())
        client()->didFail(this, ResourceError(webKitBlobResourceDomain, static_cast<int>(errorCode), firstRequest().url(), String()));

    closeFileIfOpen();
}

static void doNotifyFinish(BlobResourceHandle& handle)
{
    if (handle.aborted())
        return;

    if (!handle.client())
        return;

    handle.client()->didFinishLoading(&handle, { });
}

void BlobResourceHandle::didFinish()
{
    if (!async()) {
        doNotifyFinish(*this);
        return;
    }

    // Schedule to notify the client from a standalone function because the client might dispose the handle immediately from the callback function
    // while we still have BlobResourceHandle calls in the stack.
    callOnMainThread([protectedThis = Ref { *this }]() mutable {
        doNotifyFinish(protectedThis);
    });

}

} // namespace WebCore
