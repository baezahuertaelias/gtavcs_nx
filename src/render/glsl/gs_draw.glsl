// GLSL 330 port of upstream src/render/shaders/gs_draw.hlsl: GS primitive drawing for the HW GS renderer (gs_hw.cpp).
// Kept line for line with the HLSL (same expressions in the same order) so the two can be read side by side; the
// differences are the GLSL 330 spellings listed in README.md (texelFetch for Load, textureSize for GetDimensions, the
// Gather path by four texelFetch reads, a floor modulo for HLSL's % on negative values).
// Render targets hold PS2 rows top-down (texture row = PS2 row); colours and alpha are 0..255 values / 255.
// DUAL_SOURCE=1 also outputs the blend factor C as the second colour (Src1Alpha blending).
// SAMPLE_MODE: 0 every way, chosen per draw; 1 no texture; 2 nearest; 3 the GS's bilinear by four loads; 4 the hardware
// sampler; 5 the GS's bilinear "by Gather"; 6 a render target read 1:1 at full resolution.
#ifndef SAMPLE_MODE
#define SAMPLE_MODE 0
#endif
#ifndef SHADER_DEPTH
#define SHADER_DEPTH 1
#endif
#ifndef DUAL_SOURCE
#define DUAL_SOURCE 0
#endif

layout(std140) uniform Draw_b0
{
    vec4 targetRtFix;     // target size (PS2 pixels), render-target texel scale x (y: rtOrigin.w), FIX
    ivec4 tmeTfxTccFst;
    ivec4 linearIipFgeAte;
    ivec4 atstArefAfailRt; // ATST, AREF, pass (0: alpha-test passing pixels, 1: failing ones), texture is a render target
    ivec4 texSizeWrap;     // texture w, h, WMS, WMT
    ivec4 region;          // MINU, MAXU, MINV, MAXV
    vec4 rtOrigin;         // xy: texture origin inside the render target; z: pixel-centre offset; w: rt texel scale y
    ivec4 blendCPremul;    // C (0 As, 1 Ad, 2 FIX), premultiply (0 none, 1 rgb *= C, 2 rgb *= 1 + C)
    vec4 fogColor;         // rgb 0..255
    vec4 filterSize;       // a filtered read: xy the footprint of one target PS2 pixel in source texels, zw source size
    vec4 filterDest;       // a filtered read: xy the target's own scale (target pixels per PS2 pixel)
};

#if defined(RHI_VERTEX)
// ---- vertex shaders ------------------------------------------------------------------------------------------------
noperspective out vec3 v_stq;   // STQ, or U/V texels with q = 1 (FST)
noperspective out vec4 v_col;
flat out vec4 v_colFlat;        // the recorder gives flat primitives one colour on every vertex
noperspective out float v_fog;  // 0..1 (1 = no fog)
noperspective out float v_depth;
out float gl_ClipDistance[2];   // native draws: the PS2 clip test's near / far planes

#if defined(ENTRY_vs_main)
layout(location = 0) in vec3 i_pos; // PS2 pixels; depth 0..1
layout(location = 1) in vec3 i_stq;
layout(location = 2) in vec4 i_col;
layout(location = 3) in float i_fog;

void main()
{
    vec2 p = (i_pos.xy + rtOrigin.z) / targetRtFix.xy;
    rhiPosition(vec4(p.x * 2.0 - 1.0, 1.0 - p.y * 2.0, i_pos.z, 1.0));
    v_depth = i_pos.z;
    v_stq = i_stq;
    v_col = i_col;
    v_colFlat = i_col;
    v_fog = i_fog;
    gl_ClipDistance[0] = 1.0;
    gl_ClipDistance[1] = 1.0;
}
#else
// Native draws (packet-level HLE): the VU1 work on the GPU. nat[31] = GS XYOFFSET in pixels (xy), Z format scale (z).
layout(std140) uniform Native_b1
{
    vec4 nat[32];
};

