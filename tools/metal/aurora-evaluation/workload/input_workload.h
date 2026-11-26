/* Evaluation-only shared CPU producer. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef AE_INPUT_WORKLOAD_H
#define AE_INPUT_WORKLOAD_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
enum { AE_INPUT_WIDTH=2304, AE_INPUT_HEIGHT=1446, AE_OUTPUT_WIDTH=3456, AE_OUTPUT_HEIGHT=2168, AE_PHASES=18 };
/* The material/scene policy is fixed; this is a cost workload, not quality qualification. */
const char* ae_workload_version(void);
void ae_workload_jitter(uint32_t phase, float out[2]);
void ae_workload_projection(float out[16]);
/* RGBA16Float (linear HDR, glow alpha), reverse-Z R32Float, RG16Float previous-current input pixels. */
int ae_workload_fill(uint32_t phase, uint16_t* rgba, float* depth, uint16_t* motion);
/* Independent point observation for validation; coordinates are input pixel indices. */
int ae_workload_sample(uint32_t phase, uint32_t x, uint32_t y, uint16_t rgba[4], float* depth, uint16_t motion[2]);
/* Stable byte fingerprint for same-input checks, not a cryptographic authenticity claim. */
uint64_t ae_workload_fingerprint(const void* bytes, uint64_t count, uint64_t initial);
#ifdef __cplusplus
}
#endif
#endif
