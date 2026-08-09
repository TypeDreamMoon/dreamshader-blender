// Shadows intern/cycles/util/defines.h for the parity harness.
//
// The point of this directory is that the *upstream* kernel files — util/hash.h,
// kernel/svm/noise.h, kernel/svm/fractal_noise.h — are compiled verbatim, unmodified, straight
// from the Blender repository. Only the platform layer beneath them is replaced, and only with
// the handful of declarations those three files actually use.
//
// That is what makes this a real oracle rather than a second transcription by the same hand: a
// typo in the port shows up as a mismatch, because the other side of the comparison is upstream's
// own code rather than my reading of it.

#pragma once

#define CCL_NAMESPACE_BEGIN namespace ref {
#define CCL_NAMESPACE_END }

#define ccl_device inline
#define ccl_device_inline inline
#define ccl_device_forceinline inline
#define ccl_device_noinline
#define ccl_device_noinline_cpu
#define ccl_device_extern
#define ccl_global
#define ccl_constant const
#define ccl_private
#define ccl_restrict
#define ccl_align(x)
#define ccl_may_alias
#define ccl_optional_struct_init
#define ccl_ignore_integer_overflow

#define UNLIKELY(x) (x)
#define LIKELY(x) (x)

#define kernel_assert(x) ((void)0)