// XYZF2 / STQ / clip planes from the GS window position S and the clip-test position C (shared by the native kinds)
void nativePosition(vec4 S, vec4 C, vec3 st)
{
    float q = 1.0 / S.w;
    vec3 s = S.xyz * q;
    // XYZF2 from FTOI4: X / Y in 12.4 fixed point, Z the integer part
    vec2 xy = trunc(s.xy * 16.0) / 16.0 - nat[31].xy;
    float z = floor(trunc(s.z * 16.0) / 16.0);
    float depth = min(z * nat[31].z, 1.0);
    vec2 p = (xy + rtOrigin.z) / targetRtFix.xy;
    rhiPosition(vec4((p.x * 2.0 - 1.0) * S.w, (1.0 - p.y * 2.0) * S.w, depth * S.w, S.w));
    v_depth = depth;
    gl_ClipDistance[0] = C.z + C.w; // the PS2 clip test's near / far planes
    gl_ClipDistance[1] = C.w - C.z;
    v_stq = st * q;
}

float nativeFog(vec4 fog, float w)
{
    float f = 0.0;
    if (fog.x > 0.5)
        f = clamp(fog.z * w + fog.y, 0.0, 255.0);
    return floor(trunc(f * 16.0) / 16.0) / 255.0;
}
#endif

#if defined(ENTRY_vs_world)
layout(location = 0) in vec3 i_pos; // VU ITOF0 of the unpacked V3-16 position
layout(location = 1) in vec3 i_uv;  // ITOF0 of the V2-8 texture coordinate qword (x, y, z lanes)
layout(location = 2) in uint i_col; // the V4-5 colour's expanded bytes

// VCS prelit world (VU1 0x0030 with stages 0x0da / 0x4e0 / 0x2b6): see upstream docs/game-internals.md
void main()
{
    vec4 S = nat[0] * i_pos.x + nat[1] * i_pos.y + nat[2] * i_pos.z + nat[3];
    vec4 C = nat[11] * i_pos.x + nat[12] * i_pos.y + nat[13] * i_pos.z + nat[14];
    nativePosition(S, C, nat[4].xyz * i_uv.x + nat[5].xyz * i_uv.y + nat[6].xyz * i_uv.z);
    vec4 c = vec4(float(i_col & 0xFFu), float((i_col >> 8) & 0xFFu), float((i_col >> 16) & 0xFFu), float(i_col >> 24));
    v_col = trunc(min(nat[8] + nat[7] * c, nat[9])) / 255.0;
    v_colFlat = v_col;
    v_fog = nativeFog(nat[10], S.w);
}
#endif

#if defined(ENTRY_vs_lit)
layout(location = 0) in vec3 i_pos; // skinned position (the VU's stage 0x5a5 output)
layout(location = 1) in vec3 i_uv;  // ITOF0 of the V2-8 texture coordinate qword
layout(location = 2) in vec3 i_nrm; // skinned normal

// VCS lit / skinned models (VU1 0x0030 with stages 0x5a5 / 0x0da / 0x4ff / 0x2b6): see upstream docs/game-internals.md
void main()
{
    vec4 S = nat[0] * i_pos.x + nat[1] * i_pos.y + nat[2] * i_pos.z + nat[3];
    vec4 C = nat[8] * i_pos.x + nat[9] * i_pos.y + nat[10] * i_pos.z + nat[11];
    nativePosition(S, C, nat[4].xyz * i_uv.x + nat[5].xyz * i_uv.y + nat[6].xyz * i_uv.z);
    vec4 d = max(nat[12] * i_nrm.x + nat[13] * i_nrm.y + nat[14] * i_nrm.z, 0.0);
    vec4 c = trunc(min(nat[20], nat[15] * d.x + nat[16] * d.y + nat[17] * d.z + nat[18] * d.w + nat[19]));
    v_col = vec4(c.rgb, nat[21].x) / 255.0;
    v_colFlat = v_col;
    v_fog = nativeFog(nat[7], S.w);
}
#endif
#endif // RHI_VERTEX

#if defined(ENTRY_ps_main)
// ---- pixel shader --------------------------------------------------------------------------------------------------
noperspective in vec3 v_stq;
noperspective in vec4 v_col;
flat in vec4 v_colFlat;
noperspective in float v_fog;
noperspective in float v_depth;

uniform sampler2D tex_t0; // the hardware sampler (anisotropic option; the gather path's wrapping) is unit 0's sampler

layout(location = 0, index = 0) out vec4 o_color;
#if DUAL_SOURCE
layout(location = 0, index = 1) out vec4 o_blend;
#endif

// HLSL's ((c % size) + size) % size: % on a negative int truncates in HLSL and is undefined in GLSL 330
int floorMod(int c, int size)
{
    int r = abs(c) % size;
    return (c < 0 && r != 0) ? size - r : r;
}

