# raptor

一个简易的 C++ 网络库，目前有 EPOLL 和 IOCP 两种实现。

框架示图：

![架构示图](image/frame.png)



### 使用事项

raptor 提供 C++ 接口和兼容 C 的函数接口（include/c.h）。使用时需要使用者提供完整的协议支持，raptor本身没有协议，只会用其中两个协议相关的函数（include/protocol.h）用来解析协议的数据包。所以在调用 raptor 发送接口时，也要确保该数据的头部已经包含协议头信息。



### TODO

待完成事项

* raptor 内部的可配置参数的实现
* example 与文档
* 测试用例
* 基于 kqueue 的实现

