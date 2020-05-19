#include "core/windows/iocp_thread.h"
#include "util/log.h"

namespace raptor {
IocpThread::IocpThread(IIocpReceiver* service)
    : _service(service)
    , _shutdown(true)
    , _rs_threads(0)
    , _threads(nullptr) {}

IocpThread::~IocpThread() {
    if (!_shutdown) {
        Shutdown();
    }
}

bool IocpThread::Init(size_t rs_threads, size_t kernel_threads) {
    if (!_shutdown) false;
    if (!_iocp.create(kernel_threads)) {
        return false;
    }
    _shutdown = false;
    _rs_threads = rs_threads;
    _threads = new Thread[rs_threads];
    for (size_t i = 0; i < rs_threads; i++) {
        _threads[i] = Thread("rs-thread",
            [](void* param) ->void {
                IocpThread* p = (IocpThread*)param;
                p->WorkThread();
            },
            this);
    }
    return true;
}

void IocpThread::Start() {
    if (_shutdown) return;
    RAPTOR_ASSERT(_threads != nullptr);
    for (size_t i = 0; i < _rs_threads; i++) {
        _threads[i].Start();
    }
}

void IocpThread::Shutdown() {
    if (!_shutdown) {
        _shutdown = true;
        _iocp.post(NULL, &_exit);
        for (size_t i = 0; i < _rs_threads; i++) {
            _threads[i].Join();
        }
        delete[] _threads;
        _threads = nullptr;
    }
}

void IocpThread::WorkThread(){
    while (!_shutdown) {
        DWORD NumberOfBytesTransferred = 0;
        void* CompletionKey = NULL;
        LPOVERLAPPED lpOverlapped = NULL;
        bool ret = _iocp.polling(
            &NumberOfBytesTransferred, (PULONG_PTR)&CompletionKey, &lpOverlapped, INFINITE);

        if (!ret) {
            continue;
        }

        if(lpOverlapped == &_exit) {  // shutdown
            break;
        }

        OverLappedEx* ptr = (OverLappedEx*)lpOverlapped;
        if (ptr->event == IocpEventType::kRecvEvent) {
            _service->OnRecvEvent(CompletionKey, NumberOfBytesTransferred);
        }
        if (ptr->event == IocpEventType::kSendEvent) {
            _service->OnSendEvent(CompletionKey, NumberOfBytesTransferred);
        }
    }
}

} // namespace raptor
