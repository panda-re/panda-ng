//! Guest writes through the engine's logged API (include/panda/guest-write.h).
//!
//! Without record/replay each call writes at once. While recording, a write goes ahead only
//! inside a mutating callback at a site that logs it (a guest hypercall, for now): the engine
//! logs it and replay repeats it at the same instruction without running the callback. Anywhere
//! else during record/replay it is refused with [`GuestWriteError::Refused`].
//!
//! A [`GuestWriter`] is for one vCPU, made in a callback that mutates: an observer has no reason
//! to make one. Env fields are written with [`env_write!`](crate::env_write), which computes the
//! field's offset from the bindgen `CPUArchState` and checks the value's type; `=>` follows a
//! pointer field (by path: host addresses differ between the recording and replay processes):
//!
//! ```ignore
//! let w = unsafe { GuestWriter::new(cpu) };
//! env_write!(w, regs[0], 1)?;                    // a field, an array element
//! env_write!(w, pmsav7.drbar => [3], 0x2000)?;    // through a pointer
//! env_write!(w, pmsav8.rbar[1] => [2], 0x3000)?;  // through an array of pointers
//! ```

use crate::CPUState;
use core::ffi::{c_int, c_uint};
#[cfg(test)]
use core::ffi::{c_char, c_void};

/// The engine's GuestWriteResult, other than OK.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum GuestWriteError {
    /// No logging scope: an observer, a replay, or a site that does not log writes.
    Refused,
    /// Bad vCPU, register, length or path (a host pointer, repointing).
    Invalid,
    /// The address does not translate or is not RAM; a NULL pointer on a path.
    Fault,
    /// The target refused the register write.
    Denied,
    /// Past the per-site write budget.
    TooBig,
    /// A result this crate does not know.
    Other(c_uint),
}

fn result(code: c_uint) -> Result<(), GuestWriteError> {
    match code {
        0 => Ok(()),
        1 => Err(GuestWriteError::Refused),
        2 => Err(GuestWriteError::Invalid),
        3 => Err(GuestWriteError::Fault),
        4 => Err(GuestWriteError::Denied),
        5 => Err(GuestWriteError::TooBig),
        n => Err(GuestWriteError::Other(n)),
    }
}

#[cfg(not(test))]
use crate::{
    panda_guest_env_sync, panda_guest_reg_lookup, panda_guest_set_retval, panda_guest_write_env_path,
    panda_guest_write_phys, panda_guest_write_reg, panda_guest_write_virt,
};

/// Guest writes for one vCPU.
pub struct GuestWriter {
    cpu: *mut CPUState,
}

impl GuestWriter {
    /// # Safety
    /// `cpu` is the vCPU a PANDA callback was given, and the writer does not outlive the
    /// callback.
    pub unsafe fn new(cpu: *mut CPUState) -> Self {
        GuestWriter { cpu }
    }

    /// Bytes at a guest virtual address of this vCPU's address space.
    pub fn write_virt(&self, addr: u64, data: &[u8]) -> Result<(), GuestWriteError> {
        result(unsafe { panda_guest_write_virt(self.cpu, addr, data.as_ptr().cast(), data.len()) })
    }

    /// Bytes at a guest physical address (RAM only).
    pub fn write_phys(&self, addr: u64, data: &[u8]) -> Result<(), GuestWriteError> {
        result(unsafe { panda_guest_write_phys(addr, data.as_ptr().cast(), data.len()) })
    }

    /// The gdbstub number and size of the register named as in the target's gdb XML.
    pub fn reg_lookup(&self, name: &str) -> Option<(c_int, usize)> {
        let name = std::ffi::CString::new(name).ok()?;
        let mut len = 0usize;
        let num = unsafe { panda_guest_reg_lookup(self.cpu, name.as_ptr(), &mut len) };
        (num >= 0).then_some((num, len))
    }

    /// A register through the target's setter; `data` is its size, in the guest's byte order.
    pub fn write_reg(&self, regnum: c_int, data: &[u8]) -> Result<(), GuestWriteError> {
        result(unsafe { panda_guest_write_reg(self.cpu, regnum, data.as_ptr().cast(), data.len()) })
    }

    /// The return register of a hypercall or syscall.
    pub fn set_retval(&self, value: u64) -> Result<(), GuestWriteError> {
        result(unsafe { panda_guest_set_retval(self.cpu, value) })
    }

