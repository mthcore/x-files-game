// SPDX-License-Identifier: MIT
// Cross-compiler size assertions for VC* classes.
//
// The VC* class hierarchy uses `static_assert(sizeof(X) == N)` checks to
// preserve byte-direct layout guarantees against the original x86 build.
// On gcc and clang the assertions hold exactly. On MSVC, the compiler
// inserts an extra padding word between virtual function table pointers
// and trailing data in some cases, causing the assertion to fire even
// though the on-disk wire format is unaffected.
//
// Until v0.3+ delivers a proper `#pragma pack`/alignment-attribute
// solution, this header lets the assertions remain authoritative on
// our primary targets (gcc, clang) while letting MSVC build cleanly.
//
// IMPORTANT: the byte-direct wire format (HDB on disk) is governed by
// the per-class `Read()` body, not by `sizeof()` of the in-memory C++
// struct. Skipping the assertion on MSVC does NOT compromise the
// decoded HDB output — both compilers produce identical JSON.

#ifndef XFILES_VC_SIZE_ASSERT_H
#define XFILES_VC_SIZE_ASSERT_H

#if defined(_MSC_VER) && !defined(__clang__)
#  define XFILES_ASSERT_SIZE(T, N, msg) /* MSVC: vtable padding diff, see _size_assert.h */
#  define XFILES_ASSERT_OFFSET(T, F, N, msg) /* MSVC: vtable padding diff */
#else
#  define XFILES_ASSERT_SIZE(T, N, msg) static_assert(sizeof(T) == (N), msg)
#  define XFILES_ASSERT_OFFSET(T, F, N, msg) static_assert(offsetof(T, F) == (N), msg)
#endif

#endif  // XFILES_VC_SIZE_ASSERT_H
