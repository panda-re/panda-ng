/*!
 * @file isf.cpp
 * @brief Loads a Volatility 3 ISF with a SAX parser, keeping only what OSI needs: a full ISF for a
 * distribution kernel is ~50 MB of JSON, and a DOM of it would cost ten times that.
 */
#include "isf.h"

#include <cstring>
#include <fstream>
#include <sstream>

#include <lzma.h>
#include <nlohmann/json.hpp>

namespace isf {

namespace {

bool read_file(const std::string &path, std::string &out, std::string &err) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        err = "cannot open " + path;
        return false;
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    out = ss.str();
    return true;
}

bool unxz(const std::string &in, std::string &out, std::string &err) {
    lzma_stream s = LZMA_STREAM_INIT;
    if (lzma_stream_decoder(&s, UINT64_MAX, LZMA_CONCATENATED) != LZMA_OK) {
        err = "lzma_stream_decoder failed";
        return false;
    }
    s.next_in = (const uint8_t *)in.data();
    s.avail_in = in.size();
    std::vector<uint8_t> buf(1 << 20);
    lzma_ret r;
    do {
        s.next_out = buf.data();
        s.avail_out = buf.size();
        r = lzma_code(&s, LZMA_FINISH);
        out.append((const char *)buf.data(), buf.size() - s.avail_out);
    } while (r == LZMA_OK);
    lzma_end(&s);
    if (r != LZMA_STREAM_END) {
        err = "xz decode failed (" + std::to_string(r) + ")";
        return false;
    }
    return true;
}

std::string base64_decode(const std::string &in) {
    static const std::string chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    uint32_t acc = 0;
    int bits = 0;
    for (char c : in) {
        size_t v = chars.find(c);
        if (v == std::string::npos) continue;  // padding, line breaks
        acc = (acc << 6) | (uint32_t)v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back((char)((acc >> bits) & 0xff));
        }
    }
    return out;
}

// Tracks the key path from the root and stores the leaves OSI reads:
//   symbols.<name>.address, symbols.linux_banner.constant_data
//   user_types.<type>.size, user_types.<type>.fields.<field>.{offset,anonymous}
//   user_types.<type>.fields.<field>.type.{kind,name,count}, ...type.subtype.{kind,name}
//   base_types.<name>.size, base_types.pointer.endian
//   typedefs.<name>.{kind,name}
//   enums.<enum>.constants.<name>
//   metadata.linux.symbols[0].name, metadata.producer.name
//   metadata.kernel_lift.{unverified_needed[],inferred_needed[],provenance}
//   metadata.kernel_lift.dynamic_verified.<type.member>.offset
struct Sax : nlohmann::json_sax<nlohmann::json> {
    Profile &p;
    std::vector<std::string> path;
    explicit Sax(Profile &p) : p(p) {}

    bool at(std::initializer_list<const char *> want) const {
        if (path.size() != want.size()) return false;
        size_t i = 0;
        for (const char *w : want) {
            if (*w != '*' && path[i] != w) return false;
            i++;
        }
        return true;
    }

    void integer(int64_t v) {
        if (path.empty()) return;
        const std::string &top = path[0];
        if (top == "symbols") {
            if (at({"symbols", "*", "address"})) p.symbols[path[1]] = (uint64_t)v;
        } else if (top == "user_types") {
            if (at({"user_types", "*", "size"})) {
                p.user_types[path[1]].size = v;
            } else if (at({"user_types", "*", "fields", "*", "offset"})) {
                p.user_types[path[1]].fields[path[3]].offset = v;
            } else if (at({"user_types", "*", "fields", "*", "type", "count"})) {
                p.user_types[path[1]].fields[path[3]].count = v;
            }
        } else if (top == "base_types") {
            if (at({"base_types", "*", "size"})) {
                p.base_sizes[path[1]] = v;
                if (path[1] == "pointer") p.pointer_size = (int)v;
            }
        } else if (top == "enums") {
            if (at({"enums", "*", "constants", "*"})) p.enums[path[1]][path[3]] = v;
        } else if (top == "metadata") {
            if (at({"metadata", "kernel_lift", "dynamic_verified", "*", "offset"}))
                p.dynamic_verified[path[3]] = v;
        }
    }