int wrapc(int c, int size, int mode, int mn, int mx)
{
    int r = (c & mn) | mx; // REGION_REPEAT
    // REPEAT. GS sizes are powers of two: a mask; native effect buffers need not be
    if (mode == 0)
    {
        if ((size & (size - 1)) == 0) r = c & (size - 1);
        else r = floorMod(c, size);
    }
    else if (mode == 1) r = clamp(c, 0, size - 1); // CLAMP
    else if (mode == 2) r = clamp(c, mn, mx);    // REGION_CLAMP
    return r;
}

// the render target read: its pixels per PS2 pixel (x, y)
vec2 rtScale2()
{
    return vec2(targetRtFix.z, rtOrigin.w);
}

vec4 fetchTexel(ivec2 t)
{
    t.x = wrapc(t.x, texSizeWrap.x, texSizeWrap.z, region.x, region.y);
    t.y = wrapc(t.y, texSizeWrap.y, texSizeWrap.w, region.z, region.w);
    ivec2 at = t;
    if (atstArefAfailRt.w == 1)
        at = ivec2((vec2(t) + rtOrigin.xy + 0.5) * rtScale2());
    return texelFetch(tex_t0, at, 0) * 255.0;
}

// A render target read 1:1 (a copy of the picture, texture texel = target pixel; atstArefAfailRt.w == 2)
vec4 fetchRtFull(ivec2 c)
{
    vec2 s = rtScale2();
    ivec2 t = ivec2(floor((vec2(c) + 0.5) / s - rtOrigin.xy));
    ivec2 w;
    w.x = wrapc(t.x, texSizeWrap.x, texSizeWrap.z, region.x, region.y);
    w.y = wrapc(t.y, texSizeWrap.y, texSizeWrap.w, region.z, region.w);
    // the target pixels of texel w (non-integer scales: the native frame)
    ivec2 lo = ivec2(ceil((vec2(w) + rtOrigin.xy) * s - 0.5));
    ivec2 hi = ivec2(ceil((vec2(w) + rtOrigin.xy + 1.0) * s - 0.5)) - 1;
    ivec2 at = clamp(c, lo, max(lo, hi));
    return texelFetch(tex_t0, at, 0) * 255.0;
}

vec4 sampleRtFull(vec2 uv)
{
    vec2 h = (uv + rtOrigin.xy) * rtScale2(); // target texels
    if (linearIipFgeAte.x == 0)
        return fetchRtFull(ivec2(floor(h)));
    vec2 p = h - 0.5;
    ivec2 i = ivec2(floor(p));
    vec2 f = p - vec2(i);
    vec4 c00 = fetchRtFull(i), c10 = fetchRtFull(i + ivec2(1, 0));
    vec4 c01 = fetchRtFull(i + ivec2(0, 1)), c11 = fetchRtFull(i + ivec2(1, 1));
    return mix(mix(c00, c10, f.x), mix(c01, c11, f.x), f.y);
}

bool g_gatherMismatch = false; // PS2X_HW_GATHER=verify (sampleTex mode 4)

// The HLSL "gather" texel: what Gather through gs_hw.cpp's nearest sampler of the draw's wrapping (m_gatherSampler,
// chosen from WMS / WMT: REPEAT -> repeat, CLAMP -> clamp; only for power-of-two textures) returns for texel u of an axis
// of `size` texels. GLSL 330 core has no textureGather; the corner Gather samples is exact (power-of-two size), so its
// four texels are these, read with texelFetch.
int gatherWrap(int u, int size, int mode)
{
    return mode == 0 ? (u & (size - 1)) : clamp(u, 0, size - 1);
}

