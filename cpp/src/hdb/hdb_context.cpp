// SPDX-License-Identifier: MIT
// HDBContext implementation.

#include "hdb/hdb_context.h"

#include <vector>

namespace xfiles {
namespace hdb {

namespace {
// Temporary write buffer for HDBContext writer.
// In P3 polish phase, this will be replaced by direct page mutation.
static std::vector<uint8_t> g_write_buffer;
}  // namespace

HDBContext::HDBContext(HDBContainer* container, MasterHDB* master)
    : container_(container), master_(master), reader_(nullptr), writer_(nullptr) {}

HDBContext::~HDBContext() {
    delete reader_;
    delete writer_;
}

void HDBContext::begin_read(std::size_t page_idx) {
    delete reader_;
    reader_ = nullptr;
    if (container_ && page_idx < container_->page_count()) {
        const Page& p = container_->page(page_idx);
        reader_ = new TLVReader(p.bytes, PAGE_SIZE);
    }
}

void HDBContext::begin_write(std::size_t /*page_idx*/) {
    delete writer_;
    g_write_buffer.assign(PAGE_SIZE, 0);
    writer_ = new TLVWriter(g_write_buffer.data(), g_write_buffer.size());
}

/// Byte-direct Read decoder (NPfl/vers base init) :
///
///   void(int *ctx) {                                  // thiscall this=obj
///     int  flag_struct = ctx[3];
///     int* svtable     = ctx[0];                                   // serializer vtable
///     svtable[+0xa8]();                                            // state_init()
///     svtable[+0x74](this+8, 2, 'NPfl');                           // read_bytes(this+8, 2, FOURCC)
///     if (*(char*)(flag_struct + 0xf) != 0) {                      // version flag set ?
///       uint32_t v = svtable[+0xbc]('vers');                       // read_version_dword
///       *(uint32_t*)(this + 0x18) = v;
///     // ... clears bits in this+0x10/this+0x14 / sets word at +0x24=1 ...
///
/// In the reference implementation, this collapses to two sequential 8-byte TLV record reads —
/// HDB's TLV grammar already encodes NPfl + vers as the first two records of
/// every class instance. The bit-manipulation tail on this+0x10/+0x14 is
/// engine-internal scratch and is not surfaced via this API.
void HDBContext::read_base_init(uint32_t& out_npfl_flags, uint32_t& out_vers) {
    if (reader_) {
        out_npfl_flags = reader_->read_uint32_field(0x4e);  // 'NPfl' -> short tag 0x4e
        out_vers       = reader_->read_uint32_field(0x76);  // 'vers' -> short tag 0x76
    } else {
        out_npfl_flags = 0;
        out_vers       = 0;
    }
}

void HDBContext::write_base_init(uint32_t npfl_flags, uint32_t vers) {
    if (writer_) {
        writer_->write_uint32_field(0x4e, npfl_flags);
        writer_->write_uint32_field(0x76, vers);
    }
}

}  // namespace hdb
}  // namespace xfiles
