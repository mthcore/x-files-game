// SPDX-License-Identifier: MIT
// NL VCFactory — runtime polymorphic construction of VC* classes from
// a class_id read off the HDB stream.
//
// Native pattern ( + serializer slot 0x138) :
//   1. Read class_id from the stream
//   2. Lookup ClassRegistry::find(class_id) → ClassMeta with factory_fn
//   3. Call factory(heap, init_flag) → void*
//   4. Cast to VCObject* then invoke slot 12 (Read)
//
// Our reference implementation simplifies : we keep a compile-time table of the ported
// classes and create the matching C++ instance. Unsupported
// class_ids return nullptr (to be ported in NL.2 Batch 1+/NL.3).
//
// This factory is the pivot of NL.1.3 (polymorphic sub-readers). Once
// `make_vc_object(class_id)` works, we can implement
// `HDBSerializer::read_obj(fourcc)` which :
//   - reads the class_id from the stream
//   - constructs the object via make_vc_object
//   - invokes obj->Read(this)
//   - returns a unique_ptr<VCObject>

#ifndef NL_VC_FACTORY_H
#define NL_VC_FACTORY_H

#include "nl/vc_object.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace xfiles {
namespace nl {

/// Construct a fresh VC* instance for `class_id`. Returns nullptr for
/// class_ids not yet ported. Currently supported (class_id → C++ type) :
///
///   0x33 → VCNav
///   0x46 → VCEmotionIcon
///   0x47 → VCActionIcon
///   0x48 → VCIdeaIcon
///   0x49 → VCEvidenceIcon
///   0x4b → VCInventory
///   0x31 → VCConversation
///
/// Other class_ids return nullptr (port pending — track in
/// the NeoPersist 3.0 class registry).
std::unique_ptr<VCObject> make_vc_object(uint32_t class_id);

/// True if `make_vc_object(class_id)` would return a non-null object.
bool is_class_supported(uint32_t class_id);

/// Polymorphic record reader : reads a class_id from the serializer
/// (using `read_dword_fourcc(class_id_marker)`), constructs the right
/// VC* instance, invokes its Read, and returns the populated object.
///
/// `class_id_marker` is the FOURCC tag preceding the class_id in the
/// stream. The on-disk format uses 'NPTc' (0x4e505463) for trigger
/// composites — pass that for the most common case.
///
/// Returns nullptr if the class_id is not supported (logs to stderr in
/// debug builds).
std::unique_ptr<VCObject> read_obj(class HDBSerializer* ser,
                                    uint32_t class_id_marker);

/// Read a polymorphic list of objects from the stream. The list wire
/// format is :
///   [count : uint32 LE preceded by 'NPlt' fourcc tag]
///   N × [NPTc class_id : uint32 LE + record bytes for that class]
///
/// This mirrors the on-disk list iter, simplified for our
/// reference implementation. Returns the constructed object vector. Unsupported class_ids
/// produce nullptr entries (logged) and stop processing.
std::vector<std::unique_ptr<VCObject>> read_obj_list(class HDBSerializer* ser,
                                                       uint32_t count_marker,
                                                       uint32_t class_id_marker);

/// Look up the class_id of a VCObject by dynamic_cast through every
/// supported type. Returns 0 if the type isn't supported (it should be ;
/// every VC* class has a kClassId static constant).
uint32_t class_id_of(const VCObject* obj);

/// Write a polymorphic object : emits [class_id_marker fourcc + class_id]
/// then calls obj->Write(ser). Null obj writes class_id=0 as a sentinel.
void write_obj(class HDBSerializer* ser, uint32_t class_id_marker,
               const VCObject* obj);

/// Write a polymorphic list : emits [count_marker + count] then N times
/// [class_id_marker + class_id + record bytes].
void write_obj_list(class HDBSerializer* ser,
                    uint32_t count_marker,
                    uint32_t class_id_marker,
                    const std::vector<std::unique_ptr<VCObject>>& items);

}  // namespace nl
}  // namespace xfiles

#endif  // NL_VC_FACTORY_H
