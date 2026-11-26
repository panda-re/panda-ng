#pragma once
struct hook {
    target_ulong addr, asid;
    void (*cb)(CPUState *, TranslationBlock *, hook *);
    bool enabled;
    int km;
};
