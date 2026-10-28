#pragma once
struct hook {
    target_ulong addr, asid;
    struct {
        void (*before_tcg_codegen)(CPUState *, TranslationBlock *, hook *);
    } cb;
    int type;
    bool enabled;
    int km;
};