    /// Bytes at `offset` of CPUArchState, or of what the pointer fields at `derefs` lead to.
    /// [`env_write!`](crate::env_write) computes both.
    pub fn write_env(&self, derefs: &[u32], offset: usize, data: &[u8]) -> Result<(), GuestWriteError> {
        result(unsafe {
            panda_guest_write_env_path(
                self.cpu,
                derefs.as_ptr(),
                derefs.len(),
                offset,
                data.as_ptr().cast(),
                data.len(),
            )
        })
    }

    /// Recompute what the target derives from env (arm hflags) and flush the TLB.
    pub fn env_sync(&self) -> Result<(), GuestWriteError> {
        result(unsafe { panda_guest_env_sync(self.cpu) })
    }
}

// The macro's type plumbing: a field's place gives its type without reading it.
#[doc(hidden)]
pub fn __bytes<T: Copy>(value: &T) -> &[u8] {
    unsafe { core::slice::from_raw_parts((value as *const T).cast(), core::mem::size_of::<T>()) }
}
#[doc(hidden)]
pub fn __field<T: Copy>(_: *const T, value: T) -> T {
    value
}
#[doc(hidden)]
pub fn __elem<T: Copy, const N: usize>(_: *const [T; N], index: usize, value: T) -> (usize, T) {
    assert!(index < N, "env_write!: index {index} past an array of {N}");
    (index * core::mem::size_of::<T>(), value)
}
#[doc(hidden)]
pub fn __elem_place<T, const N: usize>(_: *const [T; N], index: usize) -> usize {
    assert!(index < N, "env_write!: index {index} past an array of {N}");
    index * core::mem::size_of::<T>()
}
#[doc(hidden)]
pub fn __pointee<T: Copy>(_: *const *mut T, index: usize, value: T) -> (usize, T) {
    (index * core::mem::size_of::<T>(), value)
}
#[doc(hidden)]
pub fn __pointee_in<T: Copy, const N: usize>(_: *const [*mut T; N], index: usize, value: T) -> (usize, T) {
    (index * core::mem::size_of::<T>(), value)
}
#[doc(hidden)]
pub fn __place() -> core::mem::MaybeUninit<crate::CPUArchState> {
    core::mem::MaybeUninit::uninit()
}

/// Write a CPUArchState field through a [`GuestWriter`]: `env_write!(w, field.path, value)`,
/// `env_write!(w, array[i], value)`, through a pointer `env_write!(w, ptr => [j], value)` or
/// an array of pointers `env_write!(w, ptrs[i] => [j], value)`. The value must have the
/// field's type. Returns the writer's result.
#[macro_export]
macro_rules! env_write {
    ($w:expr, $($f:ident).+ [$i:expr] => [$j:expr], $v:expr) => {{
        let env = $crate::guest::__place();
        let place = unsafe { ::core::ptr::addr_of!((*env.as_ptr()).$($f).+) };
        let at = ::core::mem::offset_of!($crate::CPUArchState, $($f).+)
            + $crate::guest::__elem_place(place, $i);
        let (offset, value) = $crate::guest::__pointee_in(place, $j, $v);
        $w.write_env(&[at as u32], offset, $crate::guest::__bytes(&value))
    }};
    ($w:expr, $($f:ident).+ => [$j:expr], $v:expr) => {{
        let env = $crate::guest::__place();
        let place = unsafe { ::core::ptr::addr_of!((*env.as_ptr()).$($f).+) };
        let at = ::core::mem::offset_of!($crate::CPUArchState, $($f).+);
        let (offset, value) = $crate::guest::__pointee(place, $j, $v);
        $w.write_env(&[at as u32], offset, $crate::guest::__bytes(&value))
    }};
    ($w:expr, $($f:ident).+ [$i:expr], $v:expr) => {{
        let env = $crate::guest::__place();
        let place = unsafe { ::core::ptr::addr_of!((*env.as_ptr()).$($f).+) };
        let (offset, value) = $crate::guest::__elem(place, $i, $v);
        let offset = offset + ::core::mem::offset_of!($crate::CPUArchState, $($f).+);
        $w.write_env(&[], offset, $crate::guest::__bytes(&value))
    }};
    ($w:expr, $($f:ident).+, $v:expr) => {{
        let env = $crate::guest::__place();
        let place = unsafe { ::core::ptr::addr_of!((*env.as_ptr()).$($f).+) };
        let value = $crate::guest::__field(place, $v);
        $w.write_env(&[], ::core::mem::offset_of!($crate::CPUArchState, $($f).+),
                     $crate::guest::__bytes(&value))
    }};
}

