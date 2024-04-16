#pragma once

#include <iostream>
#include <inttypes.h>
#include <stdint.h>
#include <memory>
#include <string>
#include <intrin.h>
#include <assert.h>

// NOTE: If we ever care about other endian platforms we can handle them here.
#define FAST_NTOHS(var) _byteswap_ushort(var)

#if defined(_DEBUG)
#define ASSERT(expr) assert(expr)
#else
#define ASSERT(expr) expr
#endif
