/*!
 * @file caps.h
 * @brief What osi_isf can answer for a kernel, decided before it reads guest memory.
 *
 * Each OSI capability (current process, process list, mappings, modules, ...) has one or more
 * strategies. A strategy is tied to a range of kernel versions and, where it matters, to
 * architectures, and declares every input it reads: symbols, struct members with the shape and
 * width they must have, type sizes and enum constants. At load the plan resolves every input
 * against the ISF, a symbol file and the ISF's provenance, and picks for each capability the
 * first strategy whose version range holds the kernel and whose inputs all resolve (and are
 * trusted, unless untrusted values are allowed). The plugin then reads only inputs of chosen
 * strategies: an input no chosen strategy declared reads as absent.
 *
 * Free of panda.h, so osi_isf_check can print the same report on the host without a guest.
 */
#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "isf.h"

namespace osicaps {

// A kernel version from a banner ("Linux version 4.10.0 ...") or a string ("3.18.140").
struct KVer {
    int a = 0, b = 0, c = 0;
    bool known() const { return a > 0; }
    int64_t key() const { return ((int64_t)a << 32) | ((int64_t)b << 16) | (c > 0xffff ? 0xffff : c); }
    std::string str() const;
    static KVer parse(const std::string &s);  // a banner or a dotted version; unknown if neither
};

enum class Arch { x86_64, i386, arm, aarch64, mips, riscv, ppc, loongarch, unknown };
Arch arch_from_name(const std::string &name);  // x86_64, i386, arm, aarch64, mips*, riscv*, ppc*, loongarch*
const char *arch_name(Arch a);

// ThreadSize: THREAD_SIZE, from sizeof(union thread_union) or the init_stack array.
enum class NeedKind { Sym, SymPrefix, Field, Size, Enum, ThreadSize };
// The shape a member must have. Width is in bytes: 0 is the pointer size, -1 any.
enum class Shape { Any, Ptr, Int, List, Chars, Struct, Array };

struct Need {
    NeedKind kind;
    // Sym: name (alternatives split by '|'); Field: type, member path (alternative paths split by
    // '|': a member renamed or moved across versions, the first that resolves); Size: type;
    // Enum: enum, constant
    std::string a, b;
    Shape shape = Shape::Any;
    int width = -1;
    std::string tname;    // Struct: the struct's name ("timespec"; alternatives split by '|')
    bool optional = false;
    std::string key() const;   // "sym:init_task", "field:task_struct.tasks", ...
    std::string want() const;  // the shape, for reports: "pointer", "4-byte integer", ...
};

struct Strategy {
    std::string cap, name;
    KVer min, max;              // [min, max); an unknown max is open
    std::vector<Arch> arches;   // empty: every arch
    std::vector<std::string> requires_caps;
    std::vector<Need> needs;
    std::string note;
    bool for_version(const KVer &v) const;
    bool for_arch(Arch a) const;
    std::string range() const;  // "3.17+", "2.6.0-3.17", "any"
};

struct Input {
    Need need;
    bool present = false, shape_ok = false, trusted = false;
    int64_t value = -1;   // address, offset, size or enum value
    int64_t count = 0;    // array element count
    int64_t width = 0;    // the resolved member's width in bytes
    std::string cls;      // provenance class (fields), symbol source (symbols)
    std::string used;     // the alternative member path that resolved, when there are several
    std::string problem;  // why it isn't usable
    bool usable(bool allow_untrusted) const { return present && shape_ok && (trusted || allow_untrusted); }
};

enum class Status { Available, Untrusted, Unavailable };
const char *status_name(Status s);

struct Attempt {
    const Strategy *strategy;
    std::vector<Input> inputs;
    std::string outcome;  // "chosen", "missing ...", "outside its versions", ...
};

struct CapResult {
    std::string cap;
    Status status = Status::Unavailable;
    const Strategy *strategy = nullptr;
    std::vector<Input> inputs;            // the chosen strategy's
    std::vector<std::string> degraded;    // optional inputs left out
    std::vector<Attempt> attempts;        // every strategy considered, for the report
    std::string reason;                   // for an unavailable capability
};

// Symbols from the ISF and from a System.map or /proc/kallsyms file (which wins where both
// have a symbol).
struct Symbols {
    const isf::Profile *prof = nullptr;
    std::unordered_map<std::string, uint64_t> file;
    std::string file_path;
    int differing = 0;   // symbols in both whose addresses differ
    bool load_file(const std::string &path, std::string &err);
    std::optional<uint64_t> get(const std::string &name, std::string *source = nullptr) const;
    std::optional<uint64_t> get_prefix(const std::string &name, std::string *source = nullptr) const;
};

struct Options {
    Arch arch = Arch::unknown;
    KVer version;
    std::string version_source;   // "ISF banner", "version=", "guest banner"
    bool allow_untrusted = false;
    int64_t thread_size = 0;
    bool attempt_all = false;     // resolve every strategy, also those after the chosen one (reports)
};

// What a caller gives: the plugin's arguments, or osi_isf_check's.
struct Config {
    std::string isf, provenance, symbols, version, arch;
    bool allow_untrusted = false;
    int64_t thread_size = 0;   // THREAD_SIZE, when known from elsewhere (kernel-lift's modabi)
};

// Loads the ISF, its provenance (a lift's provenance.json: the given path, else the one the ISF's
// metadata names, next to the ISF) and the symbol file, and sets the options (the version: the
// version argument, else the ISF's linux_banner). notes gets what the log should say.
bool load(const Config &cfg, isf::Profile &prof, Symbols &syms, Options &opt,
          std::vector<std::string> &notes, std::string &err);

const std::vector<Strategy> &strategies();
const std::vector<std::string> &capability_names();

class Plan {
public:
    void resolve(const isf::Profile &prof, const Symbols &syms, const Options &opt);

    const CapResult *cap(const std::string &name) const;
    bool ok(const std::string &name) const;   // available, or available from untrusted inputs
    const std::string &why(const std::string &name) const;
    // Inputs of chosen strategies; -1 / 0 where no chosen strategy declared them.
    int64_t off(const std::string &type, const std::string &path) const;
    int64_t count(const std::string &type, const std::string &path) const;
    int64_t width(const std::string &type, const std::string &path) const;
    uint64_t sym(const std::string &name) const;
    int64_t size(const std::string &type) const;
    int64_t enum_value(const std::string &e, const std::string &c) const;
    const std::string &strategy(const std::string &cap) const;

    std::string json() const;
    std::vector<std::string> lines() const;   // one per capability, for the load log

    Options opt;
    std::string isf_kind;   // "dwarf" or "kernel-lift"
    std::vector<CapResult> caps;
    std::map<std::string, Input> values;

private:
    const Input *value(const std::string &key) const;
};

}  // namespace osicaps
