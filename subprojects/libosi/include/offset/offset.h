#ifndef __OFFSET_OFFSET_H
#define __OFFSET_OFFSET_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct StructureType;

struct MemberResult {
    uint64_t offset;
    const struct StructureType* type;
};

struct StructureTypeLibrary;

struct StructureTypeLibrary* load_type_library(const char* profile);
const char* get_type_library_profile(const StructureTypeLibrary* tlib);
struct MemberResult* offset_of(struct StructureTypeLibrary* tlib,
                               const struct StructureType* type, const char* member);
void free_member_result(struct MemberResult* mr);

char* translate_enum(struct StructureTypeLibrary* tlib, const char* ename, long idx);

const struct StructureType* translate(struct StructureTypeLibrary* tlib,
                                      const char* tname);
const struct StructureType* dereference_st(struct StructureTypeLibrary* tlib,
                                           const struct StructureType* st);

bool is_valid_structure_type(const struct StructureType* st);
bool is_pointer_structure_type(const struct StructureType* st);
bool is_unknown_structure_type(const struct StructureType* st);

bool equal_structure_types(const struct StructureType* st1,
                           const struct StructureType* st2);

#define INVALID_OFFSET (0xFFFFFFFFFFFFFFFFLL)
#define INVALID_TYPE (0xFFFFFFFFFFFFFFFFLL)

#ifdef __cplusplus
}
#endif

#endif
