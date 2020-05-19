#ifndef __RAPTOR_PLUGIN_PROTOCOL__
#define __RAPTOR_PLUGIN_PROTOCOL__

#include <stddef.h>
#include "core/slice/slice.h"

namespace raptor {
class Protocol {
public:
    Protocol() : HeaderSize(0) {}
    virtual ~Protocol() {}

    // Before sending data, you need to build a header
    virtual Slice BuildPackageHeader(size_t pack_len) = 0;

    // return -1: error;  0: need more data; > 0 : pack_len
    virtual int CheckPackageLength(const void* buf, size_t len) = 0;

    // protocol chain
    virtual bool OnConnected();
    virtual bool OnDataParsing();

public:
    size_t HeaderSize;
};
} // namespace raptor
#endif  // __RAPTOR_PLUGIN_PROTOCOL__
