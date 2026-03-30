# Android-Mem-Kit Usage Guide

## Table of Contents

1. [Getting Started](#getting-started)
2. [API Reference](#api-reference)
3. [Memory Patching](#memory-patching)
4. [Function Hooking](#function-hooking)
5. [IL2CPP Instrumentation](#il2cpp-instrumentation)
6. [Error Handling](#error-handling)
7. [Best Practices](#best-practices)

---

## Getting Started

### Quick Setup

```c
#include "memkit.h"
#include <android/log.h>

#define LOG_TAG "MyResearch"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
```

### Initialization Sequence

```c
__attribute__((constructor))
void init() {
    // Step 1: Initialize ShadowHook (required before any hooking)
    int ret = memkit_hook_init(SHADOWHOOK_MODE_UNIQUE, false);
    if (ret != 0) {
        LOGE("ShadowHook init failed: %d", ret);
        return;
    }

    // Step 2: Wait for target library to load
    uintptr_t base = 0;
    for (int i = 0; i < 30 && base == 0; i++) {
        base = memkit_get_lib_base("libtarget.so");
        if (base == 0) sleep(1);
    }

    if (base == 0) {
        LOGE("Target library not found");
        return;
    }

    LOGI("Library base: 0x%lx", base);

    // Step 3: Start your instrumentation
    start_instrumentation(base);
}
```

---

## API Reference

### Memory Functions

| Function | Description | Returns |
|----------|-------------|---------|
| `memkit_get_lib_base(const char* lib_name)` | Get lowest base address of loaded library | `uintptr_t` or 0 |
| `memkit_patch_create(addr, hex_string)` | Create memory patch from hex string | `MemPatch*` or NULL |
| `memkit_patch_apply(patch)` | Apply memory patch | `bool` |
| `memkit_patch_restore(patch)` | Restore original bytes | `bool` |
| `memkit_patch_free(patch)` | Free patch resources | `void` |

### Hooking Functions

| Function | Description | Returns |
|----------|-------------|---------|
| `memkit_hook_init(mode, debuggable)` | Initialize ShadowHook | `int` (0 = success) |
| `memkit_hook(addr, replace, &orig)` | Hook function by address | `stub` or NULL |
| `memkit_hook_by_symbol(lib, sym, func, &orig)` | Hook by symbol name | `stub` or NULL |
| `memkit_unhook(stub)` | Unhook function | `void` |

### IL2CPP Functions

| Function | Description | Returns |
|----------|-------------|---------|
| `memkit_il2cpp_init()` | Initialize IL2CPP handle | `bool` |
| `memkit_il2cpp_resolve(symbol)` | Resolve from .dynsym | `void*` or NULL |
| `memkit_il2cpp_resolve_symtab(symbol)` | Resolve from .symtab | `void*` or NULL |
| `memkit_il2cpp_get_handle()` | Get cached handle | `void*` or NULL |
| `IL2CPP_CALL(ret, name, ...)` | Macro for auto-cached calls | Function pointer |

### XDL Wrapper Functions

#### Library Discovery

| Function | Description | Returns |
|----------|-------------|---------|
| `memkit_xdl_iterate(cb, data, flags)` | Iterate all loaded libraries | `int` (count or -1) |
| `memkit_xdl_open(name, flags)` | Open library handle | `void*` or NULL |
| `memkit_xdl_close(handle)` | Close library handle | `bool` |
| `memkit_xdl_get_lib_info(handle, &info)` | Get library details | `bool` |

#### Symbol Resolution

| Function | Description | Returns |
|----------|-------------|---------|
| `memkit_xdl_sym(handle, symbol, &size)` | Resolve from .dynsym | `void*` or NULL |
| `memkit_xdl_dsym(handle, symbol, &size)` | Resolve from .symtab (debug) | `void*` or NULL |

#### Address-to-Symbol (Debug Introspection)

| Function | Description | Returns |
|----------|-------------|---------|
| `memkit_xdl_addr_ctx_create()` | Create resolution context | `ctx*` or NULL |
| `memkit_xdl_addr_ctx_destroy(ctx)` | Destroy context | `void` |
| `memkit_xdl_addr_to_symbol(addr, &info, ctx)` | Resolve address to symbol | `bool` |
| `memkit_xdl_addr_to_symbol4(addr, &info, ctx, flags)` | With flags (e.g., `XDL_NON_SYM`) | `bool` |

#### Advanced

| Function | Description | Returns |
|----------|-------------|---------|
| `memkit_xdl_open_from_phdr(info)` | Create handle from `dl_phdr_info` | `void*` or NULL |

#### Convenience Macros

| Macro | Description |
|-------|-------------|
| `XDL_RESOLVE(lib, sym)` | One-shot symbol resolve |
| `XDL_RESOLVE_SIZE(lib, sym, &size)` | Resolve with size output |

---

## Memory Patching

### Basic Patch

```c
// ARM64: MOV X0, #0 (returns 0)
uintptr_t base = memkit_get_lib_base("libtarget.so");
MemPatch* patch = memkit_patch_create(base + 0x1234, "00 00 80 D2");

if (patch && memkit_patch_apply(patch)) {
    LOGI("Patch applied successfully");
} else {
    LOGE("Patch failed: %d", errno);
}

// Restore later if needed
// memkit_patch_restore(patch);

// Free when done
// memkit_patch_free(patch);
```

### Multi-Byte Patch

```c
// ARM64: Multiple instructions
MemPatch* multi_patch = memkit_patch_create(
    base + 0x5678,
    "00 00 80 D2 20 00 80 D2 1F 20 03 D5"  // MOV X0,#0; MOV X0,#1; NOP
);

if (multi_patch && memkit_patch_apply(multi_patch)) {
    LOGI("Multi-byte patch applied");
}
```

### Cross-Page Boundary (Safe)

The library automatically handles patches that span memory pages:

```c
// This 20-byte patch might span two pages
// memkit handles this automatically
MemPatch* cross_page = memkit_patch_create(
    base + 0xFFF0,  // Near page boundary
    "00 00 80 D2 20 00 80 D2 40 00 80 D2 60 00 80 D2 80 00 80 D2"
);
```

---

## Function Hooking

### Hook by Symbol Name (Recommended)

```c
// Original function pointer
static int (*orig_target_function)(int param) = NULL;
static void* hook_stub = NULL;

// Replacement function
static int my_target_function(int param) {
    LOGI("target_function called with: %d", param);

    // Call original if needed
    return orig_target_function(param);
}

// Hook it
hook_stub = memkit_hook_by_symbol(
    "libtarget.so",
    "target_function",
    (void*)my_target_function,
    (void**)&orig_target_function
);

if (hook_stub) {
    LOGI("Hook successful");
} else {
    LOGE("Hook failed: %d", errno);
}
```

### Hook by Address

```c
uintptr_t func_addr = base + 0xABCD;

hook_stub = memkit_hook(
    func_addr,
    (void*)my_function,
    (void**)&orig_function
);
```

### Unhook

```c
// When done, unhook to restore original behavior
memkit_unhook(hook_stub);
```

### ShadowHook Macros

ShadowHook provides convenient macros:

```c
// In your hook function, call original using macro
void my_hook(void* instance) {
    // Option 1: Use stored original pointer
    orig_function(instance);

    // Option 2: Use SHADOWHOOK_CALL_PREV macro
    SHADOWHOOK_CALL_PREV(my_hook, void (*)(void*), instance);
}
```

---

## IL2CPP Instrumentation

### Basic IL2CPP Call

```c
// Auto-cached function call
void* (*il2cpp_domain_get)(void) = IL2CPP_CALL(void*, "il2cpp_domain_get");

if (il2cpp_domain_get) {
    void* domain = il2cpp_domain_get();
    LOGI("IL2CPP Domain: %p", domain);
}
```

### Multiple Calls

```c
// Get domain
void* (*il2cpp_domain_get)(void) = IL2CPP_CALL(void*, "il2cpp_domain_get");
void* domain = il2cpp_domain_get();

// Attach thread
void* (*il2cpp_thread_attach)(void*) = IL2CPP_CALL(void*, "il2cpp_thread_attach", void*);
il2cpp_thread_attach(domain);

// Get root namespace
void* (*il2cpp_get_root_namespace)(void) = IL2CPP_CALL(void*, "il2cpp_get_root_namespace");
void* root_ns = il2cpp_get_root_namespace();
```

### Resolve Internal Symbols

```c
// Some symbols are only in .symtab
void* internal_func = memkit_il2cpp_resolve_symtab("_ZN6Player13InternalInitEv");

if (internal_func) {
    LOGI("Found internal function: %p", internal_func);
}
```

### Hook IL2CPP Functions

```c
static void* (*orig_il2cpp_thread_attach)(void*) = NULL;
static void* thread_hook_stub = NULL;

static void* my_il2cpp_thread_attach(void* domain) {
    LOGI("il2cpp_thread_attach called");
    return orig_il2cpp_thread_attach(domain);
}

// Hook it
thread_hook_stub = memkit_hook_by_symbol(
    "libil2cpp.so",
    "il2cpp_thread_attach",
    (void*)my_il2cpp_thread_attach,
    (void**)&orig_il2cpp_thread_attach
);
```

---

## Error Handling

### Using errno

All functions set `errno` on failure:

```c
#include <errno.h>
#include <string.h>

void* stub = memkit_hook_by_symbol("lib.so", "func", my_func, (void**)&orig);
if (stub == NULL) {
    LOGE("Hook failed: %s", strerror(errno));

    // Common errors:
    // EINVAL - Invalid argument
    // ENOENT - Symbol not found
    // EACCES - Permission denied (mprotect failed)
    // ENOMEM - Out of memory
}
```

### ShadowHook Error Codes

```c
if (stub == NULL) {
    int err = shadowhook_get_errno();
    const char* msg = shadowhook_to_errmsg(err);
    LOGE("ShadowHook error: %d - %s", err, msg);
}
```

Common ShadowHook errors:
- `SHADOWHOOK_ERRNO_HOOK_DLSYM` - Symbol not found
- `SHADOWHOOK_ERRNO_HOOK_ENTER` - Failed to enter hook
- `SHADOWHOOK_ERRNO_INVALID_ARG` - Invalid argument
- `SHADOWHOOK_ERRNO_UNINIT` - Not initialized

---

## Best Practices

### 1. Thread Safety

The library is thread-safe. You can call APIs from multiple threads:

```c
// Safe to call from any thread
void* thread_func(void* arg) {
    void* func = IL2CPP_CALL(void*, "some_function");
    if (func) func();
    return NULL;
}
```

### 2. Memory Management

Always free patches when done:

```c
MemPatch* patch = memkit_patch_create(...);
memkit_patch_apply(patch);

// ... later ...
memkit_patch_restore(patch);
memkit_patch_free(patch);
```

### 3. Wait for Library Load

```c
uintptr_t wait_for_lib(const char* name, int timeout_sec) {
    uintptr_t base = 0;
    for (int i = 0; i < timeout_sec && base == 0; i++) {
        base = memkit_get_lib_base(name);
        if (base == 0) sleep(1);
    }
    return base;
}
```

### 4. Validate Pointers

```c
// Always check for NULL before using
if (orig_function != NULL) {
    orig_function(param);
}

// Check patch before applying
if (patch && memkit_patch_apply(patch)) {
    // Success
}
```

### 5. Use UNIQUE Mode

Unless you need multiple hooks on same function:

```c
// Recommended for most cases
memkit_hook_init(SHADOWHOOK_MODE_UNIQUE, false);
```

### 6. Logging

Use Android logging for debugging:

```c
#include <android/log.h>

#define LOG_TAG "MyResearch"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
```

---

## XDL Wrapper Examples

### Discover All Loaded Libraries

```c
typedef struct {
    const char* target;
    uintptr_t base;
} find_lib_ctx_t;

static bool find_library_callback(const MemKitLibInfo* info, void* user_data) {
    find_lib_ctx_t* ctx = (find_lib_ctx_t*)user_data;

    if (strcmp(info->name, ctx->target) == 0) {
        ctx->base = info->base;
        LOGI("Found %s at 0x%lx (size: %zu bytes)", 
             info->name, info->base, info->size);
        return false;  // Stop iteration
    }

    LOGD("Library: %s @ 0x%lx", info->name, info->base);
    return true;  // Continue
}

void discover_libraries() {
    find_lib_ctx_t ctx = {.target = "libil2cpp.so"};
    
    int count = memkit_xdl_iterate(find_library_callback, &ctx, XDL_DEFAULT);
    LOGI("Iterated %d libraries, found target at 0x%lx", count, ctx.base);
}
```

### Resolve Symbol from Any Library

```c
// Generic symbol resolution (not just IL2CPP)
void* resolve_from_libc() {
    void* handle = memkit_xdl_open("libc.so", XDL_DEFAULT);
    if (!handle) return NULL;

    void* open_sym = memkit_xdl_sym(handle, "open", NULL);
    LOGI("libc.so::open = %p", open_sym);

    memkit_xdl_close(handle);
    return open_sym;
}

// One-shot with macro
void* sym = XDL_RESOLVE("libc.so", "open");
```

### Address-to-Symbol (Stack Trace / Debugging)

```c
void resolve_address(void* addr) {
    memkit_addr_ctx_t* ctx = memkit_xdl_addr_ctx_create();
    MemKitSymInfo info;

    if (memkit_xdl_addr_to_symbol(addr, &info, ctx)) {
        LOGI("Address %p:", addr);
        LOGI("  Library: %s (base: 0x%lx)", info.lib_name, info.lib_base);
        LOGI("  Symbol: %s (offset: 0x%lx, size: %zu)", 
             info.sym_name ? info.sym_name : "<unknown>",
             info.sym_offset, info.sym_size);
    } else {
        LOGI("Could not resolve address %p", addr);
    }

    memkit_xdl_addr_ctx_destroy(ctx);
}

// Resolve multiple addresses (reuse context for performance)
void resolve_multiple_addresses(void** addrs, int count) {
    memkit_addr_ctx_t* ctx = memkit_xdl_addr_ctx_create();
    
    for (int i = 0; i < count; i++) {
        MemKitSymInfo info;
        if (memkit_xdl_addr_to_symbol(addrs[i], &info, ctx)) {
            LOGI("[%d] %p -> %s!%s+0x%lx", 
                 i, addrs[i], info.lib_name, 
                 info.sym_name ? info.sym_name : "?", info.sym_offset);
        }
    }

    memkit_xdl_addr_ctx_destroy(ctx);
}
```

### Get Library Information

```c
void print_lib_info(const char* lib_name) {
    void* handle = memkit_xdl_open(lib_name, XDL_DEFAULT);
    if (!handle) {
        LOGE("Could not open %s", lib_name);
        return;
    }

    MemKitLibInfo info;
    if (memkit_xdl_get_lib_info(handle, &info)) {
        LOGI("Library: %s", info.name);
        LOGI("  Base: 0x%lx", info.base);
        LOGI("  Path: %s", info.path ? info.path : "N/A");
    }

    memkit_xdl_close(handle);
}
```

### Fast Address-to-Library (Skip Symbol Lookup)

```c
// Use XDL_NON_SYM for faster lookup when you only need library info
void quick_lib_lookup(void* addr) {
    memkit_addr_ctx_t* ctx = memkit_xdl_addr_ctx_create();
    MemKitSymInfo info;

    // Skip symbol resolution for faster results
    if (memkit_xdl_addr_to_symbol4(addr, &info, ctx, XDL_NON_SYM)) {
        LOGI("Address %p is in %s (base: 0x%lx)", 
             addr, info.lib_name, info.lib_base);
        // info.sym_name will be NULL (skipped)
    }

    memkit_xdl_addr_ctx_destroy(ctx);
}
```

### Thread Safety

The XDL wrapper is **thread-safe**:

```c
// Multiple threads can safely call memkit_xdl_iterate()
void* thread_func(void* arg) {
    // Each thread gets its own TLS buffer
    memkit_xdl_iterate(my_callback, NULL, XDL_DEFAULT);
    return NULL;
}

// Address resolution context is per-thread (NOT shared)
void* worker(void* arg) {
    // Create per-thread context
    memkit_addr_ctx_t* ctx = memkit_xdl_addr_ctx_create();
    // ... use ctx ...
    memkit_xdl_addr_ctx_destroy(ctx);
    return NULL;
}
```

---

## Next Steps

- See [RECIPES.md](RECIPES.md) for common patterns
- See [SECURITY_RESEARCH.md](SECURITY_RESEARCH.md) for legitimate use cases
