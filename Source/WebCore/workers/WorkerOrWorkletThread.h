/*
 * Copyright (C) 2008-2025 Apple Inc. All rights reserved.
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

#include "WorkerRunLoop.h"
#include "WorkerThreadMode.h"
#include <wtf/Forward.h>
#include <wtf/Function.h>
#include <wtf/FunctionDispatcher.h>
#include <wtf/Lock.h>
#include <wtf/RefPtr.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/ThreadSafeWeakHashSet.h>
#include <wtf/ThreadSafeWeakPtr.h>
#include <wtf/threads/BinarySemaphore.h>

namespace WTF {
class Thread;
}

namespace WebCore {

class WorkerDebuggerProxy;
class WorkerLoaderProxy;

class WorkerOrWorkletThread : public SerialFunctionDispatcher {
public:
    virtual ~WorkerOrWorkletThread();

    // SerialFunctionDispatcher methods
    void dispatch(Function<void()>&&) final;
    bool isCurrent() const final;

    Thread* thread() const { return m_thread.get(); }

    virtual void clearProxies() = 0;

    virtual WorkerDebuggerProxy* workerDebuggerProxy() const = 0;
    virtual WorkerLoaderProxy* workerLoaderProxy() const = 0;
    virtual CheckedPtr<WorkerLoaderProxy> checkedWorkerLoaderProxy() const;

    WorkerOrWorkletGlobalScope* globalScope() const { return m_globalScope.get(); }
    RefPtr<WorkerOrWorkletGlobalScope> protectedGlobalScope() const;
    WorkerRunLoop& runLoop() { return m_runLoop; }

    void start(Function<void(const String&)>&& evaluateCallback = { });
    void stop(Function<void()>&& terminatedCallback = { });

    void startRunningDebuggerTasks();
    void stopRunningDebuggerTasks();

    void suspend();
    void resume();

    const String& inspectorIdentifier() const { return m_inspectorIdentifier; }

    static ThreadSafeWeakHashSet<WorkerOrWorkletThread>& workerOrWorkletThreads();
    static void releaseFastMallocFreeMemoryInAllThreads();

    void addChildThread(WorkerOrWorkletThread&);
    void removeChildThread(WorkerOrWorkletThread&);

protected:
    explicit WorkerOrWorkletThread(const String& inspectorIdentifier, WorkerThreadMode = WorkerThreadMode::CreateNewThread);
    void workerOrWorkletThread();

    // Executes the event loop for the worker thread. Derived classes can override to perform actions before/after entering the event loop.
    virtual void runEventLoop();

private:
    virtual Ref<Thread> createThread() = 0;
    virtual RefPtr<WorkerOrWorkletGlobalScope> createGlobalScope() = 0;
    virtual void evaluateScriptIfNecessary(String&) { }
    virtual bool shouldWaitForWebInspectorOnStartup() const { return false; }
    void destroyWorkerGlobalScope(Ref<WorkerOrWorkletThread>&& protectedThis);

    String m_inspectorIdentifier;
    Lock m_threadCreationAndGlobalScopeLock;
    RefPtr<WorkerOrWorkletGlobalScope> m_globalScope;
    RefPtr<Thread> m_thread;
    const UniqueRef<WorkerRunLoop> m_runLoop;
    Function<void(const String&)> m_evaluateCallback;
    Function<void()> m_stoppedCallback;
    BinarySemaphore m_suspensionSemaphore;
    ThreadSafeWeakHashSet<WorkerOrWorkletThread> m_childThreads;
    Function<void()> m_runWhenLastChildThreadIsGone;
    bool m_isSuspended { false };
    bool m_pausedForDebugger { false };
};

} // namespace WebCore