// The macro's arithmetic against offset_of!, with the engine calls recorded instead of made.
#[cfg(test)]
thread_local! {
    static CALLS: std::cell::RefCell<Vec<(Vec<u32>, usize, Vec<u8>)>> = Default::default();
}
#[cfg(test)]
unsafe fn panda_guest_write_env_path(
    _cpu: *mut CPUState,
    derefs: *const u32,
    n: usize,
    offset: usize,
    buf: *const c_void,
    len: usize,
) -> c_uint {
    let path = if n == 0 { vec![] } else { std::slice::from_raw_parts(derefs, n).to_vec() };
    let data = std::slice::from_raw_parts(buf.cast::<u8>(), len).to_vec();
    CALLS.with(|c| c.borrow_mut().push((path, offset, data)));
    0
}
#[cfg(test)]
unsafe fn panda_guest_write_virt(_: *mut CPUState, _: u64, _: *const c_void, _: usize) -> c_uint { 1 }
#[cfg(test)]
unsafe fn panda_guest_write_phys(_: u64, _: *const c_void, _: usize) -> c_uint { 3 }
#[cfg(test)]
unsafe fn panda_guest_write_reg(_: *mut CPUState, _: c_int, _: *const c_void, _: usize) -> c_uint { 4 }
#[cfg(test)]
unsafe fn panda_guest_set_retval(_: *mut CPUState, _: u64) -> c_uint { 5 }
#[cfg(test)]
unsafe fn panda_guest_env_sync(_: *mut CPUState) -> c_uint { 0 }
#[cfg(test)]
unsafe fn panda_guest_reg_lookup(_: *mut CPUState, _: *const c_char, _: *mut usize) -> c_int { -1 }

#[cfg(test)]
mod tests {
    use super::*;
    use crate::CPUArchState;
    use core::mem::offset_of;

    fn calls() -> Vec<(Vec<u32>, usize, Vec<u8>)> {
        CALLS.with(|c| c.borrow_mut().drain(..).collect())
    }

    #[test]
    fn results() {
        let w = unsafe { GuestWriter::new(core::ptr::null_mut()) };
        assert_eq!(w.write_virt(0, &[1]), Err(GuestWriteError::Refused));
        assert_eq!(w.write_phys(0, &[1]), Err(GuestWriteError::Fault));
        assert_eq!(w.write_reg(0, &[1]), Err(GuestWriteError::Denied));
        assert_eq!(w.set_retval(0), Err(GuestWriteError::TooBig));
        assert_eq!(w.env_sync(), Ok(()));
        assert_eq!(w.reg_lookup("pc"), None);
    }

    #[cfg(feature = "x86_64")]
    #[test]
    fn x86_64_offsets() {
        let w = unsafe { GuestWriter::new(core::ptr::null_mut()) };
        env_write!(w, regs[3], 0x1122u64).unwrap();
        env_write!(w, eip, 0x4000u64).unwrap();
        env_write!(w, ldt.base, 7u64).unwrap();
        assert_eq!(calls(), vec![
            (vec![], offset_of!(CPUArchState, regs) + 3 * 8, 0x1122u64.to_ne_bytes().to_vec()),
            (vec![], offset_of!(CPUArchState, eip), 0x4000u64.to_ne_bytes().to_vec()),
            (vec![], offset_of!(CPUArchState, ldt.base), 7u64.to_ne_bytes().to_vec()),
        ]);
    }

    #[cfg(feature = "aarch64")]
    #[test]
    fn aarch64_paths() {
        let w = unsafe { GuestWriter::new(core::ptr::null_mut()) };
        env_write!(w, xregs[0], 9u64).unwrap();
        env_write!(w, pmsav7.drbar => [3], 0x2000u32).unwrap();
        env_write!(w, pmsav8.rbar[1] => [2], 0x3000u32).unwrap();
        assert_eq!(calls(), vec![
            (vec![], offset_of!(CPUArchState, xregs), 9u64.to_ne_bytes().to_vec()),
            (vec![offset_of!(CPUArchState, pmsav7.drbar) as u32], 12, 0x2000u32.to_ne_bytes().to_vec()),
            (vec![(offset_of!(CPUArchState, pmsav8.rbar) + 8) as u32], 8, 0x3000u32.to_ne_bytes().to_vec()),
        ]);
    }
}