    bool null() override { return true; }
    bool boolean(bool v) override {
        if (v && at({"user_types", "*", "fields", "*", "anonymous"}))
            p.user_types[path[1]].fields[path[3]].anonymous = true;
        return true;
    }
    bool number_integer(number_integer_t v) override { integer(v); return true; }
    bool number_unsigned(number_unsigned_t v) override { integer((int64_t)v); return true; }
    bool number_float(number_float_t, const string_t &) override { return true; }
    bool string(string_t &v) override {
        if (path.empty()) return true;
        if (path[0] == "user_types") {
            if (at({"user_types", "*", "fields", "*", "type", "kind"})) {
                p.user_types[path[1]].fields[path[3]].kind = v;
            } else if (at({"user_types", "*", "fields", "*", "type", "name"})) {
                p.user_types[path[1]].fields[path[3]].type_name = v;
            } else if (at({"user_types", "*", "fields", "*", "type", "subtype", "kind"})) {
                p.user_types[path[1]].fields[path[3]].subtype_kind = v;
            } else if (at({"user_types", "*", "fields", "*", "type", "subtype", "name"})) {
                p.user_types[path[1]].fields[path[3]].subtype_name = v;
            }
        } else if (path[0] == "typedefs") {
            if (at({"typedefs", "*", "kind"})) p.typedefs[path[1]].first = v;
            else if (at({"typedefs", "*", "name"})) p.typedefs[path[1]].second = v;
        } else if (at({"symbols", "linux_banner", "constant_data"})) {
            p.banner = base64_decode(v);
        } else if (at({"base_types", "pointer", "endian"})) {
            p.big_endian = v == "big";
        } else if (path[0] == "metadata") {
            if (p.kernel.empty() && at({"metadata", "linux", "symbols", "#", "name"})) {
                p.kernel = v;
            } else if (at({"metadata", "producer", "name"})) {
                p.producer = v;
            } else if (at({"metadata", "kernel_lift", "unverified_needed", "#"})) {
                p.unverified_needed.insert(v);
            } else if (at({"metadata", "kernel_lift", "inferred_needed", "#"})) {
                p.inferred_needed.insert(v);
            } else if (at({"metadata", "kernel_lift", "provenance"})) {
                p.provenance_name = v;
            }
        }
        return true;
    }
    bool binary(binary_t &) override { return true; }
    bool start_object(std::size_t) override { path.emplace_back(); return true; }
    bool key(string_t &k) override {
        path.back() = k;
        if (path.size() == 2 && path[0] == "metadata" && k == "kernel_lift") p.lifted = true;
        return true;
    }
    bool end_object() override { path.pop_back(); return true; }
    bool start_array(std::size_t) override { path.emplace_back("#"); return true; }
    bool end_array() override { path.pop_back(); return true; }
    bool parse_error(std::size_t pos, const std::string &, const nlohmann::detail::exception &e) override {
        error = "JSON error at byte " + std::to_string(pos) + ": " + e.what();
        return false;
    }
    std::string error;
};

}  // namespace

bool Profile::load(const std::string &path, std::string &err) {
    std::string raw, json;
    if (!read_file(path, raw, err)) return false;
    static const char xz_magic[] = "\xfd" "7zXZ";
    if (raw.size() >= 6 && memcmp(raw.data(), xz_magic, 5) == 0) {
        if (!unxz(raw, json, err)) return false;
        raw.clear();
        raw.shrink_to_fit();
    } else {
        json.swap(raw);
    }
    Sax sax(*this);
    if (!nlohmann::json::sax_parse(json, &sax)) {
        err = sax.error.empty() ? "JSON parse failed" : sax.error;
        return false;
    }
    // A BTF-derived ISF has types and no symbols (they come from System.map or kallsyms).
    if (user_types.empty()) {
        err = path + " has no user_types; is it an ISF?";
        return false;
    }
    return true;
}

std::optional<uint64_t> Profile::symbol(const std::string &name) const {
    auto it = symbols.find(name);
    if (it == symbols.end()) return std::nullopt;
    return it->second;
}

std::optional<uint64_t> Profile::symbol_prefix(const std::string &name) const {
    if (auto a = symbol(name)) return a;
    for (const auto &[k, v] : symbols) {
        if (k.size() > name.size() && k.compare(0, name.size(), name) == 0 && k[name.size()] == '.')
            return v;
    }
    return std::nullopt;
}

std::optional<int64_t> Profile::type_size(const std::string &type) const {
    auto it = user_types.find(type);
    if (it == user_types.end()) return std::nullopt;
    return it->second.size;
}

std::optional<int64_t> Profile::base_size(const std::string &name) const {
    auto it = base_sizes.find(name);
    if (it == base_sizes.end()) return std::nullopt;
    return it->second;
}

std::pair<std::string, std::string> Profile::untypedef(std::string kind, std::string name) const {
    for (int i = 0; kind == "typedef" && i < 16; i++) {
        auto it = typedefs.find(name);
        if (it == typedefs.end()) break;
        kind = it->second.first;
        name = it->second.second;
    }
    return {kind, name};
}

