/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2015-2025 Apple Inc. All rights reserved.
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
#include "InspectorNetworkAgent.h"

#include "CachedCSSStyleSheet.h"
#include "CachedRawResource.h"
#include "CachedResource.h"
#include "CachedResourceLoader.h"
#include "CachedResourceRequestInitiatorTypes.h"
#include "CachedScript.h"
#include "CertificateInfo.h"
#include "CertificateSummary.h"
#include "CookieJar.h"
#include "DocumentInlines.h"
#include "DocumentLoader.h"
#include "DocumentThreadableLoader.h"
#include "FormData.h"
#include "FrameLoader.h"
#include "HTTPHeaderMap.h"
#include "HTTPHeaderNames.h"
#include "InspectorDOMAgent.h"
#include "InspectorTimelineAgent.h"
#include "InstrumentingAgents.h"
#include "JSDOMWindowCustom.h"
#include "JSExecState.h"
#include "JSWebSocket.h"
#include "LoaderStrategy.h"
#include "LocalFrame.h"
#include "MIMETypeRegistry.h"
#include "MemoryCache.h"
#include "Page.h"
#include "PlatformStrategies.h"
#include "ProgressTracker.h"
#include "RenderObjectInlines.h"
#include "ResourceError.h"
#include "ResourceLoader.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "ScriptableDocumentParser.h"
#include "SubresourceLoader.h"
#include "TextResourceDecoder.h"
#include "ThreadableLoaderClient.h"
#include "ThreadableWebSocketChannel.h"
#include "WebConsoleAgent.h"
#include "WebCorePersistentCoders.h"
#include "WebSocket.h"
#include "WebSocketFrame.h"
#include <JavaScriptCore/IdentifiersFactory.h>
#include <JavaScriptCore/InjectedScript.h>
#include <JavaScriptCore/InjectedScriptManager.h>
#include <JavaScriptCore/InspectorProtocolObjects.h>
#include <JavaScriptCore/JSCInlines.h>
#include <JavaScriptCore/ScriptCallStack.h>
#include <JavaScriptCore/ScriptCallStackFactory.h>
#include <wtf/JSONValues.h>
#include <wtf/Lock.h>
#include <wtf/RefPtr.h>
#include <wtf/Stopwatch.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/URL.h>
#include <wtf/persistence/PersistentEncoder.h>
#include <wtf/text/Base64.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/WTFString.h>

typedef Inspector::NetworkBackendDispatcherHandler::LoadResourceCallback LoadResourceCallback;

namespace WebCore {

using namespace Inspector;

WTF_MAKE_TZONE_ALLOCATED_IMPL(InspectorNetworkAgent);
WTF_MAKE_TZONE_ALLOCATED_IMPL(InspectorNetworkAgent::PendingInterceptRequest);
WTF_MAKE_TZONE_ALLOCATED_IMPL(InspectorNetworkAgent::PendingInterceptResponse);

namespace {

class InspectorThreadableLoaderClient final : public ThreadableLoaderClient {
    WTF_MAKE_NONCOPYABLE(InspectorThreadableLoaderClient);
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(InspectorThreadableLoaderClient, Loader);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(InspectorThreadableLoaderClient);
public:
    InspectorThreadableLoaderClient(RefPtr<LoadResourceCallback>&& callback)
        : m_callback(WTFMove(callback))
    {
    }

    ~InspectorThreadableLoaderClient() override = default;

    void didReceiveResponse(ScriptExecutionContextIdentifier, std::optional<ResourceLoaderIdentifier>, const ResourceResponse& response) override
    {
        m_mimeType = response.mimeType();
        m_statusCode = response.httpStatusCode();

        // FIXME: This assumes text only responses. We should support non-text responses as well.
        PAL::TextEncoding textEncoding(response.textEncodingName());
        bool useDetector = false;
        if (!textEncoding.isValid()) {
            textEncoding = PAL::UTF8Encoding();
            useDetector = true;
        }

        m_decoder = TextResourceDecoder::create("text/plain"_s, textEncoding, useDetector);
    }

    void didReceiveData(const SharedBuffer& buffer) override
    {
        if (buffer.isEmpty())
            return;

        m_responseText.append(m_decoder->decode(buffer.span()));
    }

    void didFinishLoading(ScriptExecutionContextIdentifier, std::optional<ResourceLoaderIdentifier>, const NetworkLoadMetrics&) override
    {
        if (m_decoder)
            m_responseText.append(m_decoder->flush());

        m_callback->sendSuccess(m_responseText.toString(), m_mimeType, m_statusCode);
        dispose();
    }

    void didFail(std::optional<ScriptExecutionContextIdentifier>, const ResourceError& error) override
    {
        m_callback->sendFailure(error.isAccessControl() ? "Loading resource for inspector failed access control check"_s : "Loading resource for inspector failed"_s);
        dispose();
    }

    void setLoader(RefPtr<ThreadableLoader>&& loader)
    {
        m_loader = WTFMove(loader);
    }

private:
    void dispose()
    {
        m_loader = nullptr;
        delete this;
    }

