/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(MODEL_PROCESS)

#include "AuxiliaryProcess.h"
#include "SandboxExtension.h"
#include "SharedPreferencesForWebProcess.h"
#include <WebCore/ProcessIdentifier.h>
#include <WebCore/Timer.h>
#include <pal/SessionID.h>
#include <wtf/Function.h>
#include <wtf/MemoryPressureHandler.h>
#include <wtf/MonotonicTime.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakPtr.h>

#if PLATFORM(VISION) && ENABLE(GPU_PROCESS)
namespace IPC {
class SharedFileHandle;
}
#endif

namespace WebKit {

class ModelConnectionToWebProcess;
struct ModelProcessConnectionParameters;
struct ModelProcessCreationParameters;

class ModelProcess final : public AuxiliaryProcess, public ThreadSafeRefCounted<ModelProcess> {
    WTF_MAKE_NONCOPYABLE(ModelProcess);
    WTF_MAKE_TZONE_ALLOCATED(ModelProcess);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(ModelProcess);
public:
    explicit ModelProcess(AuxiliaryProcessInitializationParameters&&);
    ~ModelProcess();
    static constexpr WTF::AuxiliaryProcessType processType = WTF::AuxiliaryProcessType::Model;

    void ref() const final { ThreadSafeRefCounted::ref(); }
    void deref() const final { ThreadSafeRefCounted::deref(); }

    void removeModelConnectionToWebProcess(ModelConnectionToWebProcess&);

    void prepareToSuspend(bool isSuspensionImminent, MonotonicTime estimatedSuspendTime, CompletionHandler<void()>&&);
    void processDidResume();
    void resume();

    void connectionToWebProcessClosed(IPC::Connection&);

    ModelConnectionToWebProcess* webProcessConnection(WebCore::ProcessIdentifier) const;

    void tryExitIfUnusedAndUnderMemoryPressure();

    const String& applicationVisibleName() const { return m_applicationVisibleName; }

#if PLATFORM(VISION) && ENABLE(GPU_PROCESS)
    void requestSharedSimulationConnection(WebCore::ProcessIdentifier, CompletionHandler<void(std::optional<IPC::SharedFileHandle>)>&&);
#endif
    std::optional<int> debugEntityMemoryLimit() const { return m_debugEntityMemoryLimit; }

    void webProcessConnectionCountForTesting(CompletionHandler<void(uint64_t)>&&);
    void modelPlayerCountForTesting(CompletionHandler<void(uint64_t)>&&);

private:
    void lowMemoryHandler(Critical, Synchronous);

    // AuxiliaryProcess
    void initializeProcess(const AuxiliaryProcessInitializationParameters&) override;
    void initializeProcessName(const AuxiliaryProcessInitializationParameters&) override;
    void initializeSandbox(const AuxiliaryProcessInitializationParameters&, SandboxInitializationParameters&) override;
    bool shouldTerminate() override;

    void tryExitIfUnused();
    bool canExitUnderMemoryPressure() const;

    // IPC::Connection::Client
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

    // Message Handlers
    void initializeModelProcess(ModelProcessCreationParameters&&, CompletionHandler<void()>&&);
    void createModelConnectionToWebProcess(WebCore::ProcessIdentifier, PAL::SessionID, IPC::Connection::Handle&&, ModelProcessConnectionParameters&&, const std::optional<String>& attributionTaskID, CompletionHandler<void()>&&);
    void sharedPreferencesForWebProcessDidChange(WebCore::ProcessIdentifier, SharedPreferencesForWebProcess&&, CompletionHandler<void()>&&);
    void addSession(PAL::SessionID);
    void removeSession(PAL::SessionID);

#if ENABLE(CFPREFS_DIRECT_MODE)
    void dispatchSimulatedNotificationsForPreferenceChange(const String& key) final;
#endif

    // Connections to WebProcesses.
    HashMap<WebCore::ProcessIdentifier, Ref<ModelConnectionToWebProcess>> m_webProcessConnections;
    MonotonicTime m_creationTime { MonotonicTime::now() };

    HashSet<PAL::SessionID> m_sessions;

    WebCore::Timer m_idleExitTimer;
    String m_applicationVisibleName;
    std::optional<int> m_debugEntityMemoryLimit;
};

} // namespace WebKit

#endif // ENABLE(MODEL_PROCESS)
