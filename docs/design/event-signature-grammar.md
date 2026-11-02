# Event signature grammar

An event's signature is the typed field list of its argument struct. It fixes the struct's
layout, it is what Python builds its cffi struct from, and its hashed part is the input to the
event's UUID. The rule the grammar follows is the one `events.h` states: **only an incompatible
argument change alters the UUID**. A change is incompatible when a C consumer built against the
old signature would read the new struct wrongly.

## Grammar

```
signature := field ("," field)* ["|" field ("," field)*]
field     := name ":" type
type      := scalar | scalar "[" N "]" | "bool" | "ptr" | "ptr<" target ">"
scalar    := u8 | u16 | u32 | u64 | i8 | i16 | i32 | i64
target    := identifier | "str" | scalar "[" lenfield "]"
```

- Fields are laid out with natural alignment after `uint32_t size`.
- The UUID is UUIDv5(ns, name + NUL + *hashed*). *Hashed* is the canonical text of the fields
  before `|`: `name:type` joined by `,`, with whitespace and empty items dropped. Fields after
  `|` are appended. They are not hashed, and `PANDA_EVENT_HAS` tells a consumer whether the
  publisher's struct has them.

## Types

| Type | C field | Hashed as | Python |
|---|---|---|---|
| `u8` … `i64` | `uint8_t` … `int64_t` | itself | int |
| `bool` | `bool` (1 byte) | `u8` | `True`/`False` |
| `type[N]` | `type name[N]` | `type[N]` | `bytes` for `u8[N]`, otherwise a list |
| `ptr` | `void *` | `ptr` | an opaque pointer |
| `ptr<T>` | `void *` | `ptr<T>` | a `T *` from the arch bindings |
| `ptr<str>` | `void *` | `ptr<str>` | `str`, from a NUL-terminated host string |
| `ptr<u8[len]>` | `void *` | `ptr<u8[len]>` | `bytes` of length `len` |

- **`bool`** has the size and alignment of `u8`, so the two bind to each other and hash the
  same. The full signature, which listings and `panda.events()` carry, keeps `bool`, so Python
  gets real booleans. A mutator that writes a value other than 0 or 1 into a `bool` field is
  undefined in C. The registry does not check for it.
- **Arrays** have `u8`…`i64` elements, and `N` is 1..65536. `N` is hashed, because a different
  `N` moves every later field. Arrays of pointers, arrays of arrays, and arrays of `bool` are not
  supported. Arrays may appear after `|`.
- **Pointer targets** are hashed. A consumer built against `ptr<TranslationBlock>` that receives
  a `CPUState *` would read garbage, so changing a target changes the UUID, and a mismatch is
  reported as a MISMATCH with a field diff. A `ptr` with no target hashes as it always has.
  - `T` is a C type name the bindings know (`CPUState`, `TranslationBlock`, `PandaSyscall`).
    It must be an identifier. The name is hashed, not the layout, so arch-specific layouts are
    fine.
  - In `ptr<u8[len]>`, `len` is an integer field that comes earlier in the signature. If the
    buffer is hashed, its `len` must be hashed too. The element can be any scalar type.
  - Guest addresses are not host pointers. They are `u64`, and helpers read them.

## C declarations

Publishers declare their structs with the field-list macros in `events.h`, which generate the
struct and the signature text from one list:
- `PANDA_EVENT_ARRAY` declares an array field.
- `PANDA_EVENT_PTR`, `PANDA_EVENT_STR` and `PANDA_EVENT_BUF` declare pointer fields with their
  targets.
- `PANDA_EVENT_CTYPE_bool` is `bool`.

## Python

The event struct is built in a runtime `FFI()`, with `void *` for every pointer, so a target
never changes the layout. The decoder casts across to the compiled arch FFI:
`ffi_arch.cast("CPUState *", int(ffi_ev.cast("uintptr_t", args.cpu)))`. An unknown `T` yields
the raw pointer and a warning.

## Effect on UUIDs

| Change | Hashed text | Effect on an existing UUID |
|---|---|---|
| `ptr<T>` added to a field | `ptr<T>` replaces `ptr` | changes |
| `ptr` with no target | `ptr` | none |
| `u8` spelled `bool` | `u8` | none |
| `type[N]` | `type[N]` | none (the syntax is new) |
| whitespace inside `< >` or `[ ]` | dropped | none |

`tests/static/event-uuid-vectors.tsv` holds the vectors that C, Python and Rust check,
including a `bool`/`u8` pair with the same UUID.
