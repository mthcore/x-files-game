// SPDX-License-Identifier: MIT
// NL ClassRegistry implementation — byte-direct Read decoder (392 B).

#include "nl/class_meta.h"

namespace xfiles {
namespace nl {

ClassRegistry& ClassRegistry::instance() {
    static ClassRegistry r;
    return r;
}

ClassRegistry::~ClassRegistry() {
    for (auto& kv : entries_) {
        delete kv.second;
    }
}

ClassMeta* ClassRegistry::register_class(uint32_t   class_id,
                                          ClassKind  kind,
                                          const std::string& name,
                                          FactoryFn  factory,
                                          ClassMeta* parent,
                                          uint32_t   default_value) {
    // doesn't replace existing entries — it overwrites the
    // ref-counted slot at a registry table[class_id+0x2d], dec-ref-ing the previous
    // and storing the new. In the reference implementation we replace + delete the old meta.
    auto it = entries_.find(class_id);
    if (it != entries_.end()) {
        delete it->second;
        entries_.erase(it);
    }

    ClassMeta* m = new ClassMeta;
    m->class_id      = class_id;
    m->kind          = kind;
    m->name          = name;
    m->factory       = factory;
    m->parent        = parent;
    m->default_value = default_value;

    // the on-disk format user-class branch (line 63) : if type_flag==0 && class_id != 1,
    // walk parent chain to inherit factory if param_5 is null. The reference implementation
    // requires callers to pass the resolved parent meta explicitly, so we
    // skip the walk.
    //
    // the on-disk format system-class branch (line 79-83) : stores fourcc 'ID  ' at
    // default_value slot. Callers that pass kind=System should set
    // default_value to 0x49442020 ('ID  ') explicitly ; this matches the
    // the on-disk format override.

    entries_.emplace(class_id, m);
    return m;
}

ClassMeta* ClassRegistry::find(uint32_t class_id) const {
    auto it = entries_.find(class_id);
    return (it == entries_.end()) ? nullptr : it->second;
}

ClassMeta* ClassRegistry::find_by_name(const std::string& name) const {
    for (const auto& kv : entries_) {
        if (kv.second->name == name) return kv.second;
    }
    return nullptr;
}

void ClassRegistry::clear() {
    for (auto& kv : entries_) {
        delete kv.second;
    }
    entries_.clear();
}

}  // namespace nl
}  // namespace xfiles
