#ifndef __RAPTOR_CORE_WINDOWS_IOCP_THREAD__
#define __RAPTOR_CORE_WINDOWS_IOCP_THREAD__

#include "core/windows/iocp.h"
#include "util/thread.h"

namespace raptor {
class IIocpReceiver {
public:
    virtual ~IIocpReceiver() {}
    virtual void OnRecvEvent(void* ptr, size_t transferred_bytes) = 0;
    virtual void OnSendEvent(void* ptr, size_t transferred_bytes) = 0;
};

class IocpThread {
public:
    explicit IocpThread(IIocpReceiver* service);
    ~IocpThread();
    bool Init(size_t rs_threads, size_t kernel_threads);
    void Start();
    void Shutdown();

private:
    void WorkThread();

    IIocpReceiver* _service;
    bool _shutdown;
    size_t _rs_threads;
    Thread* _threads;

    OVERLAPPED _exit;
    Iocp _iocp;
};
} // namespace raptor


#endif  // __RAPTOR_CORE_WINDOWS_IOCP_THREAD__
