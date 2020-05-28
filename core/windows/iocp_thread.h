#ifndef __RAPTOR_CORE_WINDOWS_IOCP_THREAD__
#define __RAPTOR_CORE_WINDOWS_IOCP_THREAD__

#include "core/windows/iocp.h"
#include "core/service.h"
#include "util/status.h"
#include "util/thread.h"

namespace raptor {
class SendRecvThread {
public:
    explicit SendRecvThread(internal::IIocpReceiver* service);
    ~SendRecvThread();
    RefCountedPtr<Status> Init(size_t rs_threads, size_t kernel_threads);
    bool Start();
    void Shutdown();

private:
    void WorkThread();

    internal::IIocpReceiver* _service;
    bool _shutdown;
    size_t _rs_threads;
    Thread* _threads;

    OVERLAPPED _exit;
    Iocp _iocp;
};
} // namespace raptor


#endif  // __RAPTOR_CORE_WINDOWS_IOCP_THREAD__
