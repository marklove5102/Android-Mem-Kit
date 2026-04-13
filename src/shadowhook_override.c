#include <sys/system_properties.h>
#include <android/log.h>
#include <stdlib.h>

// The real sh_linker_init from ShadowHook's statically-linked code.
// The linker --wrap flag replaces references to sh_linker_init with __wrap_sh_linker_init,
// and makes the real implementation available as __real_sh_linker_init.
extern int __real_sh_linker_init(void);

static int get_android_api_level(void) {
    char value[PROP_VALUE_MAX] = {0};
    int len = __system_property_get("ro.build.version.sdk", value);
    if (len <= 0) return 0;
    return (int)atoi(value);
}

// This function replaces sh_linker_init at link time via -Wl,--wrap,sh_linker_init
//
// We unconditionally skip sh_linker_init() because:
// 1. On Android 15 (API 35): linker internal symbols changed, hooking ctor/dtor fails
// 2. On some devices < API 35: the same failure occurs due to OEM ROM modifications
//
// The ShadowHook linker module is ONLY used for dl_init/dl_fini callbacks
// (tracking when libraries are loaded/unloaded). Core hooking APIs work fine without it:
//   - shadowhook_hook_func_addr()   ✅ works
//   - shadowhook_hook_sym_addr()    ✅ works
//   - shadowhook_hook_sym_name()    ✅ works
//   - shadowhook_hook_sym_name_callback() ✅ works
//
// What WON'T work:
//   - shadowhook_register_dl_init_callback() ❌ callbacks never fire
int __wrap_sh_linker_init(void) {
    int api_level = get_android_api_level();

    __android_log_print(ANDROID_LOG_INFO, "memkit",
        "sh_linker_init skipped (API %d). "
        "Core hooking works; dl_init/dl_fini callbacks disabled.", api_level);

    // Always return success — skip linker module init entirely
    (void)__real_sh_linker_init; // suppress unused warning
    return 0;
}
