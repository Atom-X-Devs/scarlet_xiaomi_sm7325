/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __LINUX_COMPILER_TYPES_H
#error "Please don't include <linux/compiler-clang.h> directly, include <linux/compiler.h> instead."
#endif

/* Compiler specific definitions for Clang compiler */

/* same as gcc, this was present in clang-2.6 so we can assume it works
 * with any version that can compile the kernel
 */
#define __UNIQUE_ID(prefix) __PASTE(__PASTE(__UNIQUE_ID_, prefix), __COUNTER__)

/* all clang versions usable with the kernel support KASAN ABI version 5 */
#define KASAN_ABI_VERSION 5

/*
 * Clang 22 added preprocessor macros to match GCC, in hopes of eventually
 * dropping __has_feature support for sanitizers:
 * https://github.com/llvm/llvm-project/commit/568c23bbd3303518c5056d7f03444dae4fdc8a9c
 * Create these macros for older versions of clang so that it is easy to clean
 * up once the minimum supported version of LLVM for building the kernel always
 * creates these macros.
 *
 * Note: Checking __has_feature(*_sanitizer) is only true if the feature is
 * enabled. Therefore it is not required to additionally check defined(CONFIG_*)
 * to avoid adding redundant attributes in other configurations.
 */
#if __has_feature(address_sanitizer) && !defined(__SANITIZE_ADDRESS__)
#define __SANITIZE_ADDRESS__
#endif
#if __has_feature(hwaddress_sanitizer) && !defined(__SANITIZE_HWADDRESS__)
#define __SANITIZE_HWADDRESS__
#endif
#if __has_feature(thread_sanitizer) && !defined(__SANITIZE_THREAD__)
#define __SANITIZE_THREAD__
#endif

/*
 * Treat __SANITIZE_HWADDRESS__ the same as __SANITIZE_ADDRESS__ in the kernel.
 */
#ifdef __SANITIZE_HWADDRESS__
#define __SANITIZE_ADDRESS__
#endif

#ifdef __SANITIZE_ADDRESS__
#define __no_sanitize_address \
		__attribute__((no_sanitize("address", "hwaddress")))
#else
#define __no_sanitize_address
#endif

#ifdef __SANITIZE_THREAD__
#define __no_sanitize_thread \
		__attribute__((no_sanitize("thread")))
#else
#define __no_sanitize_thread
#endif

#if __has_feature(undefined_behavior_sanitizer)
/* GCC does not have __SANITIZE_UNDEFINED__ */
#define __no_sanitize_undefined \
		__attribute__((no_sanitize("undefined")))
#else
#define __no_sanitize_undefined
#endif

/*
 * Support for __has_feature(coverage_sanitizer) was added in Clang 13 together
 * with no_sanitize("coverage"). Prior versions of Clang support coverage
 * instrumentation, but cannot be queried for support by the preprocessor.
 */
#if __has_feature(coverage_sanitizer)
#define __no_sanitize_coverage __attribute__((no_sanitize("coverage")))
#else
#define __no_sanitize_coverage
#endif

/*
 * Not all versions of clang implement the the type-generic versions
 * of the builtin overflow checkers. Fortunately, clang implements
 * __has_builtin allowing us to avoid awkward version
 * checks. Unfortunately, we don't know which version of gcc clang
 * pretends to be, so the macro may or may not be defined.
 */
#if __has_builtin(__builtin_mul_overflow) && \
    __has_builtin(__builtin_add_overflow) && \
    __has_builtin(__builtin_sub_overflow)
#define COMPILER_HAS_GENERIC_BUILTIN_OVERFLOW 1
#endif

#if __has_feature(shadow_call_stack)
# define __noscs	__attribute__((__no_sanitize__("shadow-call-stack")))
#else
# define __noscs
#endif

#ifdef CONFIG_LTO_CLANG
#ifdef CONFIG_FTRACE_MCOUNT_RECORD
#define __norecordmcount \
	__attribute__((__section__(".text..ftrace")))
#endif


#define __nocfi		__attribute__((__no_sanitize__("cfi")))
#endif
