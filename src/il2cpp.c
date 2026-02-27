#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "memkit.h"
#include "xdl.h"

// ============================================================================
// IL2CPP: STATIC STATE (Thread-Safe)
// ============================================================================

static void* g_il2cpp_handle = NULL;
static bool g_initialized = false;
static pthread_once_t g_init_once = PTHREAD_ONCE_INIT;
static pthread_mutex_t g_il2cpp_mutex = PTHREAD_MUTEX_INITIALIZER;

// Internal init function (called once via pthread_once)
static void memkit_il2cpp_init_once(void) {
    // Use XDL to open libil2cpp.so
    // XDL_DEFAULT = 0, bypasses linker restrictions on Android 7+
    // XDL can read both .dynsym and .symtab sections
    g_il2cpp_handle = xdl_open("libil2cpp.so", XDL_DEFAULT);
    g_initialized = true;
}

// ============================================================================
// IL2CPP: INITIALIZATION
// ============================================================================

bool memkit_il2cpp_init(void) {
    // Use pthread_once for thread-safe one-time initialization
    pthread_once(&g_init_once, memkit_il2cpp_init_once);
    
    // Lock mutex to safely read the handle
    pthread_mutex_lock(&g_il2cpp_mutex);
    bool has_handle = (g_il2cpp_handle != NULL);
    pthread_mutex_unlock(&g_il2cpp_mutex);
    
    return has_handle;
}

// ============================================================================
// IL2CPP: GET HANDLE
// ============================================================================

void* memkit_il2cpp_get_handle(void) {
    // Ensure initialization
    memkit_il2cpp_init();
    
    // Lock mutex to safely read the handle
    pthread_mutex_lock(&g_il2cpp_mutex);
    void* handle = g_il2cpp_handle;
    pthread_mutex_unlock(&g_il2cpp_mutex);
    
    return handle;
}

// ============================================================================
// IL2CPP: RESOLVE SYMBOL (.dynsym)
// ============================================================================

void* memkit_il2cpp_resolve(const char* symbol_name) {
    if (!symbol_name) {
        return NULL;
    }

    // Ensure initialization (thread-safe)
    memkit_il2cpp_init();

    // Lock mutex to safely access handle
    pthread_mutex_lock(&g_il2cpp_mutex);
    void* handle = g_il2cpp_handle;
    pthread_mutex_unlock(&g_il2cpp_mutex);

    // If handle is still NULL, try to open again
    if (!handle) {
        pthread_mutex_lock(&g_il2cpp_mutex);
        g_il2cpp_handle = xdl_open("libil2cpp.so", XDL_DEFAULT);
        handle = g_il2cpp_handle;
        pthread_mutex_unlock(&g_il2cpp_mutex);
        
        if (!handle) {
            return NULL;
        }
    }

    // Resolve the symbol using XDL
    // xdl_sym searches in .dynsym (dynamic symbol table)
    // This is where most exported functions live
    return xdl_sym(handle, symbol_name, NULL);
}

// ============================================================================
// IL2CPP: RESOLVE SYMBOL FROM SYMTAB ONLY (Advanced)
// Use this for stripped/internal functions not in .dynsym
// ============================================================================

void* memkit_il2cpp_resolve_symtab(const char* symbol_name) {
    if (!symbol_name) {
        return NULL;
    }

    // Ensure initialization (thread-safe)
    memkit_il2cpp_init();

    // Lock mutex to safely access handle
    pthread_mutex_lock(&g_il2cpp_mutex);
    void* handle = g_il2cpp_handle;
    pthread_mutex_unlock(&g_il2cpp_mutex);

    if (!handle) {
        return NULL;
    }

    // Use xdl_dsym to search only in .symtab section
    // This is useful for:
    // - Stripped symbols
    // - Internal/private functions
    // - Debug symbols
    return xdl_dsym(handle, symbol_name, NULL);
}