std::optional<Resolved> Profile::member(const std::string &type, const std::string &name, int depth) const {
    auto t = user_types.find(type);
    if (t == user_types.end() || depth > 8) return std::nullopt;
    auto f = t->second.fields.find(name);
    if (f != t->second.fields.end()) {
        Resolved r;
        r.offset = f->second.offset;
        r.leaf = &f->second;
        r.hops.emplace_back(type, name);
        return r;
    }
    for (const auto &[k, sub] : t->second.fields) {
        bool anon = sub.anonymous || k.rfind("unnamed_field", 0) == 0;
        if (anon && (sub.kind == "struct" || sub.kind == "union")) {
            if (auto r = member(sub.type_name, name, depth + 1)) {
                r->offset += sub.offset;
                r->hops.insert(r->hops.begin(), {type, k});
                return r;
            }
        }
    }
    return std::nullopt;
}

std::optional<Resolved> Profile::resolve(const std::string &type, const std::string &path) const {
    std::string t = type;
    Resolved out;
    size_t start = 0;
    while (true) {
        size_t dot = path.find('.', start);
        std::string part = path.substr(start, dot == std::string::npos ? std::string::npos : dot - start);
        auto r = member(t, part, 0);
        if (!r) return std::nullopt;
        out.offset += r->offset;
        out.leaf = r->leaf;
        out.hops.insert(out.hops.end(), r->hops.begin(), r->hops.end());
        if (dot == std::string::npos) return out;
        // Through a typedef'd struct member too.
        t = untypedef(r->leaf->kind, r->leaf->type_name).second;
        start = dot + 1;
    }
}

const Field *Profile::field(const std::string &type, const std::string &path) const {
    auto r = resolve(type, path);
    return r ? r->leaf : nullptr;
}

std::optional<int64_t> Profile::offset(const std::string &type, const std::string &path) const {
    auto r = resolve(type, path);
    if (!r) return std::nullopt;
    return r->offset;
}

bool Profile::load_provenance(const std::string &path, std::string &err) {
    std::string raw;
    if (!read_file(path, raw, err)) return false;
    nlohmann::json j = nlohmann::json::parse(raw, nullptr, false);
    if (j.is_discarded() || !j.is_object() || !j.contains("fields") || !j["fields"].is_object()) {
        err = path + " is not a kernel-lift provenance.json (no fields)";
        return false;
    }
    for (auto &[type, classes] : j["fields"].items()) {
        if (!classes.is_object()) continue;
        auto &m = prov[type];
        for (auto &[cls, members] : classes.items()) {
            if (cls == "non_emitting_transfer_status") continue;
            if (members.is_object()) {
                for (auto &[name, _] : members.items()) m.emplace(name, cls);
            } else if (members.is_array()) {
                for (auto &name : members)
                    if (name.is_string()) m.emplace(name.get<std::string>(), cls);
            }
        }
    }
    // Needed fields carry their class too ("type.member": {"class": ...}).
    if (j.contains("needed") && j["needed"].is_object()) {
        for (auto &[key, rec] : j["needed"].items()) {
            size_t dot = key.find('.');
            if (dot == std::string::npos || !rec.is_object() || !rec.contains("class") || !rec["class"].is_string())
                continue;
            prov[key.substr(0, dot)].emplace(key.substr(dot + 1), rec["class"].get<std::string>());
        }
    }
    provenance_loaded = true;
    return true;
}

std::string Profile::field_class(const std::string &type, const std::string &member, int64_t offset) const {
    if (!lifted) return "dwarf";
    std::string key = type + "." + member;
    if (unverified_needed.count(key)) return "unverified-needed";
    auto dv = dynamic_verified.find(key);
    if (dv != dynamic_verified.end() && dv->second == offset) return "dynamic-verified";
    auto t = prov.find(type);
    if (t != prov.end()) {
        auto m = t->second.find(member);
        if (m != t->second.end()) return m->second;
    }
    if (inferred_needed.count(key)) return "inferred";
    return "unknown";
}

bool Profile::trusted_class(const std::string &cls) {
    return cls == "dwarf" || cls == "target-verified" || cls == "dynamic-verified" || cls == "inferred";
}

std::optional<int64_t> Profile::enum_value(const std::string &enum_name, const std::string &constant) const {
    auto e = enums.find(enum_name);
    if (e == enums.end()) return std::nullopt;
    auto c = e->second.find(constant);
    if (c == e->second.end()) return std::nullopt;
    return c->second;
}

}  // namespace isf
