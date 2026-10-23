/*
 * Linked into the engine the wheel ships (mkEngine { glibcCompat = true; }), so that it loads with
 * an older glibc than the one nix builds against.
 *
 * glibc 2.42 gave cfsetispeed/cfsetospeed new symbol versions: speed_t is now the baud rate
 * itself (B9600 == 9600), where it used to be a CBAUD code (B9600 == 015). They are the only
 * GLIBC_2.39+ symbols the engine needs (QEMU's host serial chardev). These definitions are hidden,
 * so the engine's calls bind here at link time; they translate the rate to the old code and call
 * the original version of the function, which every glibc still provides.
 */
struct termios;
typedef unsigned int speed_t;

#if defined(__x86_64__)
#define GLIBC_BASE "GLIBC_2.2.5"
#elif defined(__aarch64__)
#define GLIBC_BASE "GLIBC_2.17"
#else
#error "glibc-compat: unknown host architecture"
#endif

int pn_legacy_cfsetispeed(struct termios *t, speed_t s);
int pn_legacy_cfsetospeed(struct termios *t, speed_t s);
__asm__(".symver pn_legacy_cfsetispeed, cfsetispeed@" GLIBC_BASE);
__asm__(".symver pn_legacy_cfsetospeed, cfsetospeed@" GLIBC_BASE);

extern int *__errno_location(void);

/* Linux's CBAUD codes (asm-generic termbits, the same on x86_64 and aarch64). */
static const struct { unsigned int rate, code; } codes[] = {
    { 0, 0 }, { 50, 01 }, { 75, 02 }, { 110, 03 }, { 134, 04 }, { 150, 05 }, { 200, 06 },
    { 300, 07 }, { 600, 010 }, { 1200, 011 }, { 1800, 012 }, { 2400, 013 }, { 4800, 014 },
    { 9600, 015 }, { 19200, 016 }, { 38400, 017 }, { 57600, 010001 }, { 115200, 010002 },
    { 230400, 010003 }, { 460800, 010004 }, { 500000, 010005 }, { 576000, 010006 },
    { 921600, 010007 }, { 1000000, 010010 }, { 1152000, 010011 }, { 1500000, 010012 },
    { 2000000, 010013 }, { 2500000, 010014 }, { 3000000, 010015 }, { 3500000, 010016 },
    { 4000000, 010017 },
};

static int legacy_code(speed_t rate, speed_t *code)
{
    for (unsigned i = 0; i < sizeof(codes) / sizeof(codes[0]); i++) {
        if (codes[i].rate == rate) {
            *code = codes[i].code;
            return 0;
        }
    }
    *__errno_location() = 22; /* EINVAL */
    return -1;
}

__attribute__((visibility("hidden"))) int cfsetispeed(struct termios *t, speed_t s)
{
    speed_t code;
    return legacy_code(s, &code) ? -1 : pn_legacy_cfsetispeed(t, code);
}

__attribute__((visibility("hidden"))) int cfsetospeed(struct termios *t, speed_t s)
{
    speed_t code;
    return legacy_code(s, &code) ? -1 : pn_legacy_cfsetospeed(t, code);
}
