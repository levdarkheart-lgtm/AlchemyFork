/* Evaluation-only deterministic target-size reconstruction inputs.
 * The same C implementation is linked into the native-Metal and Rust runners.
 * No image resize, recording, decoder, viewer setting or production renderer is involved.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "input_workload.h"
#include <math.h>
#include <stddef.h>
#include <string.h>

const char* ae_workload_version(void) { return "perspective-mask-texture-hdr-static-v1"; }
static float halton(uint32_t n, uint32_t base) {
    float value=0.f, fraction=1.f;
    while(n) { fraction/=(float)base; value+=fraction*(float)(n%base); n/=base; }
    return value-.5f;
}
void ae_workload_jitter(uint32_t phase,float out[2]) {
    out[0]=halton(phase%AE_PHASES+1,2); out[1]=halton(phase%AE_PHASES+1,3);
}
void ae_workload_projection(float out[16]) {
    memset(out,0,16*sizeof(float));
    out[0]=(float)AE_OUTPUT_HEIGHT/(float)AE_OUTPUT_WIDTH;
    out[5]=1.f; out[11]=-1.f; out[14]=.1f;
}
static uint16_t half_bits(float value) {
    /* IEEE754 binary32 -> binary16, round-to-nearest/ties-even. All authored values are finite. */
    uint32_t bits;memcpy(&bits,&value,sizeof(bits));
    const uint32_t sign=(bits>>16)&0x8000u, fraction=bits&0x7fffffu;
    const int exponent=(int)((bits>>23)&255u)-127+15;
    if(exponent<=0) {
        if(exponent< -10)return (uint16_t)sign;
        const uint32_t m=fraction|0x800000u;const unsigned shift=(unsigned)(14-exponent);
        const uint32_t base=m>>shift, remainder=m&((1u<<shift)-1u), midpoint=1u<<(shift-1u);
        return (uint16_t)(sign+base+(remainder>midpoint || (remainder==midpoint && (base&1u))));
    }
    if(exponent>=31)return (uint16_t)(sign|0x7c00u);
    const uint32_t base=fraction>>13, remainder=fraction&8191u;
    return (uint16_t)(sign+((uint32_t)exponent<<10)+base+(remainder>4096u || (remainder==4096u && (base&1u))));
}
static float fraction(float x) { return x-floorf(x); }
static void sample_at(float x,float y,uint16_t rgba[4],float* depth,uint16_t motion[2]) {
    /* These coordinates locate static geometry before the changing raster jitter.
       Opaque layers are frontoparallel perspective planes; mask texture is attached to the2m plane. */
    const float u=x/(float)AE_INPUT_WIDTH,v=y/(float)AE_INPUT_HEIGHT;
    float r=.035f+.08f*v,g=.075f+.12f*v,b=.21f+.20f*v,glow=0.f,z=0.f;
    if(v>.44f || u<.38f) {
        z=.1f/12.f;
        const float checker=(float)(((int)floorf(u*600.f)^(int)floorf(v*360.f))&1);
        const float mortar=(fraction(u*48.f)<.035f || fraction(v*28.f)<.045f)? .35f:1.f;
        r=(.19f+.13f*checker)*mortar;g=(.17f+.11f*checker)*mortar;b=(.15f+.09f*checker)*mortar;
    }
    /* Opaque near/far structures, hard boundaries and HDR emissive detail. */
    if(u>.70f && v>.62f && v<.88f) {
        z=.1f/4.f;
        const float stripe=fraction(u*320.f+v*110.f)<.32f?1.f:0.f;
        r=.12f+1.8f*stripe;g=.08f+.65f*stripe;b=.07f+.15f*stripe;glow=.08f*stripe;
    }
    const float tx=fraction(u*8.f),ty=fraction(v*4.f);
    const float horizontal=fabsf(tx-.5f),vertical=fabsf(ty-.5f);
    const float extent=.46f*(1.f-1.6f*vertical);
    const float branch=fraction(ty*22.f-horizontal*3.5f);
    const int covered=(ty>.055f && ty<.945f && (horizontal<.006f || (horizontal<extent && branch<.16f)));
    if(covered) {
        z=.1f/2.f;
        const float fine=(float)(((int)floorf(u*1800.f)^(int)floorf(v*1200.f))&1);
        r=.07f+.035f*fine;g=.36f+.34f*fine;b=.055f+.025f*fine;glow=0.f;
    }
    rgba[0]=half_bits(r);rgba[1]=half_bits(g);rgba[2]=half_bits(b);rgba[3]=half_bits(glow);
    *depth=z;motion[0]=0;motion[1]=0;
}
int ae_workload_sample(uint32_t phase,uint32_t x,uint32_t y,uint16_t rgba[4],float* depth,uint16_t motion[2]) {
    if(phase>=AE_PHASES || x>=AE_INPUT_WIDTH || y>=AE_INPUT_HEIGHT || !rgba || !depth || !motion)return 0;
    float jitter[2];ae_workload_jitter(phase,jitter);
    sample_at((float)x+.5f-jitter[0],(float)y+.5f-jitter[1],rgba,depth,motion);return 1;
}
int ae_workload_fill(uint32_t phase,uint16_t* rgba,float* depth,uint16_t* motion) {
    if(phase>=AE_PHASES || !rgba || !depth || !motion)return 0;
    float jitter[2];ae_workload_jitter(phase,jitter);
    for(uint32_t y=0;y<AE_INPUT_HEIGHT;++y)for(uint32_t x=0;x<AE_INPUT_WIDTH;++x) {
        const size_t i=(size_t)y*AE_INPUT_WIDTH+x;
        sample_at((float)x+.5f-jitter[0],(float)y+.5f-jitter[1],rgba+4*i,depth+i,motion+2*i);
    }
    return 1;
}
uint64_t ae_workload_fingerprint(const void* bytes,uint64_t count,uint64_t initial) {
    const unsigned char* b=(const unsigned char*)bytes;uint64_t h=initial;
    for(uint64_t i=0;i<count;++i){h^=b[i];h*=UINT64_C(1099511628211);}return h;
}
