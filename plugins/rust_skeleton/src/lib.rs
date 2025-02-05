use core::ffi::c_void;
use panda::{
    panda_cb, panda_register_callback, CPUState, TranslationBlock, PANDA_CB_START_BLOCK_EXEC,
};

extern "C" fn before_block_exec(_env: *mut CPUState, _tb: *mut TranslationBlock) {
    println!("before_block_exec\n");
}

#[no_mangle]
extern "C" fn init_plugin(plugin_handle: *mut c_void) -> bool {
    let callback = panda_cb {
        before_block_exec: Some(before_block_exec),
    };

    unsafe {
        panda_register_callback(plugin_handle, PANDA_CB_START_BLOCK_EXEC, callback);
    }

    true
}

#[no_mangle]
extern "C" fn uninit_plugin(_: *mut c_void) {}