// The GS's bilinear (its texels, its weights) with the 2x2 texels fetched as the HLSL's Gather does (linearIipFgeAte.x
// 3 / 4): a power-of-two texture with plain REPEAT / CLAMP wrapping.
vec4 sampleGather(vec2 uv)
{
    vec2 p = uv - 0.5;
    ivec2 i = ivec2(floor(p));
    vec2 f = p - floor(p);
    ivec2 size = texSizeWrap.xy;
    ivec2 w;
    w.x = texSizeWrap.z == 0 ? (i.x & (size.x - 1)) : clamp(i.x, -1, size.x - 1);
    w.y = texSizeWrap.w == 0 ? (i.y & (size.y - 1)) : clamp(i.y, -1, size.y - 1);
    // the footprint of a Gather at the texel corner (w + 1) / size: texels w and w + 1 on each axis, wrapped / clamped
    // by the sampler
    int u0 = gatherWrap(w.x, size.x, texSizeWrap.z), u1 = gatherWrap(w.x + 1, size.x, texSizeWrap.z);
    int v0 = gatherWrap(w.y, size.y, texSizeWrap.w), v1 = gatherWrap(w.y + 1, size.y, texSizeWrap.w);
    vec4 c00 = texelFetch(tex_t0, ivec2(u0, v0), 0) * 255.0, c10 = texelFetch(tex_t0, ivec2(u1, v0), 0) * 255.0;
    vec4 c01 = texelFetch(tex_t0, ivec2(u0, v1), 0) * 255.0, c11 = texelFetch(tex_t0, ivec2(u1, v1), 0) * 255.0;
#if SAMPLE_MODE == 0
    // mode 4 (PS2X_HW_GATHER=verify, the full shader): the four loads as well; a texel that differs paints the pixel magenta
    if (linearIipFgeAte.x == 4)
        g_gatherMismatch = any(notEqual(c00, fetchTexel(i))) || any(notEqual(c10, fetchTexel(i + ivec2(1, 0)))) ||
                           any(notEqual(c01, fetchTexel(i + ivec2(0, 1)))) || any(notEqual(c11, fetchTexel(i + ivec2(1, 1))));
#endif
    return mix(mix(c00, c10, f.x), mix(c01, c11, f.x), f.y);
}

// the GS's bilinear by four loads (the wrap worked out per tap: region modes, render targets read per PS2 texel)
vec4 sampleBilinear(vec2 uv)
{
    vec2 p = uv - 0.5;
    ivec2 i = ivec2(floor(p));
    vec2 f = p - vec2(i);
    vec4 c00 = fetchTexel(i), c10 = fetchTexel(i + ivec2(1, 0));
    vec4 c01 = fetchTexel(i + ivec2(0, 1)), c11 = fetchTexel(i + ivec2(1, 1));
    return mix(mix(c00, c10, f.x), mix(c01, c11, f.x), f.y);
}

vec2 g_pixel; // SV_Position of the pixel being shaded (main): gl_FragCoord, rows top-down in every render target

// A filtered read of a render target (atstArefAfailRt.w == 3): a tent filter over the texel's footprint
vec4 sampleRtFiltered(vec2 uv)
{
    vec2 ds = max(filterDest.xy, 1.0);
    vec2 P = floor(g_pixel);
    vec2 t = floor((P + 0.5) / ds); // the PS2 texel this target pixel is in
    if (any(notEqual(floor((t + 0.5) * ds), P)))
        return sampleBilinear(uv); // not the pixel a texel read takes
    vec2 s = rtScale2();
    vec2 halfSize = max(filterSize.xy, 0.5);
    ivec2 dims = textureSize(tex_t0, 0);
    ivec2 maxPx = min(dims, ivec2(ceil(filterSize.zw * s))) - 1;
    ivec2 p0 = clamp(ivec2(ceil((uv - halfSize + rtOrigin.xy) * s - 0.5)), ivec2(0, 0), maxPx);
    ivec2 p1 = clamp(ivec2(floor((uv + halfSize + rtOrigin.xy) * s - 0.5)), p0, maxPx);
    ivec2 stride = max(ivec2(1, 1), (p1 - p0 + 16) / 16);
    vec4 sum = vec4(0.0);
    float weights = 0.0;
    for (int y = p0.y; y <= p1.y; y += stride.y)
    {
        float wy = max(0.0, 1.0 - abs((float(y) + 0.5) / s.y - rtOrigin.y - uv.y) / halfSize.y);
        for (int x = p0.x; x <= p1.x; x += stride.x)
        {
            float w = wy * max(0.0, 1.0 - abs((float(x) + 0.5) / s.x - rtOrigin.x - uv.x) / halfSize.x);
            sum += texelFetch(tex_t0, ivec2(x, y), 0) * w;
            weights += w;
        }
    }
    return weights > 0.0 ? sum * (255.0 / weights) : fetchTexel(ivec2(floor(uv)));
}

