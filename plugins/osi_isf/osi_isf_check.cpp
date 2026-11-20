/*!
 * @file osi_isf_check.cpp
 * @brief Prints, without a guest, what osi_isf would answer for a kernel: its capability report
 * (JSON) from the ISF, provenance and symbols the plugin would be given.
 *
 *   osi_isf_check --isf F --arch A [--provenance P] [--symbols S] [--version V] [--allow-untrusted] [--text]
 *                 [--all-attempts]   (every strategy's inputs, also those after the chosen one)
 *   osi_isf_check --strategies     (the strategy table, as Markdown)
 */
#include <cstdio>
#include <cstring>
#include <string>

#include <nlohmann/json.hpp>

#include "caps.h"

namespace {

void print_strategies() {
    printf("| capability | strategy | kernels | arches | reads |\n|---|---|---|---|---|\n");
    for (const auto &st : osicaps::strategies()) {
        std::string arches, reads;
        for (auto a : st.arches) arches += (arches.empty() ? "" : ", ") + std::string(osicaps::arch_name(a));
        for (const auto &n : st.needs) {
            std::string k = n.key();
            k = k.substr(k.find(':') + 1);
            if (n.kind == osicaps::NeedKind::Size) k = "sizeof(" + k + ")";
            // Alternatives are split by '|', which a Markdown table cell must escape.
            for (size_t p = k.find('|'); p != std::string::npos; p = k.find('|', p + 2)) k.replace(p, 1, "\\|");
            reads += (reads.empty() ? "" : ", ") + std::string("`") + k + "`" +
                     (n.kind == osicaps::NeedKind::Field ? " (" + n.want() + ")" : "") + (n.optional ? " (optional)" : "");
        }
        for (const auto &r : st.requires_caps) reads += (reads.empty() ? "" : ", ") + std::string("capability ") + r;
        if (reads.empty()) reads = "registers only";
        printf("| %s | %s | %s | %s | %s |\n", st.cap.c_str(), st.name.c_str(), st.range().c_str(),
               arches.empty() ? "all" : arches.c_str(), reads.c_str());
    }
}

}  // namespace

int main(int argc, char **argv) {
    osicaps::Config cfg;
    bool text = false, all_attempts = false;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        auto val = [&](std::string &dst) {
            if (i + 1 >= argc) {
                fprintf(stderr, "%s needs a value\n", a.c_str());
                exit(2);
            }
            dst = argv[++i];
        };
        if (a == "--isf") val(cfg.isf);
        else if (a == "--arch") val(cfg.arch);
        else if (a == "--provenance") val(cfg.provenance);
        else if (a == "--symbols") val(cfg.symbols);
        else if (a == "--version") val(cfg.version);
        else if (a == "--allow-untrusted") cfg.allow_untrusted = true;
        else if (a == "--thread-size") {
            std::string v;
            val(v);
            cfg.thread_size = strtoll(v.c_str(), nullptr, 0);
        }
        else if (a == "--text") text = true;
        else if (a == "--all-attempts") all_attempts = true;
        else if (a == "--strategies") {
            print_strategies();
            return 0;
        }
        else {
            fprintf(stderr, "usage: %s --isf F --arch A [--provenance P] [--symbols S] [--version V] "
                            "[--allow-untrusted] [--thread-size N] [--text] [--all-attempts]\n", argv[0]);
            return 2;
        }
    }
    if (cfg.isf.empty() || cfg.arch.empty()) {
        fprintf(stderr, "--isf and --arch are required\n");
        return 2;
    }
    isf::Profile prof;
    osicaps::Symbols syms;
    osicaps::Options opt;
    std::vector<std::string> notes;
    std::string err;
    if (!osicaps::load(cfg, prof, syms, opt, notes, err)) {
        fprintf(stderr, "osi_isf_check: %s\n", err.c_str());
        return 1;
    }
    if (opt.arch == osicaps::Arch::unknown) {
        fprintf(stderr, "osi_isf_check: unknown arch %s\n", cfg.arch.c_str());
        return 2;
    }
    osicaps::Plan plan;
    opt.attempt_all = all_attempts;
    plan.resolve(prof, syms, opt);
    if (text) {
        for (const auto &n : notes) printf("note: %s\n", n.c_str());
        for (const auto &l : plan.lines()) printf("%s\n", l.c_str());
        return 0;
    }
    nlohmann::json j = nlohmann::json::parse(plan.json());
    j["kernel"] = prof.kernel;
    j["banner"] = prof.banner.substr(0, prof.banner.find('\n'));
    j["pointer_size"] = prof.pointer_size;
    j["big_endian"] = prof.big_endian;
    j["symbols"] = prof.symbols.size() + syms.file.size();
    j["notes"] = notes;
    printf("%s\n", j.dump(1).c_str());
    return 0;
}
