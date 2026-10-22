/*!
 * @file isf.h
 * @brief A Volatility 3 ISF (Intermediate Symbol Format) profile: symbol addresses, struct
 * layouts and enum values, as dwarf2json writes them. Kept free of panda.h so the JSON parser
 * never meets QEMU's macros.
 */
#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace isf {

struct Field {
    int64_t offset = 0;
    std::string kind;       // the field's type kind: base, pointer, struct, union, array, enum...
    std::string type_name;  // for struct, union, enum and base kinds
    int64_t count = 0;      // for arrays
    bool anonymous = false;
};

struct UserType {
    int64_t size = 0;
    std::map<std::string, Field> fields;
};

class Profile {
public:
    // Reads an ISF, xz-compressed or not. On failure returns false and sets err.
    bool load(const std::string &path, std::string &err);

    std::optional<uint64_t> symbol(const std::string &name) const;
    // The first symbol named name or name.<suffix> (GCC's .isra.0, .constprop.0, ...).
    std::optional<uint64_t> symbol_prefix(const std::string &name) const;
    std::optional<int64_t> type_size(const std::string &type) const;
    // Offset of a dotted member path ("f_path.dentry") in type, looking through anonymous
    // structs and unions the way C does.
    std::optional<int64_t> offset(const std::string &type, const std::string &path) const;
    // The member at the end of the path, for its kind and array count.
    const Field *field(const std::string &type, const std::string &path) const;
    std::optional<int64_t> enum_value(const std::string &enum_name, const std::string &constant) const;

    int pointer_size = 8;
    bool big_endian = false;
    std::string kernel;  // the vmlinux name from the metadata, for logs

    // Filled by the parser.
    std::unordered_map<std::string, uint64_t> symbols;
    std::unordered_map<std::string, UserType> user_types;
    std::unordered_map<std::string, std::unordered_map<std::string, int64_t>> enums;

private:
    std::optional<std::pair<int64_t, const Field *>> member(const std::string &type,
                                                            const std::string &name,
                                                            int depth) const;
};

}  // namespace isf
