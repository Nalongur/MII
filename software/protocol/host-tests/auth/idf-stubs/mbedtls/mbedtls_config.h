#pragma once
// Minimal host-only configuration of the pinned ESP-IDF mbedTLS sources.
// Does not override target sdkconfig or enable target entropy/crypto hardware.
#define MBEDTLS_MD_C
#define MBEDTLS_SHA256_C
#define MBEDTLS_PLATFORM_C
#define MBEDTLS_PLATFORM_MEMORY