    RefPtr<LoadResourceCallback> m_callback;
    RefPtr<ThreadableLoader> m_loader;
    RefPtr<TextResourceDecoder> m_decoder;
    String m_mimeType;
    StringBuilder m_responseText;
    int m_statusCode;
};

Ref<Inspector::Protocol::Network::WebSocketFrame> buildWebSocketMessage(const WebSocketFrame& frame)
{
    return Inspector::Protocol::Network::WebSocketFrame::create()
        .setOpcode(frame.opCode)
        .setMask(frame.masked)
        .setPayloadData(frame.opCode == 1 ? String::fromUTF8WithLatin1Fallback(frame.payload) : base64EncodeToString(frame.payload))
        .setPayloadLength(frame.payload.size())
        .release();
}

} // namespace

InspectorNetworkAgent::InspectorNetworkAgent(WebAgentContext& context, const NetworkResourcesData::Settings& networkResourcesDataSettings)
    : InspectorAgentBase("Network"_s, context)
    , m_frontendDispatcher(makeUniqueRef<Inspector::NetworkFrontendDispatcher>(context.frontendRouter))
    , m_backendDispatcher(Inspector::NetworkBackendDispatcher::create(context.backendDispatcher, this))
    , m_injectedScriptManager(context.injectedScriptManager)
    , m_resourcesData(makeUniqueRef<NetworkResourcesData>(networkResourcesDataSettings))
{
}

InspectorNetworkAgent::~InspectorNetworkAgent() = default;

void InspectorNetworkAgent::didCreateFrontendAndBackend()
{
}

void InspectorNetworkAgent::willDestroyFrontendAndBackend(Inspector::DisconnectReason)
{
    disable();
}

static Ref<Inspector::Protocol::Network::Headers> buildObjectForHeaders(const HTTPHeaderMap& headers)
{
    auto headersValue = Inspector::Protocol::Network::Headers::create().release();

    auto headersObject = headersValue->asObject();
    for (const auto& header : headers)
        headersObject->setString(header.key, header.value);

    return headersValue;
}

Ref<Inspector::Protocol::Network::ResourceTiming> InspectorNetworkAgent::buildObjectForTiming(const NetworkLoadMetrics& timing, ResourceLoader& resourceLoader)
{
    auto elapsedTimeSince = [&] (const MonotonicTime& time) {
        return m_environment.executionStopwatch().elapsedTimeSince(time).seconds();
    };
    auto millisecondsSinceFetchStart = [&] (const MonotonicTime& time) {
        if (!time)
            return 0.0;
        return (time - timing.fetchStart).milliseconds();
    };

    return Inspector::Protocol::Network::ResourceTiming::create()
        .setStartTime(elapsedTimeSince(resourceLoader.loadTiming().startTime()))
        .setRedirectStart(elapsedTimeSince(timing.redirectStart))
        .setRedirectEnd(elapsedTimeSince(timing.fetchStart))
        .setFetchStart(elapsedTimeSince(timing.fetchStart))
        .setDomainLookupStart(millisecondsSinceFetchStart(timing.domainLookupStart))
        .setDomainLookupEnd(millisecondsSinceFetchStart(timing.domainLookupEnd))
        .setConnectStart(millisecondsSinceFetchStart(timing.connectStart))
        .setConnectEnd(millisecondsSinceFetchStart(timing.connectEnd))
        .setSecureConnectionStart(millisecondsSinceFetchStart(timing.secureConnectionStart))
        .setRequestStart(millisecondsSinceFetchStart(timing.requestStart))
        .setResponseStart(millisecondsSinceFetchStart(timing.responseStart))
        .setResponseEnd(millisecondsSinceFetchStart(timing.responseEnd))
        .release();
}

static Inspector::Protocol::Network::Metrics::Priority toProtocol(NetworkLoadPriority priority)
{
    switch (priority) {
    case NetworkLoadPriority::Low:
        return Inspector::Protocol::Network::Metrics::Priority::Low;
    case NetworkLoadPriority::Medium:
        return Inspector::Protocol::Network::Metrics::Priority::Medium;
    case NetworkLoadPriority::High:
        return Inspector::Protocol::Network::Metrics::Priority::High;
    case NetworkLoadPriority::Unknown:
        break;
    }

    ASSERT_NOT_REACHED();
    return Inspector::Protocol::Network::Metrics::Priority::Medium;
}

Ref<Inspector::Protocol::Network::Metrics> InspectorNetworkAgent::buildObjectForMetrics(const NetworkLoadMetrics& networkLoadMetrics)
{
    auto metrics = Inspector::Protocol::Network::Metrics::create().release();

    if (!networkLoadMetrics.protocol.isNull())
        metrics->setProtocol(networkLoadMetrics.protocol);
    if (auto* additionalMetrics = networkLoadMetrics.additionalNetworkLoadMetricsForWebInspector.get()) {
        if (additionalMetrics->priority != NetworkLoadPriority::Unknown)
            metrics->setPriority(toProtocol(additionalMetrics->priority));
        if (!additionalMetrics->remoteAddress.isNull())
            metrics->setRemoteAddress(additionalMetrics->remoteAddress);
        if (!additionalMetrics->connectionIdentifier.isNull())
            metrics->setConnectionIdentifier(additionalMetrics->connectionIdentifier);
        if (!additionalMetrics->requestHeaders.isEmpty())
            metrics->setRequestHeaders(buildObjectForHeaders(additionalMetrics->requestHeaders));
        if (additionalMetrics->requestHeaderBytesSent != std::numeric_limits<uint64_t>::max())
            metrics->setRequestHeaderBytesSent(additionalMetrics->requestHeaderBytesSent);
        if (additionalMetrics->requestBodyBytesSent != std::numeric_limits<uint64_t>::max())
            metrics->setRequestBodyBytesSent(additionalMetrics->requestBodyBytesSent);
        if (additionalMetrics->responseHeaderBytesReceived != std::numeric_limits<uint64_t>::max())
            metrics->setResponseHeaderBytesReceived(additionalMetrics->responseHeaderBytesReceived);
        metrics->setIsProxyConnection(additionalMetrics->isProxyConnection);
    }

    if (networkLoadMetrics.responseBodyBytesReceived != std::numeric_limits<uint64_t>::max())
        metrics->setResponseBodyBytesReceived(networkLoadMetrics.responseBodyBytesReceived);
    if (networkLoadMetrics.responseBodyDecodedSize != std::numeric_limits<uint64_t>::max())
        metrics->setResponseBodyDecodedSize(networkLoadMetrics.responseBodyDecodedSize);

    auto connectionPayload = Inspector::Protocol::Security::Connection::create()
        .release();

    if (auto* additionalMetrics = networkLoadMetrics.additionalNetworkLoadMetricsForWebInspector.get()) {
        if (!additionalMetrics->tlsProtocol.isEmpty())
            connectionPayload->setProtocol(additionalMetrics->tlsProtocol);
        if (!additionalMetrics->tlsCipher.isEmpty())
            connectionPayload->setCipher(additionalMetrics->tlsCipher);
    }

    metrics->setSecurityConnection(WTFMove(connectionPayload));

    return metrics;
}

static Inspector::Protocol::Network::ReferrerPolicy toProtocol(ReferrerPolicy referrerPolicy)
{
    switch (referrerPolicy) {
    case ReferrerPolicy::EmptyString:
        return Inspector::Protocol::Network::ReferrerPolicy::EmptyString;
    case ReferrerPolicy::NoReferrer:
        return Inspector::Protocol::Network::ReferrerPolicy::NoReferrer;
    case ReferrerPolicy::NoReferrerWhenDowngrade:
        return Inspector::Protocol::Network::ReferrerPolicy::NoReferrerWhenDowngrade;
    case ReferrerPolicy::SameOrigin:
        return Inspector::Protocol::Network::ReferrerPolicy::SameOrigin;
    case ReferrerPolicy::Origin:
        return Inspector::Protocol::Network::ReferrerPolicy::Origin;
    case ReferrerPolicy::StrictOrigin:
        return Inspector::Protocol::Network::ReferrerPolicy::StrictOrigin;
    case ReferrerPolicy::OriginWhenCrossOrigin:
        return Inspector::Protocol::Network::ReferrerPolicy::OriginWhenCrossOrigin;
    case ReferrerPolicy::StrictOriginWhenCrossOrigin:
        return Inspector::Protocol::Network::ReferrerPolicy::StrictOriginWhenCrossOrigin;
    case ReferrerPolicy::UnsafeUrl:
        return Inspector::Protocol::Network::ReferrerPolicy::UnsafeUrl;
    }

    ASSERT_NOT_REACHED();
    return Inspector::Protocol::Network::ReferrerPolicy::EmptyString;
}

static Ref<Inspector::Protocol::Network::Request> buildObjectForResourceRequest(const ResourceRequest& request, ResourceLoader* resourceLoader)
{
    auto requestObject = Inspector::Protocol::Network::Request::create()
        .setUrl(request.url().string())
        .setMethod(request.httpMethod())
        .setHeaders(buildObjectForHeaders(request.httpHeaderFields()))
        .release();

    if (request.httpBody() && !request.httpBody()->isEmpty()) {
        auto bytes = request.httpBody()->flatten();
        requestObject->setPostData(String::fromUTF8WithLatin1Fallback(bytes.span()));
    }

    if (resourceLoader) {
        requestObject->setReferrerPolicy(toProtocol(resourceLoader->options().referrerPolicy));

        if (auto integrity = resourceLoader->options().integrity; !integrity.isEmpty())
            requestObject->setIntegrity(integrity);
    }

    return requestObject;
}

static Inspector::Protocol::Network::Response::Source responseSource(ResourceResponse::Source source)
{
    switch (source) {
    case ResourceResponse::Source::DOMCache:
    case ResourceResponse::Source::ApplicationCache:
        // FIXME: Add support for ApplicationCache in inspector.
    case ResourceResponse::Source::Unknown:
        return Inspector::Protocol::Network::Response::Source::Unknown;
    case ResourceResponse::Source::Network:
        return Inspector::Protocol::Network::Response::Source::Network;
    case ResourceResponse::Source::MemoryCache:
    case ResourceResponse::Source::MemoryCacheAfterValidation:
        return Inspector::Protocol::Network::Response::Source::MemoryCache;
    case ResourceResponse::Source::DiskCache:
    case ResourceResponse::Source::DiskCacheAfterValidation:
        return Inspector::Protocol::Network::Response::Source::DiskCache;
    case ResourceResponse::Source::ServiceWorker:
        return Inspector::Protocol::Network::Response::Source::ServiceWorker;
    case ResourceResponse::Source::InspectorOverride:
        return Inspector::Protocol::Network::Response::Source::InspectorOverride;
    }

    ASSERT_NOT_REACHED();
    return Inspector::Protocol::Network::Response::Source::Unknown;
}

RefPtr<Inspector::Protocol::Network::Response> InspectorNetworkAgent::buildObjectForResourceResponse(const ResourceResponse& response, ResourceLoader* resourceLoader)
{
    if (response.isNull())
        return nullptr;

    auto responseObject = Inspector::Protocol::Network::Response::create()
        .setUrl(response.url().string())
        .setStatus(response.httpStatusCode())
        .setStatusText(response.httpStatusText())
        .setHeaders(buildObjectForHeaders(response.httpHeaderFields()))
        .setMimeType(response.mimeType())
        .setSource(responseSource(response.source()))
        .release();

    if (resourceLoader) {
        auto* metrics = response.deprecatedNetworkLoadMetricsOrNull();
        responseObject->setTiming(buildObjectForTiming(metrics ? *metrics : NetworkLoadMetrics::emptyMetrics(), *resourceLoader));
    }

    if (auto& certificateInfo = response.certificateInfo()) {
        auto securityPayload = Inspector::Protocol::Security::Security::create()
            .release();

        if (auto certificateSummaryInfo = certificateInfo.value().summary()) {
            auto certificatePayload = Inspector::Protocol::Security::Certificate::create()
                .release();

            certificatePayload->setSubject(certificateSummaryInfo.value().subject);

            if (auto validFrom = certificateSummaryInfo.value().validFrom)
                certificatePayload->setValidFrom(validFrom.seconds());

            if (auto validUntil = certificateSummaryInfo.value().validUntil)
                certificatePayload->setValidUntil(validUntil.seconds());

            auto dnsNamesPayload = JSON::ArrayOf<String>::create();
            for (auto& dnsName : certificateSummaryInfo.value().dnsNames)
                dnsNamesPayload->addItem(dnsName);
            if (dnsNamesPayload->length())
                certificatePayload->setDnsNames(WTFMove(dnsNamesPayload));

            auto ipAddressesPayload = JSON::ArrayOf<String>::create();
            for (auto& ipAddress : certificateSummaryInfo.value().ipAddresses)
                ipAddressesPayload->addItem(ipAddress);
            if (ipAddressesPayload->length())
                certificatePayload->setIpAddresses(WTFMove(ipAddressesPayload));

            securityPayload->setCertificate(WTFMove(certificatePayload));
        }

        responseObject->setSecurity(WTFMove(securityPayload));
    }

    return responseObject;
}

Ref<Inspector::Protocol::Network::CachedResource> InspectorNetworkAgent::buildObjectForCachedResource(CachedResource* cachedResource)
{
    auto resourceObject = Inspector::Protocol::Network::CachedResource::create()
        .setUrl(cachedResource->url().string())
        .setType(InspectorPageAgent::cachedResourceTypeJSON(*cachedResource))
        .setBodySize(cachedResource->encodedSize())
        .release();

    if (auto resourceResponse = buildObjectForResourceResponse(cachedResource->response(), cachedResource->loader()))
        resourceObject->setResponse(resourceResponse.releaseNonNull());

    String sourceMappingURL = InspectorPageAgent::sourceMapURLForResource(cachedResource);
    if (!sourceMappingURL.isEmpty())
        resourceObject->setSourceMapURL(sourceMappingURL);

    return resourceObject;
}

double InspectorNetworkAgent::timestamp()
{
    return m_environment.executionStopwatch().elapsedTime().seconds();
}

void InspectorNetworkAgent::willSendRequest(ResourceLoaderIdentifier identifier, DocumentLoader* loader, ResourceRequest& request, const ResourceResponse& redirectResponse, InspectorPageAgent::ResourceType type, ResourceLoader* resourceLoader)
{
    if (request.hiddenFromInspector()) {
        m_hiddenRequestIdentifiers.add(identifier);
        return;
    }

    double sendTimestamp = timestamp();
    WallTime walltime = WallTime::now();

    auto requestId = IdentifiersFactory::requestId(identifier.toUInt64());
    auto frameId = frameIdentifier(loader);
    auto loaderId = loaderIdentifier(loader);
    String targetId = request.initiatorIdentifier();


    m_resourcesData->resourceCreated(requestId, loaderId, type);

    for (auto& entry : m_extraRequestHeaders)
        request.setHTTPHeaderField(entry.key, entry.value);

    auto protocolResourceType = InspectorPageAgent::resourceTypeJSON(type);

    Document* document = loader && loader->frame() ? loader->frame()->document() : nullptr;
    auto initiatorObject = buildInitiatorObject(document, &request);

    String url = loader ? loader->url().string() : request.url().string();
    std::optional<Inspector::Protocol::Page::ResourceType> typePayload;
    if (type != InspectorPageAgent::OtherResource)
        typePayload = protocolResourceType;
    m_frontendDispatcher->requestWillBeSent(requestId, frameId, loaderId, url, buildObjectForResourceRequest(request, resourceLoader), sendTimestamp, walltime.secondsSinceEpoch().seconds(), WTFMove(initiatorObject), buildObjectForResourceResponse(redirectResponse, nullptr), WTFMove(typePayload), targetId);
}

static InspectorPageAgent::ResourceType resourceTypeForCachedResource(const CachedResource* resource)
{
    if (resource)
        return InspectorPageAgent::inspectorResourceType(*resource);

    return InspectorPageAgent::OtherResource;
}

static InspectorPageAgent::ResourceType resourceTypeForLoadType(InspectorInstrumentation::LoadType loadType)
{
    switch (loadType) {
    case InspectorInstrumentation::LoadType::Ping:
        return InspectorPageAgent::PingResource;
    case InspectorInstrumentation::LoadType::Beacon:
        return InspectorPageAgent::BeaconResource;
    }

    ASSERT_NOT_REACHED();
    return InspectorPageAgent::OtherResource;
}

void InspectorNetworkAgent::willSendRequest(ResourceLoaderIdentifier identifier, DocumentLoader* loader, ResourceRequest& request, const ResourceResponse& redirectResponse, const CachedResource* cachedResource, ResourceLoader* resourceLoader)
{
    InspectorPageAgent::ResourceType type = InspectorPageAgent::OtherResource;
    if (m_loadingXHRSynchronously || request.requester() == ResourceRequestRequester::XHR)
        type = InspectorPageAgent::XHRResource;
    else if (request.requester() == ResourceRequestRequester::Fetch)
        type = InspectorPageAgent::FetchResource;
    else if (loader && equalIgnoringFragmentIdentifier(request.url(), loader->url()) && !loader->isCommitted())
        type = InspectorPageAgent::DocumentResource;
    else if (loader) {
        for (auto& linkIcon : loader->linkIcons()) {
            if (equalIgnoringFragmentIdentifier(request.url(), linkIcon.url)) {
                type = InspectorPageAgent::ImageResource;
                break;
            }
        }
    }
    if (type == InspectorPageAgent::OtherResource) {
        if (!cachedResource && loader)
            cachedResource = InspectorPageAgent::cachedResource(loader->frame(), request.url());
        type = resourceTypeForCachedResource(cachedResource);
    }
    willSendRequest(identifier, loader, request, redirectResponse, type, resourceLoader);
}

void InspectorNetworkAgent::willSendRequestOfType(ResourceLoaderIdentifier identifier, DocumentLoader* loader, ResourceRequest& request, InspectorInstrumentation::LoadType loadType)
{
    willSendRequest(identifier, loader, request, ResourceResponse(), resourceTypeForLoadType(loadType), nullptr);
}

void InspectorNetworkAgent::didReceiveResponse(ResourceLoaderIdentifier identifier, DocumentLoader* loader, const ResourceResponse& response, ResourceLoader* resourceLoader)
{
    if (m_hiddenRequestIdentifiers.contains(identifier))
        return;

    String requestId = IdentifiersFactory::requestId(identifier.toUInt64());

    std::optional<ResourceResponse> realResponse;
    if (platformStrategies()->loaderStrategy()->havePerformedSecurityChecks(response)) {
        callOnMainThreadAndWait([&] {
            // We do not need to isolate response since it comes straight from IPC, but we might want to isolate it for extra safety.
            auto response = platformStrategies()->loaderStrategy()->responseFromResourceLoadIdentifier(identifier);
            if (!response.isNull())
                realResponse = WTFMove(response);
        });
    }

    auto resourceResponse = buildObjectForResourceResponse(realResponse ? *realResponse : response, resourceLoader);
    ASSERT(resourceResponse);

    bool isNotModified = response.httpStatusCode() == 304;

    CachedResource* cachedResource = nullptr;
    if (auto* subresourceLoader = dynamicDowncast<SubresourceLoader>(resourceLoader); subresourceLoader && !isNotModified)
        cachedResource = subresourceLoader->cachedResource();
    if (!cachedResource && loader)
        cachedResource = InspectorPageAgent::cachedResource(loader->frame(), response.url());

    if (cachedResource) {
        // Use mime type from cached resource in case the one in response is empty.
        if (resourceResponse && response.mimeType().isEmpty())
            resourceResponse->setString("mimeType"_s, cachedResource->response().mimeType());
        m_resourcesData->addCachedResource(requestId, cachedResource);
    }

    InspectorPageAgent::ResourceType type = m_resourcesData->resourceType(requestId);
    InspectorPageAgent::ResourceType newType = cachedResource ? InspectorPageAgent::inspectorResourceType(*cachedResource) : type;

    // FIXME: XHRResource is returned for CachedResource::Type::RawResource, it should be OtherResource unless it truly is an XHR.
    // RawResource is used for loading worker scripts, and those should stay as ScriptResource and not change to XHRResource.
    if (type != newType && newType != InspectorPageAgent::XHRResource && newType != InspectorPageAgent::OtherResource)
        type = newType;
    
    // FIXME: <webkit.org/b/216125> 304 Not Modified responses for XHR/Fetch do not have all their information from the cache.
    if (isNotModified && (type == InspectorPageAgent::XHRResource || type == InspectorPageAgent::FetchResource) && (!cachedResource || !cachedResource->encodedSize())) {
        if (auto previousResourceData = m_resourcesData->dataForURL(response.url().string())) {
            if (previousResourceData->hasContent())
                m_resourcesData->setResourceContent(requestId, previousResourceData->content(), previousResourceData->base64Encoded());
            else if (previousResourceData->hasBufferedData()) {
                previousResourceData->buffer()->forEachSegmentAsSharedBuffer([&](auto&& buffer) {
                    m_resourcesData->maybeAddResourceData(requestId, buffer);
                });
            }
            
            resourceResponse->setString("mimeType"_s, previousResourceData->mimeType());
            
            resourceResponse->setInteger("status"_s, previousResourceData->httpStatusCode());
            resourceResponse->setString("statusText"_s, previousResourceData->httpStatusText());
            
            resourceResponse->setString("source"_s, Inspector::Protocol::Helpers::getEnumConstantValue(Inspector::Protocol::Network::Response::Source::DiskCache));
        }
    }

    Inspector::Protocol::Network::FrameId frameId = frameIdentifier(loader);
    Inspector::Protocol::Network::LoaderId loaderId = loaderIdentifier(loader);

    m_resourcesData->responseReceived(requestId, frameId, response, type, shouldForceBufferingNetworkResourceData());

    m_frontendDispatcher->responseReceived(requestId, frameId, loaderId, timestamp(), InspectorPageAgent::resourceTypeJSON(type), resourceResponse.releaseNonNull());

    // If we revalidated the resource and got Not modified, send content length following didReceiveResponse
    // as there will be no calls to didReceiveData from the network stack.
    if (isNotModified && cachedResource && cachedResource->encodedSize())
        didReceiveData(identifier, nullptr, cachedResource->encodedSize(), 0);
}

void InspectorNetworkAgent::didReceiveData(ResourceLoaderIdentifier identifier, const SharedBuffer* data, int expectedDataLength, int encodedDataLength)
{
    if (m_hiddenRequestIdentifiers.contains(identifier))
        return;

    String requestId = IdentifiersFactory::requestId(identifier.toUInt64());

    if (data) {
        NetworkResourcesData::ResourceData const* resourceData = m_resourcesData->maybeAddResourceData(requestId, *data);

        // For a synchronous XHR, if we didn't add data then we can apply it here as base64 encoded content.
        // Often the data is text and we would have a decoder, but for non-text we won't have a decoder.
        // Sync XHRs may not have a cached resource, while non-sync XHRs usually transfer data over on completion.
        if (m_loadingXHRSynchronously && resourceData && !resourceData->hasBufferedData() && !resourceData->cachedResource())
            m_resourcesData->setResourceContent(requestId, base64EncodeToString(data->span()), true);
    }

    m_frontendDispatcher->dataReceived(requestId, timestamp(), expectedDataLength, encodedDataLength);
}

void InspectorNetworkAgent::didFinishLoading(ResourceLoaderIdentifier identifier, DocumentLoader* loader, const NetworkLoadMetrics& networkLoadMetrics, ResourceLoader*)
{
    if (m_hiddenRequestIdentifiers.remove(identifier))
        return;

    double elapsedFinishTime;
    if (networkLoadMetrics.responseEnd)
        elapsedFinishTime = m_environment.executionStopwatch().elapsedTimeSince(networkLoadMetrics.responseEnd).seconds();
    else
        elapsedFinishTime = timestamp();

    String requestId = IdentifiersFactory::requestId(identifier.toUInt64());
    if (loader && loader->frameLoader() && m_resourcesData->resourceType(requestId) == InspectorPageAgent::DocumentResource)
        m_resourcesData->addResourceSharedBuffer(requestId, loader->frameLoader()->documentLoader()->mainResourceData(), loader->frame()->document()->encoding());

    m_resourcesData->maybeDecodeDataToContent(requestId);

    String sourceMappingURL;
    NetworkResourcesData::ResourceData const* resourceData = m_resourcesData->data(requestId);
    if (resourceData && resourceData->cachedResource())
        sourceMappingURL = InspectorPageAgent::sourceMapURLForResource(resourceData->cachedResource());

    std::optional<NetworkLoadMetrics> realMetrics;
    if (platformStrategies()->loaderStrategy()->shouldPerformSecurityChecks() && !networkLoadMetrics.isComplete()) {
        callOnMainThreadAndWait([&] {
            realMetrics = platformStrategies()->loaderStrategy()->networkMetricsFromResourceLoadIdentifier(identifier).isolatedCopy();
        });
    }
    auto metrics = buildObjectForMetrics(realMetrics ? *realMetrics : networkLoadMetrics);

    m_frontendDispatcher->loadingFinished(requestId, elapsedFinishTime, sourceMappingURL, WTFMove(metrics));
}

void InspectorNetworkAgent::didFailLoading(ResourceLoaderIdentifier identifier, DocumentLoader* loader, const ResourceError& error)
{
    if (m_hiddenRequestIdentifiers.remove(identifier))
        return;

    String requestId = IdentifiersFactory::requestId(identifier.toUInt64());

    if (loader && m_resourcesData->resourceType(requestId) == InspectorPageAgent::DocumentResource) {
        auto* frame = loader->frame();
        if (frame && frame->loader().documentLoader() && frame->document()) {
            m_resourcesData->addResourceSharedBuffer(requestId,
                frame->loader().documentLoader()->mainResourceData(),
                frame->document()->encoding());
        }
    }

    m_frontendDispatcher->loadingFailed(requestId, timestamp(), error.localizedDescription(), error.isCancellation());
}

void InspectorNetworkAgent::didLoadResourceFromMemoryCache(DocumentLoader* loader, CachedResource& resource)
{
    ASSERT(loader);
    if (!loader)
        return;

    auto identifier = ResourceLoaderIdentifier::generate();
    String requestId = IdentifiersFactory::requestId(identifier.toUInt64());
    Inspector::Protocol::Network::LoaderId loaderId = loaderIdentifier(loader);
    Inspector::Protocol::Network::FrameId frameId = frameIdentifier(loader);

    m_resourcesData->resourceCreated(requestId, loaderId, resource);

    auto initiatorObject = buildInitiatorObject(loader->frame() ? loader->frame()->document() : nullptr, &resource.resourceRequest());

    // FIXME: It would be ideal to generate the Network.Response with the MemoryCache source
    // instead of whatever ResourceResponse::Source the CachedResources's response has.
    // The frontend already knows for certain that this was served from the memory cache.

    m_frontendDispatcher->requestServedFromMemoryCache(requestId, frameId, loaderId, loader->url().string(), timestamp(), WTFMove(initiatorObject), buildObjectForCachedResource(&resource));
}

void InspectorNetworkAgent::setInitialScriptContent(ResourceLoaderIdentifier identifier, const String& sourceString)
{
    m_resourcesData->setResourceContent(IdentifiersFactory::requestId(identifier.toUInt64()), sourceString);
}

void InspectorNetworkAgent::didReceiveScriptResponse(ResourceLoaderIdentifier identifier)
{
    m_resourcesData->setResourceType(IdentifiersFactory::requestId(identifier.toUInt64()), InspectorPageAgent::ScriptResource);
}

void InspectorNetworkAgent::didReceiveThreadableLoaderResponse(ResourceLoaderIdentifier identifier, DocumentThreadableLoader& documentThreadableLoader)
{
    String initiatorType = documentThreadableLoader.options().initiatorType;
    if (initiatorType == cachedResourceRequestInitiatorTypes().fetch)
        m_resourcesData->setResourceType(IdentifiersFactory::requestId(identifier.toUInt64()), InspectorPageAgent::FetchResource);
    else if (initiatorType == cachedResourceRequestInitiatorTypes().xmlhttprequest)
        m_resourcesData->setResourceType(IdentifiersFactory::requestId(identifier.toUInt64()), InspectorPageAgent::XHRResource);
}

void InspectorNetworkAgent::willLoadXHRSynchronously()
{
    m_loadingXHRSynchronously = true;
}

void InspectorNetworkAgent::didLoadXHRSynchronously()
{
    m_loadingXHRSynchronously = false;
}

void InspectorNetworkAgent::willDestroyCachedResource(CachedResource& cachedResource)
{
    Vector<String> requestIds = m_resourcesData->removeCachedResource(&cachedResource);
    if (!requestIds.size())
        return;

    String content;
    bool base64Encoded;
    if (!InspectorNetworkAgent::cachedResourceContent(cachedResource, &content, &base64Encoded))
        return;

    for (auto& id : requestIds)
        m_resourcesData->setResourceContent(id, content, base64Encoded);
}

void InspectorNetworkAgent::willRecalculateStyle()
{
    m_isRecalculatingStyle = true;
}

void InspectorNetworkAgent::didRecalculateStyle()
{
    m_isRecalculatingStyle = false;
    m_styleRecalculationInitiator = nullptr;
}

void InspectorNetworkAgent::didScheduleStyleRecalculation(Document& document)
{
    if (!m_styleRecalculationInitiator)
        m_styleRecalculationInitiator = buildInitiatorObject(&document);
}

Ref<Inspector::Protocol::Network::Initiator> InspectorNetworkAgent::buildInitiatorObject(Document* document, const ResourceRequest* resourceRequest)
{
    // FIXME: Worker support.
    if (!isMainThread()) {
        return Inspector::Protocol::Network::Initiator::create()
            .setType(Inspector::Protocol::Network::Initiator::Type::Other)
            .release();
    }

    RefPtr<Inspector::Protocol::Network::Initiator> initiatorObject;

    Ref<ScriptCallStack> stackTrace = createScriptCallStack(JSExecState::currentState());
    if (stackTrace->size() > 0) {
        initiatorObject = Inspector::Protocol::Network::Initiator::create()
            .setType(Inspector::Protocol::Network::Initiator::Type::Script)
            .release();
        initiatorObject->setStackTrace(stackTrace->buildInspectorObject());
    } else if (document && document->scriptableDocumentParser()) {
        initiatorObject = Inspector::Protocol::Network::Initiator::create()
            .setType(Inspector::Protocol::Network::Initiator::Type::Parser)
            .release();
        initiatorObject->setUrl(document->url().string());
        initiatorObject->setLineNumber(document->scriptableDocumentParser()->textPosition().m_line.oneBasedInt());
    }

    auto domAgent = m_instrumentingAgents.persistentDOMAgent();
    if (domAgent && resourceRequest) {
        if (auto inspectorInitiatorNodeIdentifier = resourceRequest->inspectorInitiatorNodeIdentifier()) {
            if (!initiatorObject) {
                initiatorObject = Inspector::Protocol::Network::Initiator::create()
                    .setType(Inspector::Protocol::Network::Initiator::Type::Other)
                    .release();
            }

            initiatorObject->setNodeId(*inspectorInitiatorNodeIdentifier);
        }
    }

    if (initiatorObject)
        return initiatorObject.releaseNonNull();

    if (m_isRecalculatingStyle && m_styleRecalculationInitiator)
        return *m_styleRecalculationInitiator;

    return Inspector::Protocol::Network::Initiator::create()
        .setType(Inspector::Protocol::Network::Initiator::Type::Other)
        .release();
}

void InspectorNetworkAgent::didCreateWebSocket(WebSocketChannelIdentifier identifier, const URL& requestURL)
{
    m_frontendDispatcher->webSocketCreated(IdentifiersFactory::requestId(identifier.toUInt64()), requestURL.string());
}

void InspectorNetworkAgent::willSendWebSocketHandshakeRequest(WebSocketChannelIdentifier identifier, const ResourceRequest& request)
{
    auto requestObject = Inspector::Protocol::Network::WebSocketRequest::create()
        .setHeaders(buildObjectForHeaders(request.httpHeaderFields()))
        .release();
    m_frontendDispatcher->webSocketWillSendHandshakeRequest(IdentifiersFactory::requestId(identifier.toUInt64()), timestamp(), WallTime::now().secondsSinceEpoch().seconds(), WTFMove(requestObject));
}

void InspectorNetworkAgent::didReceiveWebSocketHandshakeResponse(WebSocketChannelIdentifier identifier, const ResourceResponse& response)
{
    auto responseObject = Inspector::Protocol::Network::WebSocketResponse::create()
        .setStatus(response.httpStatusCode())
        .setStatusText(response.httpStatusText())
        .setHeaders(buildObjectForHeaders(response.httpHeaderFields()))
        .release();
    m_frontendDispatcher->webSocketHandshakeResponseReceived(IdentifiersFactory::requestId(identifier.toUInt64()), timestamp(), WTFMove(responseObject));
}

void InspectorNetworkAgent::didCloseWebSocket(WebSocketChannelIdentifier identifier)
{
    m_frontendDispatcher->webSocketClosed(IdentifiersFactory::requestId(identifier.toUInt64()), timestamp());
}

void InspectorNetworkAgent::didReceiveWebSocketFrame(WebSocketChannelIdentifier identifier, const WebSocketFrame& frame)
{
    m_frontendDispatcher->webSocketFrameReceived(IdentifiersFactory::requestId(identifier.toUInt64()), timestamp(), buildWebSocketMessage(frame));
}
void InspectorNetworkAgent::didSendWebSocketFrame(WebSocketChannelIdentifier identifier, const WebSocketFrame& frame)
{
    m_frontendDispatcher->webSocketFrameSent(IdentifiersFactory::requestId(identifier.toUInt64()), timestamp(), buildWebSocketMessage(frame));
}

void InspectorNetworkAgent::didReceiveWebSocketFrameError(WebSocketChannelIdentifier identifier, const String& errorMessage)
{
    m_frontendDispatcher->webSocketFrameError(IdentifiersFactory::requestId(identifier.toUInt64()), timestamp(), errorMessage);
}

Inspector::Protocol::ErrorStringOr<void> InspectorNetworkAgent::enable()
{
    m_enabled = true;
    m_instrumentingAgents.setEnabledNetworkAgent(this);

    {
        Locker locker { WebSocket::allActiveWebSocketsLock() };

        for (auto* webSocket : activeWebSockets()) {
            RefPtr document = dynamicDowncast<Document>(webSocket->scriptExecutionContext());
            if (!document)
                continue;

            auto channel = webSocket->channel();

            auto identifier = channel->progressIdentifier();
            didCreateWebSocket(identifier, webSocket->url());

            auto cookieRequestHeaderFieldValue = [document = WeakPtr<Document, WeakPtrImplWithEventTargetData> { document }](const URL& url) -> String {
                if (!document || !document->page())
                    return { };
                return document->page()->cookieJar().cookieRequestHeaderFieldValue(*document, url);
            };
            willSendWebSocketHandshakeRequest(identifier, channel->clientHandshakeRequest(WTFMove(cookieRequestHeaderFieldValue)));

            if (channel->isConnected())
                didReceiveWebSocketHandshakeResponse(identifier, channel->serverHandshakeResponse());

            if (webSocket->readyState() == WebSocket::CLOSED)
                didCloseWebSocket(identifier);
        }
    }

    return { };
}

Inspector::Protocol::ErrorStringOr<void> InspectorNetworkAgent::disable()
{
    m_enabled = false;
    m_interceptionEnabled = false;
    m_intercepts.clear();
    m_instrumentingAgents.setEnabledNetworkAgent(nullptr);
    m_resourcesData->clear();
    m_extraRequestHeaders.clear();

    continuePendingRequests();
    continuePendingResponses();

    setResourceCachingDisabled(false);

#if ENABLE(INSPECTOR_NETWORK_THROTTLING)
    setEmulatedConditions(std::nullopt);
#endif

    return { };
}

bool InspectorNetworkAgent::shouldIntercept(URL url, Inspector::Protocol::Network::NetworkStage networkStage)
{
    url.removeFragmentIdentifier();

    String urlString = url.string();
    if (urlString.isEmpty())
        return false;

    for (auto& intercept : m_intercepts) {
        if (intercept.matches(urlString, networkStage))
            return true;
    }

    return false;
}

void InspectorNetworkAgent::continuePendingRequests()
{
    for (auto& pendingRequest : m_pendingInterceptRequests.values())
        pendingRequest->continueWithOriginalRequest();
    m_pendingInterceptRequests.clear();
}

void InspectorNetworkAgent::continuePendingResponses()
{
    for (auto& pendingInterceptResponse : m_pendingInterceptResponses.values())
        pendingInterceptResponse->respondWithOriginalResponse();
    m_pendingInterceptResponses.clear();
}

Inspector::Protocol::ErrorStringOr<void> InspectorNetworkAgent::setExtraHTTPHeaders(Ref<JSON::Object>&& headers)
{
    for (auto& entry : headers.get()) {
        auto stringValue = entry.value->asString();
        if (!!stringValue)
            m_extraRequestHeaders.set(entry.key, stringValue);
    }

    return { };
}

Inspector::Protocol::ErrorStringOr<std::tuple<String, bool /* base64Encoded */>> InspectorNetworkAgent::getResponseBody(const Inspector::Protocol::Network::RequestId& requestId)
{
    NetworkResourcesData::ResourceData const* resourceData = m_resourcesData->data(requestId);
    if (!resourceData)
        return makeUnexpected("Missing resource for given requestId"_s);

    if (resourceData->hasContent())
        return { { resourceData->content(), resourceData->base64Encoded() } };

    if (resourceData->isContentEvicted())
        return makeUnexpected("Resource content was evicted from inspector cache"_s);

    if (resourceData->buffer() && !resourceData->textEncodingName().isNull()) {
        String body;
        if (InspectorPageAgent::sharedBufferContent(resourceData->buffer(), resourceData->textEncodingName(), false, &body))
            return { { body, false } };
    }

    if (resourceData->cachedResource()) {
        String body;
        bool base64Encoded;
        if (InspectorNetworkAgent::cachedResourceContent(*resourceData->cachedResource(), &body, &base64Encoded))
            return { { body, base64Encoded } };
    }

    return makeUnexpected("Missing content of resource for given requestId"_s);
}

Inspector::Protocol::ErrorStringOr<void> InspectorNetworkAgent::setResourceCachingDisabled(bool disabled)
{
    setResourceCachingDisabledInternal(disabled);

    return { };
}

void InspectorNetworkAgent::loadResource(const Inspector::Protocol::Network::FrameId& frameId, const String& urlString, Ref<LoadResourceCallback>&& callback)
{
    Inspector::Protocol::ErrorString errorString;
    auto* context = scriptExecutionContext(errorString, frameId);
    if (!context) {
        callback->sendFailure(errorString);
        return;
    }

    URL url = context->completeURL(urlString);
    ResourceRequest request(WTFMove(url));
    request.setHTTPMethod("GET"_s);
    request.setHiddenFromInspector(true);

    ThreadableLoaderOptions options;
    options.sendLoadCallbacks = SendCallbackPolicy::SendCallbacks; // So we remove this from m_hiddenRequestIdentifiers on completion.
    options.defersLoadingPolicy = DefersLoadingPolicy::DisallowDefersLoading; // So the request is never deferred.
    options.mode = FetchOptions::Mode::NoCors;
    options.credentials = FetchOptions::Credentials::SameOrigin;
    options.contentSecurityPolicyEnforcement = ContentSecurityPolicyEnforcement::DoNotEnforce;

    // InspectorThreadableLoaderClient deletes itself when the load completes or fails.
    InspectorThreadableLoaderClient* inspectorThreadableLoaderClient = new InspectorThreadableLoaderClient(callback.copyRef());
    auto loader = ThreadableLoader::create(*context, *inspectorThreadableLoaderClient, WTFMove(request), options);
    if (!loader) {
        callback->sendFailure("Could not load requested resource."_s);
        return;
    }

    // If the load already completed, inspectorThreadableLoaderClient will have been deleted and we will have already called the callback.
    if (!callback->isActive())
        return;

    inspectorThreadableLoaderClient->setLoader(WTFMove(loader));
}

Inspector::Protocol::ErrorStringOr<String> InspectorNetworkAgent::getSerializedCertificate(const Inspector::Protocol::Network::RequestId& requestId)
{
    auto* resourceData = m_resourcesData->data(requestId);
    if (!resourceData)
        return makeUnexpected("Missing resource for given requestId"_s);

    auto& certificate = resourceData->certificateInfo();
    if (!certificate || certificate.value().isEmpty())
        return makeUnexpected("Missing certificate of resource for given requestId"_s);

    WTF::Persistence::Encoder encoder;
    WTF::Persistence::Coder<WebCore::CertificateInfo>::encodeForPersistence(encoder, certificate.value());
    return base64EncodeToString(encoder.span());
}

WebSocket* InspectorNetworkAgent::webSocketForRequestId(const Inspector::Protocol::Network::RequestId& requestId)
{
    Locker locker { WebSocket::allActiveWebSocketsLock() };

    for (auto* webSocket : activeWebSockets()) {
        if (IdentifiersFactory::requestId(webSocket->channel()->progressIdentifier().toUInt64()) == requestId)
            return webSocket;
    }

    return nullptr;
}

static JSC::JSValue webSocketAsScriptValue(JSC::JSGlobalObject& state, WebSocket* webSocket)
{
    JSC::JSLockHolder lock(&state);
    return toJS(&state, deprecatedGlobalObjectForPrototype(&state), webSocket);
}

Inspector::Protocol::ErrorStringOr<Ref<Inspector::Protocol::Runtime::RemoteObject>> InspectorNetworkAgent::resolveWebSocket(const Inspector::Protocol::Network::RequestId& requestId, const String& objectGroup)
{
    WebSocket* webSocket = webSocketForRequestId(requestId);
    if (!webSocket)
        return makeUnexpected("Missing web socket for given requestId"_s);

    // FIXME: <https://webkit.org/b/168475> Web Inspector: Correctly display iframe's and worker's WebSockets
    RefPtr document = dynamicDowncast<Document>(webSocket->scriptExecutionContext());
    if (!document)
        return makeUnexpected("Not supported"_s);

    auto* frame = document->frame();
    if (!frame)
        return makeUnexpected("Missing frame of web socket for given requestId"_s);

    auto& globalObject = mainWorldGlobalObject(*frame);
    auto injectedScript = m_injectedScriptManager.injectedScriptFor(&globalObject);
    ASSERT(!injectedScript.hasNoValue());

    auto object = injectedScript.wrapObject(webSocketAsScriptValue(globalObject, webSocket), objectGroup);
    if (!object)
        return makeUnexpected("Internal error: unable to cast WebSocket"_s);

    return object.releaseNonNull();
}

Inspector::Protocol::ErrorStringOr<void> InspectorNetworkAgent::setInterceptionEnabled(bool enabled)
{
    if (m_interceptionEnabled == enabled)
        return makeUnexpected(m_interceptionEnabled ? "Interception already enabled"_s : "Interception already disabled"_s);

    m_interceptionEnabled = enabled;

    if (!m_interceptionEnabled) {
        continuePendingRequests();
        continuePendingResponses();
    }

    return { };
}

Inspector::Protocol::ErrorStringOr<void> InspectorNetworkAgent::addInterception(const String& url, Inspector::Protocol::Network::NetworkStage networkStage, std::optional<bool>&& caseSensitive, std::optional<bool>&& isRegex)
{
    Intercept intercept;
    intercept.url = url;
    if (caseSensitive)
        intercept.caseSensitive = *caseSensitive;
    if (isRegex)
        intercept.isRegex = *isRegex;
    intercept.networkStage = networkStage;

    if (!m_intercepts.appendIfNotContains(intercept))
        return makeUnexpected("Intercept for given url, given isRegex, and given stage already exists"_s);

    return { };
}

Inspector::Protocol::ErrorStringOr<void> InspectorNetworkAgent::removeInterception(const String& url, Inspector::Protocol::Network::NetworkStage networkStage, std::optional<bool>&& caseSensitive, std::optional<bool>&& isRegex)
{
    Intercept intercept;
    intercept.url = url;
    if (caseSensitive)
        intercept.caseSensitive = *caseSensitive;
    if (isRegex)
        intercept.isRegex = *isRegex;
    intercept.networkStage = networkStage;

    if (!m_intercepts.removeAll(intercept))
        return makeUnexpected("Missing intercept for given url, given isRegex, and given stage"_s);

    return { };
}

bool InspectorNetworkAgent::willIntercept(const ResourceRequest& request)
{
    if (!m_interceptionEnabled)
        return false;

    return shouldIntercept(request.url(), Inspector::Protocol::Network::NetworkStage::Request)
        || shouldIntercept(request.url(), Inspector::Protocol::Network::NetworkStage::Response);
}

bool InspectorNetworkAgent::shouldInterceptRequest(const ResourceLoader& loader)
{
    if (!m_interceptionEnabled)
        return false;

    if (loader.options().serviceWorkerRegistrationIdentifier)
        return false;

    return shouldIntercept(loader.url(), Inspector::Protocol::Network::NetworkStage::Request);
}

bool InspectorNetworkAgent::shouldInterceptResponse(const ResourceResponse& response)
{
    if (!m_interceptionEnabled)
        return false;

    return shouldIntercept(response.url(), Inspector::Protocol::Network::NetworkStage::Response);
}

void InspectorNetworkAgent::interceptRequest(ResourceLoader& loader, Function<void(const ResourceRequest&)>&& handler)
{
    ASSERT(m_enabled);
    ASSERT(m_interceptionEnabled);

    String requestId = IdentifiersFactory::requestId(loader.identifier()->toUInt64());
    if (m_pendingInterceptRequests.contains(requestId)) {
        handler(loader.request());
        return;
    }
    m_pendingInterceptRequests.set(requestId, makeUnique<PendingInterceptRequest>(&loader, WTFMove(handler)));
    m_frontendDispatcher->requestIntercepted(requestId, buildObjectForResourceRequest(loader.request(), &loader));
}

void InspectorNetworkAgent::interceptResponse(const ResourceResponse& response, ResourceLoaderIdentifier identifier, CompletionHandler<void(const ResourceResponse&, RefPtr<FragmentedSharedBuffer>)>&& handler)
{
    ASSERT(m_enabled);
    ASSERT(m_interceptionEnabled);

    String requestId = IdentifiersFactory::requestId(identifier.toUInt64());
    if (m_pendingInterceptResponses.contains(requestId)) {
        ASSERT_NOT_REACHED();
        handler(response, nullptr);
        return;
    }

    m_pendingInterceptResponses.set(requestId, makeUnique<PendingInterceptResponse>(response, WTFMove(handler)));

    auto resourceResponse = buildObjectForResourceResponse(response, nullptr);
    if (!resourceResponse)
        return;

    m_frontendDispatcher->responseIntercepted(requestId, resourceResponse.releaseNonNull());
}

Inspector::Protocol::ErrorStringOr<void> InspectorNetworkAgent::interceptContinue(const Inspector::Protocol::Network::RequestId& requestId, Inspector::Protocol::Network::NetworkStage networkStage)
{
    switch (networkStage) {
    case Inspector::Protocol::Network::NetworkStage::Request:
        if (auto pendingInterceptRequest = m_pendingInterceptRequests.take(requestId)) {
            pendingInterceptRequest->continueWithOriginalRequest();
            return { };
        }
        return makeUnexpected("Missing pending intercept request for given requestId"_s);

    case Inspector::Protocol::Network::NetworkStage::Response:
        if (auto pendingInterceptResponse = m_pendingInterceptResponses.take(requestId)) {
            pendingInterceptResponse->respondWithOriginalResponse();
            return { };
        }
        return makeUnexpected("Missing pending intercept response for given requestId"_s);
    }

    ASSERT_NOT_REACHED();
    return { };
}

Inspector::Protocol::ErrorStringOr<void> InspectorNetworkAgent::interceptWithRequest(const Inspector::Protocol::Network::RequestId& requestId, const String& url, const String& method, RefPtr<JSON::Object>&& headers, const String& postData)
{
    auto pendingRequest = m_pendingInterceptRequests.take(requestId);
    if (!pendingRequest)
        return makeUnexpected("Missing pending intercept request for given requestId"_s);

    auto& loader = *pendingRequest->m_loader;
    ResourceRequest request = loader.request();
    if (!!url)
        request.setURL(URL({ }, url));
    if (!!method)
        request.setHTTPMethod(method);
    if (headers) {
        HTTPHeaderMap explicitHeaders;
        for (auto& [key, value] : *headers) {
            auto headerValue = value->asString();
            if (!!headerValue)
                explicitHeaders.add(key, headerValue);
        }
        request.setHTTPHeaderFields(WTFMove(explicitHeaders));
    }
    if (!!postData) {
        auto buffer = base64Decode(postData);
        if (!buffer)
            return makeUnexpected("Unable to decode given postData"_s);

        request.setHTTPBody(FormData::create(WTFMove(*buffer)));
    }
    // FIXME: figure out how to identify when a request has been overridden when we add this to the frontend.
    pendingRequest->continueWithRequest(request);

    return { };
}

Inspector::Protocol::ErrorStringOr<void> InspectorNetworkAgent::interceptWithResponse(const Inspector::Protocol::Network::RequestId& requestId, const String& content, bool base64Encoded, const String& mimeType, std::optional<int>&& status, const String& statusText, RefPtr<JSON::Object>&& headers)
{
    auto pendingInterceptResponse = m_pendingInterceptResponses.take(requestId);
    if (!pendingInterceptResponse)
        return makeUnexpected("Missing pending intercept response for given requestId"_s);

    ResourceResponse overrideResponse(pendingInterceptResponse->originalResponse());
    overrideResponse.setSource(ResourceResponse::Source::InspectorOverride);

    if (status)
        overrideResponse.setHTTPStatusCode(*status);
    if (!!statusText)
        overrideResponse.setHTTPStatusText(String { statusText });
    if (!!mimeType)
        overrideResponse.setMimeType(String { mimeType });
    if (headers) {
        HTTPHeaderMap explicitHeaders;
        for (auto& header : *headers) {
            auto headerValue = header.value->asString();
            if (!!headerValue)
                explicitHeaders.add(header.key, headerValue);
        }
        overrideResponse.setHTTPHeaderFields(WTFMove(explicitHeaders));
        overrideResponse.setHTTPHeaderField(HTTPHeaderName::ContentType, overrideResponse.mimeType());
    }

    RefPtr<FragmentedSharedBuffer> overrideData;
    if (base64Encoded) {
        auto buffer = base64Decode(content);
        if (!buffer)
            return makeUnexpected("Unable to decode given content"_s);

        overrideData = SharedBuffer::create(WTFMove(*buffer));
    } else
        overrideData = SharedBuffer::create(content.utf8().span());

    pendingInterceptResponse->respond(overrideResponse, overrideData);

    return { };
}

Inspector::Protocol::ErrorStringOr<void> InspectorNetworkAgent::interceptRequestWithResponse(const Inspector::Protocol::Network::RequestId& requestId, const String& content, bool base64Encoded, const String& mimeType, int status, const String& statusText, Ref<JSON::Object>&& headers)
{
    auto pendingRequest = m_pendingInterceptRequests.take(requestId);
    if (!pendingRequest)
        return makeUnexpected("Missing pending intercept request for given requestId"_s);

    // Loader will be retained in the didReceiveResponse lambda below.
    RefPtr<ResourceLoader> loader = pendingRequest->m_loader.get();
    if (loader->reachedTerminalState())
        return makeUnexpected("Unable to fulfill request, it has already been processed"_s);

    RefPtr<SharedBuffer> data;
    if (base64Encoded) {
        auto buffer = base64Decode(content);
        if (!buffer)
            return makeUnexpected("Unable to decode given content"_s);

        data = SharedBuffer::create(WTFMove(*buffer));
    } else
        data = SharedBuffer::create(content.utf8().span());

    // Mimic data URL load behavior - report didReceiveResponse & didFinishLoading.
    ResourceResponse response(URL { pendingRequest->m_loader->url() }, String { mimeType }, data->size(), String());
    response.setSource(ResourceResponse::Source::InspectorOverride);
    response.setHTTPStatusCode(status);
    response.setHTTPStatusText(String { statusText });
    HTTPHeaderMap explicitHeaders;
    for (auto& header : headers.get()) {
        auto headerValue = header.value->asString();
        if (!!headerValue)
            explicitHeaders.add(header.key, headerValue);
    }
    response.setHTTPHeaderFields(WTFMove(explicitHeaders));
    response.setHTTPHeaderField(HTTPHeaderName::ContentType, response.mimeType());
    loader->didReceiveResponse(WTFMove(response), [loader, buffer = data.releaseNonNull()]() {
        if (loader->reachedTerminalState())
            return;

        if (buffer->size())
            loader->didReceiveData(buffer, buffer->size(), DataPayloadWholeResource);
        loader->didFinishLoading(NetworkLoadMetrics());
    });

    return { };
}

static ResourceError::Type toResourceErrorType(Inspector::Protocol::Network::ResourceErrorType protocolResourceErrorType)
{
    switch (protocolResourceErrorType) {
    case Inspector::Protocol::Network::ResourceErrorType::General:
        return ResourceError::Type::General;

    case Inspector::Protocol::Network::ResourceErrorType::AccessControl:
        return ResourceError::Type::AccessControl;

    case Inspector::Protocol::Network::ResourceErrorType::Cancellation:
        return ResourceError::Type::Cancellation;

    case Inspector::Protocol::Network::ResourceErrorType::Timeout:
        return ResourceError::Type::Timeout;
    }

    ASSERT_NOT_REACHED();
    return ResourceError::Type::Null;
}

Inspector::Protocol::ErrorStringOr<void> InspectorNetworkAgent::interceptRequestWithError(const Inspector::Protocol::Network::RequestId& requestId, Inspector::Protocol::Network::ResourceErrorType errorType)
{
    auto pendingRequest = m_pendingInterceptRequests.take(requestId);
    if (!pendingRequest)
        return makeUnexpected("Missing pending intercept request for given requestId"_s);

    auto& loader = *pendingRequest->m_loader;
    if (loader.reachedTerminalState())
        return makeUnexpected("Unable to abort request, it has already been processed"_s);

    addConsoleMessage(makeUnique<Inspector::ConsoleMessage>(MessageSource::Network, MessageType::Log, MessageLevel::Info, makeString("Web Inspector blocked "_s, loader.url().string(), " from loading"_s), loader.identifier() ? loader.identifier()->toUInt64() : 0));

    loader.didFail(ResourceError(InspectorNetworkAgent::errorDomain(), 0, loader.url(), "Blocked by Web Inspector"_s, toResourceErrorType(errorType)));
    return { };
}

#if ENABLE(INSPECTOR_NETWORK_THROTTLING)

Inspector::Protocol::ErrorStringOr<void> InspectorNetworkAgent::setEmulatedConditions(std::optional<int>&& bytesPerSecondLimit)
{
    if (bytesPerSecondLimit && *bytesPerSecondLimit < 0)
        return makeUnexpected("bytesPerSecond cannot be negative"_s);

    if (setEmulatedConditionsInternal(WTFMove(bytesPerSecondLimit)))
        return { };

    return makeUnexpected("Not supported"_s);
}

#endif // ENABLE(INSPECTOR_NETWORK_THROTTLING)

bool InspectorNetworkAgent::shouldTreatAsText(const String& mimeType)
{
    return startsWithLettersIgnoringASCIICase(mimeType, "text/"_s)
        || MIMETypeRegistry::isSupportedJavaScriptMIMEType(mimeType)
        || MIMETypeRegistry::isSupportedJSONMIMEType(mimeType)
        || MIMETypeRegistry::isXMLMIMEType(mimeType)
        || MIMETypeRegistry::isTextMediaPlaylistMIMEType(mimeType);
}

Ref<TextResourceDecoder> InspectorNetworkAgent::createTextDecoder(const String& mimeType, const String& textEncodingName)
{
    if (!textEncodingName.isEmpty())
        return TextResourceDecoder::create("text/plain"_s, textEncodingName);

    if (MIMETypeRegistry::isTextMIMEType(mimeType))
        return TextResourceDecoder::create(mimeType, "UTF-8"_s);

    if (MIMETypeRegistry::isXMLMIMEType(mimeType)) {
        auto decoder = TextResourceDecoder::create("application/xml"_s);
        decoder->useLenientXMLDecoding();
        return decoder;
    }

    return TextResourceDecoder::create("text/plain"_s, "UTF-8"_s);
}

std::optional<String> InspectorNetworkAgent::textContentForCachedResource(CachedResource& cachedResource)
{
    if (!InspectorNetworkAgent::shouldTreatAsText(cachedResource.mimeType()))
        return std::nullopt;

    String result;
    bool base64Encoded;
    if (InspectorNetworkAgent::cachedResourceContent(cachedResource, &result, &base64Encoded)) {
        ASSERT(!base64Encoded);
        return result;
    }

    return std::nullopt;
}

bool InspectorNetworkAgent::cachedResourceContent(CachedResource& resource, String* result, bool* base64Encoded)
{
    ASSERT(result);
    ASSERT(base64Encoded);

    if (!resource.encodedSize()) {
        *base64Encoded = false;
        *result = String();
        return true;
    }

    switch (resource.type()) {
    case CachedResource::Type::CSSStyleSheet:
        *base64Encoded = false;
        *result = downcast<CachedCSSStyleSheet>(resource).sheetText();
        // The above can return a null String if the MIME type is invalid.
        return !result->isNull();
    case CachedResource::Type::Script:
        *base64Encoded = false;
        *result = downcast<CachedScript>(resource).script().toString();
        return true;
    default:
        auto* buffer = resource.resourceBuffer();
        if (!buffer)
            return false;

        if (InspectorNetworkAgent::shouldTreatAsText(resource.mimeType())) {
            auto decoder = InspectorNetworkAgent::createTextDecoder(resource.mimeType(), resource.response().textEncodingName());
            *base64Encoded = false;
            *result = decoder->decodeAndFlush(buffer->makeContiguous()->span());
            return true;
        }

        *base64Encoded = true;
        *result = base64EncodeToString(buffer->makeContiguous()->span());
        return true;
    }
}

static Ref<Inspector::Protocol::Page::SearchResult> buildObjectForSearchResult(const Inspector::Protocol::Network::RequestId& requestId, const Inspector::Protocol::Network::FrameId& frameId, const String& url, int matchesCount)
{
    auto searchResult = Inspector::Protocol::Page::SearchResult::create()
        .setUrl(url)
        .setFrameId(frameId)
        .setMatchesCount(matchesCount)
        .release();
    searchResult->setRequestId(requestId);
    return searchResult;
}

static std::optional<String> textContentForResourceData(const NetworkResourcesData::ResourceData& resourceData)
{
    if (resourceData.hasContent() && !resourceData.base64Encoded())
        return resourceData.content();

    if (resourceData.cachedResource())
        return InspectorNetworkAgent::textContentForCachedResource(*resourceData.cachedResource());

    return std::nullopt;
}

void InspectorNetworkAgent::searchOtherRequests(const JSC::Yarr::RegularExpression& regex, Ref<JSON::ArrayOf<Inspector::Protocol::Page::SearchResult>>& result)
{
    Vector<NetworkResourcesData::ResourceData*> resources = m_resourcesData->resources();
    for (auto* resourceData : resources) {
        if (auto textContent = textContentForResourceData(*resourceData)) {
            int matchesCount = ContentSearchUtilities::countRegularExpressionMatches(regex, resourceData->content());
            if (matchesCount)
                result->addItem(buildObjectForSearchResult(resourceData->requestId(), resourceData->frameId(), resourceData->url(), matchesCount));
        }
    }
}

void InspectorNetworkAgent::searchInRequest(Inspector::Protocol::ErrorString& errorString, const Inspector::Protocol::Network::RequestId& requestId, const String& query, bool caseSensitive, bool isRegex, RefPtr<JSON::ArrayOf<Inspector::Protocol::GenericTypes::SearchMatch>>& results)
{
    NetworkResourcesData::ResourceData const* resourceData = m_resourcesData->data(requestId);
    if (!resourceData) {
        errorString = "Missing resource for given requestId"_s;
        return;
    }

    if (!resourceData->hasContent()) {
        errorString = "Missing content of resource for given requestId"_s;
        return;
    }

    results = ContentSearchUtilities::searchInTextByLines(resourceData->content(), query, caseSensitive, isRegex);
}

void InspectorNetworkAgent::mainFrameNavigated(DocumentLoader& loader)
{
    m_resourcesData->clear(loaderIdentifier(&loader));
}

bool InspectorNetworkAgent::Intercept::matches(const String& url, Inspector::Protocol::Network::NetworkStage networkStage)
{
    if (this->networkStage != networkStage)
        return false;

    if (this->url.isEmpty())
        return true;

    if (m_knownMatchingURLs.contains(url))
        return true;

    if (!m_urlSearcher) {
        auto searchType = isRegex ? ContentSearchUtilities::SearchType::Regex : ContentSearchUtilities::SearchType::ExactString;
        auto searchCaseSensitive = caseSensitive ? ContentSearchUtilities::SearchCaseSensitive::Yes : ContentSearchUtilities::SearchCaseSensitive::No;
        m_urlSearcher = ContentSearchUtilities::createSearcherForString(this->url, searchType, searchCaseSensitive);
    }
    if (!ContentSearchUtilities::searcherMatchesText(*m_urlSearcher, url))
        return false;

    m_knownMatchingURLs.add(url);
    return true;
}

} // namespace WebCore
