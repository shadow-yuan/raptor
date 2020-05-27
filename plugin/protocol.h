#ifndef __RAPTOR_PLUGIN_PROTOCOL__
#define __RAPTOR_PLUGIN_PROTOCOL__

#include <stddef.h>
#include "raptor/slice.h"

namespace raptor {
class Protocol {
public:
    virtual ~Protocol() {}

    // Get the max header size of current protocol
    virtual size_t GetMaxHeaderSize() = 0;

    // Before sending data, you need to build a header
    virtual Slice BuildPackageHeader(size_t pack_len) = 0;

    // return -1: error;  0: need more data; > 0 : pack_len
    virtual int CheckPackageLength(const Slice* obj) = 0;
};
} // namespace raptor
#endif  // __RAPTOR_PLUGIN_PROTOCOL__
