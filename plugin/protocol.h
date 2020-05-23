#ifndef __RAPTOR_PLUGIN_PROTOCOL__
#define __RAPTOR_PLUGIN_PROTOCOL__

#include <stddef.h>

namespace raptor {
class Slice;
class Protocol {
public:
    virtual ~Protocol() {}

    // Only return the header size of the current layer protocol
    virtual size_t GetHeaderSize();

    // Before sending data, you need to build a header
    virtual Slice BuildPackageHeader(size_t pack_len) = 0;

    // return -1: error;  0: need more data; > 0 : pack_len
    virtual int CheckPackageLength(const Slice* obj) = 0;

    // protocol parsing chain
    virtual bool OnDataParsing(const Slice* input, Slice* output) = 0;

};
} // namespace raptor
#endif  // __RAPTOR_PLUGIN_PROTOCOL__
