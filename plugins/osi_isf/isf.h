/*!
 * @file isf.h
 * @brief A Volatility 3 ISF (Intermediate Symbol Format) profile: symbol addresses, struct
 * layouts, base type sizes and enum values, as dwarf2json, btf2json or kernel-lift write them,
 * plus a kernel-lift ISF's per-field provenance. Kept free of panda.h so the JSON parser never
 * meets QEMU's macros, and so a host tool (osi_isf_check) can use it without an engine.
 */
#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace isf {

struct Field {
    int64_t offset = 0;
    std::string kind;          // the field's type kind: base, pointer, struct, union, array, enum, typedef...
    std::string type_name;     // for struct, union, enum, typedef and base kinds
    int64_t count = 0;         // for arrays
    std::string subtype_kind;  // for arrays and pointers: the element's (pointee's) kind
    std::string subtype_name;  // ... and its name
    bool anonymous = false;
};

struct UserType {
    int64_t size = 0;
    std::map<std::string, Field> fields;
};

// A member path resolved through a type: its offset from the type's start, the member at the end,
// and the (type, member) hops taken, anonymous containers included.
struct Resolved {
    int64_t offset = 0;
    const Field *leaf = nullptr;
    std::vector<std::pair<std::string, std::string>> hops;
};

class Profile {
public:
    // Reads an ISF, xz-compressed or not. On failure returns false and sets err.
    bool load(const std::string &path, std::string &err);
    // Reads a kernel-lift provenance.json (per-field classes). On failure returns false.
    bool load_provenance(const std::string &path, std::string &err);

    std::optional<uint64_t> symbol(const std::string &name) const;
    // The first symbol named name or name.<suffix> (GCC's .isra.0, .constprop.0, ...).
    std::optional<uint64_t> symbol_prefix(const std::string &name) const;
    std::optional<int64_t> type_size(const std::string &type) const;
    // A base type's size in bytes ("int", "long unsigned int", "pointer"...).
    std::optional<int64_t> base_size(const std::string &name) const;
    // Offset of a dotted member path ("f_path.dentry") in type, looking through anonymous
    // structs and unions the way C does.
    std::optional<int64_t> offset(const std::string &type, const std::string &path) const;
    std::optional<Resolved> resolve(const std::string &type, const std::string &path) const;
    // The member at the end of the path, for its kind and array count.
    const Field *field(const std::string &type, const std::string &path) const;
    std::optional<int64_t> enum_value(const std::string &enum_name, const std::string &constant) const;
    // A typedef followed to what it names: (kind, name).
    std::pair<std::string, std::string> untypedef(std::string kind, std::string name) const;

    // The provenance class of type.member: "dwarf" for an ISF that isn't a kernel-lift ISF;
    // for a lift, "dynamic-verified", "target-verified", "inferred", "reference-carried",
    // "unverified-needed" (listed in metadata.kernel_lift.unverified_needed) or "unknown".
    std::string field_class(const std::string &type, const std::string &member, int64_t offset) const;
    static bool trusted_class(const std::string &cls);

    int pointer_size = 8;
    bool big_endian = false;
    std::string kernel;     // the vmlinux name from the metadata, for logs
    std::string producer;   // metadata.producer.name
    std::string banner;     // linux_banner's constant_data, when the ISF has it

    // kernel-lift metadata (metadata.kernel_lift), when the ISF is a lift.
    bool lifted = false;
    std::set<std::string> unverified_needed, inferred_needed;
    std::map<std::string, int64_t> dynamic_verified;  // "type.member" -> the verified offset
    std::string provenance_name;                      // metadata.kernel_lift.provenance
    bool provenance_loaded = false;

    // Filled by the parser.
    std::unordered_map<std::string, uint64_t> symbols;
    std::unordered_map<std::string, UserType> user_types;
    std::unordered_map<std::string, int64_t> base_sizes;
    std::unordered_map<std::string, std::pair<std::string, std::string>> typedefs;
    std::unordered_map<std::string, std::unordered_map<std::string, int64_t>> enums;
    // provenance.json: type -> member -> class
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> prov;

private:
    std::optional<Resolved> member(const std::string &type, const std::string &name, int depth) const;
};

}  // namespace isf
