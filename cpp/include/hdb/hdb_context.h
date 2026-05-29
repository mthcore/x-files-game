// SPDX-License-Identifier: MIT
// HDBContext — serializer context passed to VC class Read/Write methods.
//
// on-disk equivalent : the `param_1` argument to each Read method (
// for VCGameState etc.). Provides :
//   - Access to current container
//   - TLV reader/writer cursor state
//   - Master HDB for ID dispatch
//   - Class-specific vtable method dispatch (slots +0x74, +0x9c, +0xa4, etc.)

#ifndef HDB_CONTEXT_H
#define HDB_CONTEXT_H

#include "hdb/hdb_container.h"
#include "hdb/hdb_tlv.h"
#include "hdb/master_hdb.h"

#include <cstdint>

namespace xfiles {
namespace hdb {

/// Serializer context — state for reading/writing VC objects.
class HDBContext {
public:
    HDBContext(HDBContainer* container, MasterHDB* master);
    ~HDBContext();

    /// Begin reading from a specific page.
    void begin_read(std::size_t page_idx);

    /// Begin writing to a specific page.
    void begin_write(std::size_t page_idx);

    /// Current TLV reader (valid after begin_read).
    TLVReader& reader() { return *reader_; }

    /// Current TLV writer (valid after begin_write).
    TLVWriter& writer() { return *writer_; }

    /// Access master HDB for ID dispatch.
    MasterHDB* master() { return master_; }

    /// Read VCObject base init (NPfl flags + vers version dword).
    /// Equivalent to the on-disk base init (this, ctx).
    void read_base_init(uint32_t& out_npfl_flags, uint32_t& out_vers);

    /// Write VCObject base init.
    void write_base_init(uint32_t npfl_flags, uint32_t vers);

private:
    HDBContainer* container_;
    MasterHDB*    master_;
    TLVReader*    reader_;
    TLVWriter*    writer_;
};

}  // namespace hdb
}  // namespace xfiles

#endif  // HDB_CONTEXT_H
