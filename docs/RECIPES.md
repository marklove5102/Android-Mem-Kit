# Android-Mem-Kit - Common Recipes

A collection of common patterns and use cases for security research.

---

## Table of Contents

1. [SSL/TLS Research](#ssltls-research)
2. [Integrity Checks](#integrity-checks)
3. [Function Tracing](#function-tracing)
4. [Root Detection Bypass](#root-detection-bypass)
5. [Crypto Analysis](#crypto-analysis)
6. [Anti-Debugging Bypass](#anti-debugging-bypass)

---

## SSL/TLS Research

### 1. SSL Pinning Bypass

```c
#include "memkit.h"
#include <android/log.h>

#define LOG_TAG "SSL-Research"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

// Hook SSL_verify_cert_chain to always succeed
static int (*orig_SSL_verify_cert_chain)(void* ssl) = NULL;
static void* ssl_verify_stub = NULL;

static int my_SSL_verify_cert_chain(void* ssl) {
    LOGI("[SSL Research] Certificate verification intercepted");
    return 1; // Always succeed
}

// Hook SSL_get1_peer_certificate to log certificate info
static void* (*orig_SSL_get1_peer_certificate)(void* ssl) = NULL;
static void* ssl_peer_stub = NULL;

static void* my_SSL_get1_peer_certificate(void* ssl) {
    LOGI("[SSL Research] Peer certificate requested");
    return orig_SSL_get1_peer_certificate(ssl);
}

void init_ssl_bypass() {
    // Hook certificate verification
    ssl_verify_stub = memkit_hook_by_symbol(
        "libssl.so",
        "SSL_verify_cert_chain",
        (void*)my_SSL_verify_cert_chain,
        (void**)&orig_SSL_verify_cert_chain
    );

    // Hook peer certificate retrieval
    ssl_peer_stub = memkit_hook_by_symbol(
        "libssl.so",
        "SSL_get1_peer_certificate",
        (void*)my_SSL_get1_peer_certificate,
        (void**)&orig_SSL_get1_peer_certificate
    );
}
```

### 2. Log Decrypted SSL Data

```c
static int (*orig_SSL_read)(void* ssl, void* buf, int num) = NULL;
static int (*orig_SSL_write)(void* ssl, const void* buf, int num) = NULL;

static int my_SSL_read(void* ssl, void* buf, int num) {
    int ret = orig_SSL_read(ssl, buf, num);

    if (ret > 0) {
        LOGI("[SSL Research] Received %d bytes:", ret);
        // Log first 256 bytes (for research only!)
        int log_len = (ret < 256) ? ret : 256;

        char hex[512];
        for (int i = 0; i < log_len; i++) {
            sprintf(hex + (i * 2), "%02x ", ((unsigned char*)buf)[i]);
        }
        LOGD("%s", hex);
    }

    return ret;
}

static int my_SSL_write(void* ssl, const void* buf, int num) {
    LOGI("[SSL Research] Sending %d bytes:", num);

    // Log outgoing data
    int log_len = (num < 256) ? num : 256;
    char hex[512];
    for (int i = 0; i < log_len; i++) {
        sprintf(hex + (i * 2), "%02x ", ((unsigned char*)buf)[i]);
    }
    LOGD("%s", hex);

    return orig_SSL_write(ssl, buf, num);
}

void init_ssl_logging() {
    memkit_hook_by_symbol("libssl.so", "SSL_read",
                          (void*)my_SSL_read, (void**)&orig_SSL_read);
    memkit_hook_by_symbol("libssl.so", "SSL_write",
                          (void*)my_SSL_write, (void**)&orig_SSL_write);
}
```

### 3. OKHttp Pinning Bypass (Java Layer)

```c
// Hook OkHttp's CertificatePinter.check()
static bool (*orig_CertificatePinter_check)(void* instance, const char* hostname, void* chain) = NULL;

static bool my_CertificatePinter_check(void* instance, const char* hostname, void* chain) {
    LOGI("[OKHttp Research] Pinning check for: %s", hostname);
    return true; // Always pass
}

void init_okhttp_bypass() {
    memkit_hook_by_symbol(
        "libokhttp.so",
        "CertificatePinter_check",
        (void*)my_CertificatePinter_check,
        (void**)&orig_CertificatePinter_check
    );
}
```

---

## Integrity Checks

### 1. Signature Verification Bypass

```c
// Hook signature verification
static int (*orig_verifySignature)(const char* data, const char* sig) = NULL;
static void* verify_stub = NULL;

static int my_verifySignature(const char* data, const char* sig) {
    LOGI("[Integrity Research] Signature verification called");
    LOGI("  Data: %.50s...", data);
    return 1; // Always valid
}

void init_signature_bypass() {
    verify_stub = memkit_hook_by_symbol(
        "libtarget.so",
        "verifySignature",
        (void*)my_verifySignature,
        (void**)&orig_verifySignature
    );
}
```

### 2. Checksum/Hash Bypass

```c
// Hook MD5/SHA comparison functions
static int (*orig_memcmp)(const void* s1, const void* s2, size_t n) = NULL;

static int my_memcmp(const void* s1, const void* s2, size_t n) {
    // Check if this is a hash comparison (16 or 32 bytes)
    if (n == 16 || n == 32) {
        LOGI("[Integrity Research] Hash comparison detected (%zu bytes)", n);
        return 0; // Always match
    }
    return orig_memcmp(s1, s2, n);
}

void init_hash_bypass() {
    memkit_hook_by_symbol("libc.so", "memcmp",
                          (void*)my_memcmp, (void**)&orig_memcmp);
}
```

### 3. Anti-Tamper Bypass

```c
// Hook common anti-tamper functions
static int (*orig_ptrace)(int request, pid_t pid, void* addr, void* data) = NULL;

static int my_ptrace(int request, pid_t pid, void* addr, void* data) {
    // Block anti-debugging ptrace calls
    if (request == 0) {  // PTRACE_TRACEME
        LOGI("[Anti-Tamper Research] Blocked PTRACE_TRACEME");
        errno = EPERM;
        return -1;
    }
    return orig_ptrace(request, pid, addr, data);
}

void init_anti_tamper_bypass() {
    memkit_hook_by_symbol("libc.so", "ptrace",
                          (void*)my_ptrace, (void**)&orig_ptrace);
}
```

---

## Function Tracing

### 1. Basic Function Tracing

```c
// Trace all calls to a function
static void (*orig_targetFunc)(int param1, void* param2) = NULL;

static void my_targetFunc(int param1, void* param2) {
    LOGI("[Trace] targetFunc called:");
    LOGI("  param1 (int): %d", param1);
    LOGI("  param2 (ptr): %p", param2);

    // Call original
    orig_targetFunc(param1, param2);

    LOGI("[Trace] targetFunc returned");
}

void init_tracing() {
    memkit_hook_by_symbol("libtarget.so", "targetFunc",
                          (void*)my_targetFunc, (void**)&orig_targetFunc);
}
```

### 2. Return Value Tracing

```c
static int (*orig_getValue)(void* instance) = NULL;

static int my_getValue(void* instance) {
    int ret = orig_getValue(instance);
    LOGI("[Trace] getValue() returned: %d (0x%x)", ret, ret);
    return ret;
}
```

### 3. String Parameter Logging

```c
static void (*orig_logMessage)(const char* msg) = NULL;

static void my_logMessage(const char* msg) {
    LOGI("[Trace] logMessage: %s", msg ? msg : "(null)");
    orig_logMessage(msg);
}
```

---

## Root Detection Bypass

### 1. File Existence Check Bypass

```c
static int (*orig_access)(const char* pathname, int mode) = NULL;

static int my_access(const char* pathname, int mode) {
    // Common root detection files
    if (strstr(pathname, "su") ||
        strstr(pathname, "magisk") ||
        strstr(pathname, "root")) {
        LOGI("[Root Detection] Blocked access check: %s", pathname);
        errno = ENOENT;
        return -1;
    }
    return orig_access(pathname, mode);
}

void init_root_bypass() {
    memkit_hook_by_symbol("libc.so", "access",
                          (void*)my_access, (void**)&orig_access);
}
```

### 2. Process Check Bypass

```c
static FILE* (*orig_fopen)(const char* filename, const char* mode) = NULL;

static FILE* my_fopen(const char* filename, const char* mode) {
    if (strstr(filename, "/proc/") && strstr(filename, "status")) {
        LOGI("[Root Detection] Blocked proc read: %s", filename);
        return NULL;
    }
    return orig_fopen(filename, mode);
}
```

### 3. System Property Check

```c
static const char* (*orig_getprop)(const char* key, const char* default_value) = NULL;

static const char* my_getprop(const char* key, const char* default_value) {
    if (strstr(key, "ro.debuggable") ||
        strstr(key, "ro.secure") ||
        strstr(key, "ro.build.type")) {
        LOGI("[Root Detection] Blocked getprop: %s", key);
        return default_value;
    }
    return orig_getprop(key, default_value);
}
```

---

## Crypto Analysis

### 1. AES Encryption Logging

```c
static int (*orig_AES_encrypt)(const unsigned char* in, unsigned char* out, void* key) = NULL;

static int my_AES_encrypt(const unsigned char* in, unsigned char* out, void* key) {
    LOGI("[Crypto Research] AES encrypt called");

    // Log plaintext (for research only!)
    LOGI("  Plaintext: %.64s...", in);

    int ret = orig_AES_encrypt(in, out, key);

    // Log ciphertext
    char hex[256];
    for (int i = 0; i < 16 && i < 64; i++) {
        sprintf(hex + (i * 2), "%02x", out[i]);
    }
    LOGI("  Ciphertext: %s", hex);

    return ret;
}
```

### 2. Key Extraction

```c
static int (*orig_AES_set_encrypt_key)(const unsigned char* userKey, int bits, void* key) = NULL;

static int my_AES_set_encrypt_key(const unsigned char* userKey, int bits, void* key) {
    LOGI("[Crypto Research] AES key setup (%d bits)", bits);

    // Log the key (for research only!)
    char hex[128];
    for (int i = 0; i < (bits / 8); i++) {
        sprintf(hex + (i * 2), "%02x", userKey[i]);
    }
    LOGI("  Key: %s", hex);

    return orig_AES_set_encrypt_key(userKey, bits, key);
}
```

---

## Anti-Debugging Bypass

### 1. ptrace Anti-Attach

```c
static int (*orig_ptrace)(int request, pid_t pid, void* addr, void* data) = NULL;

static int my_ptrace(int request, pid_t pid, void* addr, void* data) {
    // Block anti-debugging
    if (request == 0) {  // PTRACE_TRACEME
        LOGI("[Anti-Debug] Blocked PTRACE_TRACEME");
        errno = EPERM;
        return -1;
    }
    return orig_ptrace(request, pid, addr, data);
}
```

### 2. Debugger Detection

```c
static int (*orig_syscall)(long number, ...) = NULL;

static int my_syscall(long number, ...) {
    // Block prctl(PR_SET_DUMPABLE, 0)
    if (number == 161) {  // SYS_prctl
        va_list args;
        va_start(args, number);
        int option = va_arg(args, int);
        va_end(args);

        if (option == 4) {  // PR_SET_DUMPABLE
            LOGI("[Anti-Debug] Blocked PR_SET_DUMPABLE");
            return 0;
        }
    }
    return orig_syscall(number);
}
```

### 3. Timing Check Bypass

```c
static int (*orig_clock_gettime)(clockid_t clock_id, struct timespec* tp) = NULL;

static int my_clock_gettime(clockid_t clock_id, struct timespec* tp) {
    int ret = orig_clock_gettime(clock_id, tp);

    // Log timing checks (often used for anti-debug)
    if (clock_id == CLOCK_PROCESS_CPUTIME_ID) {
        LOGD("[Timing] CPU time check: %ld.%ld", tp->tv_sec, tp->tv_nsec);
    }

    return ret;
}
```

---

## Complete Example: All-in-One

```c
#include "memkit.h"
#include <android/log.h>

#define LOG_TAG "Research-Kit"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

// Storage for stubs
static void* ssl_stub = NULL;
static void* ptrace_stub = NULL;
static void* access_stub = NULL;

// Initialize all bypasses
void init_research_tools() {
    // Initialize hooking
    if (memkit_hook_init(SHADOWHOOK_MODE_UNIQUE, false) != 0) {
        LOGE("Failed to init ShadowHook");
        return;
    }

    LOGI("=== Research Tools Initialized ===");

    // SSL pinning bypass
    ssl_stub = memkit_hook_by_symbol(
        "libssl.so", "SSL_verify_cert_chain",
        (void*)my_SSL_verify_cert_chain, (void**)&orig_SSL_verify_cert_chain
    );

    // Anti-debug bypass
    ptrace_stub = memkit_hook_by_symbol(
        "libc.so", "ptrace",
        (void*)my_ptrace, (void**)&orig_ptrace
    );

    // Root detection bypass
    access_stub = memkit_hook_by_symbol(
        "libc.so", "access",
        (void*)my_access, (void**)&orig_access
    );

    LOGI("Active hooks: SSL=%p, ptrace=%p, access=%p",
         ssl_stub, ptrace_stub, access_stub);
}

// Cleanup
void cleanup_research_tools() {
    if (ssl_stub) memkit_unhook(ssl_stub);
    if (ptrace_stub) memkit_unhook(ptrace_stub);
    if (access_stub) memkit_unhook(access_stub);
}
```

---

## Tips

1. **Always test on emulators first** before real devices
2. **Log everything** during research - you never know what's important
3. **Use UNIQUE mode** unless you need multiple hooks on same function
4. **Free resources** when done to prevent memory leaks
5. **Document your findings** for future reference

---

*These recipes are for security research and educational purposes only.*
