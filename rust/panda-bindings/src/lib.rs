#![allow(
    non_camel_case_types,
    non_upper_case_globals,
    improper_ctypes,
    non_snake_case,
    improper_ctypes_definitions
)]

#[cfg(feature = "aarch64")]
include!("autogen/aarch64.rs");

#[cfg(feature = "arm")]
include!("autogen/arm.rs");

#[cfg(feature = "i386")]
include!("autogen/i386.rs");

#[cfg(feature = "mips64el")]
include!("autogen/mips64el.rs");

#[cfg(feature = "mips64")]
include!("autogen/mips64.rs");

#[cfg(feature = "mipsel")]
include!("autogen/mipsel.rs");

#[cfg(feature = "mips")]
include!("autogen/mips.rs");

#[cfg(feature = "ppc64")]
include!("autogen/ppc64.rs");

#[cfg(feature = "ppc")]
include!("autogen/ppc.rs");

#[cfg(feature = "x86_64")]
include!("autogen/x86_64.rs");
