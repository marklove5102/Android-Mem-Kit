#ifndef MEMKIT_H
#define MEMKIT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// MEMORY PATCHING API
// ============================================================================

typedef struct {
    uintptr_t address;
    size_t size;
    uint8_t* orig_bytes;
    uint8_t* patch_bytes;
} MemPatch;

/**
 * Get the lowest base address of a loaded library
 * @param lib_name Name of the library (e.g., "libil2cpp.so")
 * @return Base address or 0 if not found
 */
uintptr_t memkit_get_lib_base(const char* lib_name);

/**
 * Create a memory patch from hex string
 * Automatically backs up original bytes safely (handles XOM)
 * @param address Target absolute address
 * @param hex_string Hex string with spaces (e.g., "00 00 A0 E3")
 * @return MemPatch pointer or NULL on failure
 */
MemPatch* memkit_patch_create(uintptr_t address, const char* hex_string);

/**
 * Apply the memory patch
 * @param patch MemPatch pointer
 * @return true on success, false on failure
 */
bool memkit_patch_apply(MemPatch* patch);

/**
 * Restore original bytes
 * @param patch MemPatch pointer
 * @return true on success, false on failure
 */
bool memkit_patch_restore(MemPatch* patch);

/**
 * Free MemPatch resources
 * @param patch MemPatch pointer
 */
void memkit_patch_free(MemPatch* patch);

// ============================================================================
// HOOKING API (ShadowHook)
// ============================================================================

/**
 * Initialize ShadowHook (call once at startup)
 * @param mode SHADOWHOOK_MODE_UNIQUE, SHARED, or MULTI
 * @param debuggable Enable debug logging
 * @return 0 on success, negative value on failure
 */
int memkit_hook_init(int mode, bool debuggable);

/**
 * Hook a function at target address
 * FIXED: Returns stub handle for later unhooking
 * FIXED: Uses out parameter for original function
 * 
 * @param target_addr Address of target function
 * @param replace_func Pointer to replacement function (proxy)
 * @param out_orig_func Output: pointer to original function
 * @return Stub handle (for unhook) or NULL on failure
 */
void* memkit_hook(uintptr_t target_addr, void* replace_func, void** out_orig_func);

/**
 * Unhook a function using stub handle
 * @param stub Handle returned by memkit_hook
 */
void memkit_unhook(void* stub);

/**
 * Hook by symbol name (convenience wrapper)
 * @param lib_name Library name (e.g., "libil2cpp.so")
 * @param symbol_name Symbol to hook (e.g., "il2cpp_thread_attach")
 * @param replace_func Pointer to replacement function
 * @param out_orig_func Output: pointer to original function
 * @return Stub handle or NULL on failure
 */
void* memkit_hook_by_symbol(const char* lib_name, const char* symbol_name, void* replace_func, void** out_orig_func);

// ============================================================================
// IL2CPP API (XDL)
// ============================================================================

/**
 * Initialize IL2CPP handle (call once or let resolve auto-initialize)
 * @return true on success, false on failure
 */
bool memkit_il2cpp_init(void);

/**
 * Resolve an IL2CPP export symbol from .dynsym
 * @param symbol_name Symbol name (e.g., "il2cpp_domain_get")
 * @return Function pointer or NULL on failure
 */
void* memkit_il2cpp_resolve(const char* symbol_name);

/**
 * Resolve an IL2CPP symbol from .symtab section only (advanced)
 * Use this for stripped/internal symbols not in .dynsym
 * @param symbol_name Symbol name
 * @return Function pointer or NULL on failure
 */
void* memkit_il2cpp_resolve_symtab(const char* symbol_name);

/**
 * Get cached IL2CPP handle
 * @return Handle pointer or NULL
 */
void* memkit_il2cpp_get_handle(void);

// ============================================================================
// MACROS FOR CONVENIENCE
// ============================================================================

/**
 * IL2CPP_CALL macro - Auto-caches resolved function pointer
 * Uses __builtin_expect for branch prediction optimization
 * 
 * Usage: IL2CPP_CALL(return_type, "symbol_name", arg_types...)(arguments...)
 * 
 * Example:
 *   void* domain = IL2CPP_CALL(void*, "il2cpp_domain_get")(void);
 *   IL2CPP_CALL(void, "il2cpp_thread_attach", void*)(domain);
 */
#define IL2CPP_CALL(ret_type, func_name, ...) ({ \
    static ret_type (*func_ptr)(__VA_ARGS__) = NULL; \
    if (__builtin_expect(!func_ptr, 0)) { \
        func_ptr = (ret_type (*)(__VA_ARGS__)) memkit_il2cpp_resolve(func_name); \
    } \
    func_ptr; \
})

#ifdef __cplusplus
}
#endif

#endif // MEMKIT_H