vec4 sampleTex(vec2 uv)
{
#if SAMPLE_MODE == 2
    return fetchTexel(ivec2(floor(uv)));
#elif SAMPLE_MODE == 3
    return sampleBilinear(uv);
#elif SAMPLE_MODE == 4
    return texture(tex_t0, uv / vec2(texSizeWrap.xy)) * 255.0;
#elif SAMPLE_MODE == 5
    return sampleGather(uv);
#elif SAMPLE_MODE == 6
    return sampleRtFull(uv);
#else
    vec4 r;
    if (atstArefAfailRt.w == 2)
    {
        r = sampleRtFull(uv);
    }
    else if (atstArefAfailRt.w == 3)
    {
        r = sampleRtFiltered(uv); // only the full shader (SAMPLE_MODE 0) has it: drawSampleMode picks that one
    }
    else if (linearIipFgeAte.x == 2)
    {
        // a game texture with plain wrapping, sampled by the hardware (trilinear, anisotropic); the option
        r = texture(tex_t0, uv / vec2(texSizeWrap.xy)) * 255.0;
    }
    else if (linearIipFgeAte.x >= 3)
    {
        r = sampleGather(uv);
    }
    else if (linearIipFgeAte.x == 0)
    {
        r = fetchTexel(ivec2(floor(uv)));
    }
    else
    {
        r = sampleBilinear(uv);
    }
    return r;
#endif
}

void main()
{
    g_pixel = gl_FragCoord.xy;
    // GS Z written directly (SHADER_DEPTH 1) or the rasterised depth used (0): both window-space 0..1
#if SHADER_DEPTH
    gl_FragDepth = v_depth;
#endif
    vec4 vc = floor((linearIipFgeAte.y == 1 ? v_col : v_colFlat) * 255.0 + 0.5);
    vec4 c = vc;
#if SAMPLE_MODE != 1
    if (tmeTfxTccFst.x == 1)
    {
        vec2 uv = (tmeTfxTccFst.w == 1) ? v_stq.xy : (v_stq.xy / v_stq.z) * vec2(texSizeWrap.xy);
        vec4 t = sampleTex(uv);
        int tfx = tmeTfxTccFst.y;
        bool tcc = tmeTfxTccFst.z == 1;
        if (tfx == 0) { c.rgb = t.rgb * vc.rgb / 128.0; c.a = tcc ? t.a * vc.a / 128.0 : vc.a; }
        else if (tfx == 1) { c.rgb = t.rgb; c.a = tcc ? t.a : vc.a; }
        else if (tfx == 2) { c.rgb = t.rgb * vc.rgb / 128.0 + vc.a; c.a = tcc ? t.a + vc.a : vc.a; }
        else { c.rgb = t.rgb * vc.rgb / 128.0 + vc.a; c.a = tcc ? t.a : vc.a; }
    }
#endif
    c = clamp(floor(c), 0.0, 255.0);
    if (linearIipFgeAte.z == 1)
    {
        float f = floor(v_fog * 255.0 + 0.5);
        c.rgb = floor((c.rgb * f + fogColor.rgb * (255.0 - f)) / 255.0);
    }
    bool passed = true;
    if (linearIipFgeAte.w == 1)
    {
        float a = c.a, r = float(atstArefAfailRt.y);
        int atst = atstArefAfailRt.x;
        if (atst == 0) passed = false;
        else if (atst == 2) passed = a < r;
        else if (atst == 3) passed = a <= r;
        else if (atst == 4) passed = a == r;
        else if (atst == 5) passed = a >= r;
        else if (atst == 6) passed = a > r;
        else if (atst == 7) passed = a != r;
    }
    if (atstArefAfailRt.z == 0 && !passed) discard;
    if (atstArefAfailRt.z == 1 && passed) discard;
    int bc = blendCPremul.x;
    float cf = (bc == 0) ? c.a / 128.0 : ((bc == 2) ? targetRtFix.w / 128.0 : 1.0);
    vec3 rgb = c.rgb;
    if (blendCPremul.y == 1) rgb *= cf;
    else if (blendCPremul.y == 2) rgb *= (1.0 + cf);
    o_color = vec4(rgb / 255.0, c.a / 255.0);
    if (g_gatherMismatch)
        o_color = vec4(1.0, 0.0, 1.0, 1.0);
#if DUAL_SOURCE
    o_blend = vec4(min(cf, 1.0));
#endif
}
#endif
