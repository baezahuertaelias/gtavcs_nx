// GLSL 330 port of upstream src/render/shaders/gs_util.hlsl: helper passes for the HW GS renderer (gs_hw.cpp), textured
// blits between targets and solid fills (partial clears). One quad from the vertex id (triangle strip of 4) inside
// `rect` (NDC: left, top, right, bottom).
layout(std140) uniform Quad_b0
{
    vec4 rect;
    vec4 uvRect; // blit: source texture coordinates (0..1) at the left/top and right/bottom edges
    vec4 color;  // fill: colour 0..1
    vec4 params; // x: fill depth 0..1, y: blit writes alpha 1 (the displayed picture)
};

#if defined(ENTRY_vs_main)
out vec2 v_uv;

void main()
{
    vec2 c = vec2(float(gl_VertexID & 1), float(gl_VertexID >> 1));
    rhiPosition(vec4(mix(rect.x, rect.z, c.x), mix(rect.y, rect.w, c.y), 0.0, 1.0));
    v_uv = mix(uvRect.xy, uvRect.zw, c);
}
#endif

#if defined(ENTRY_ps_blit)
in vec2 v_uv;
uniform sampler2D source_t0;
layout(location = 0) out vec4 o_color;

void main()
{
    vec4 t = texture(source_t0, v_uv);
    o_color = vec4(t.rgb, params.y > 0.5 ? 1.0 : t.a);
}
#endif

#if defined(ENTRY_ps_fill_color)
layout(location = 0) out vec4 o_color;

void main()
{
    o_color = color;
}
#endif

#if defined(ENTRY_ps_fill_depth)
// SV_Depth only, no colour output (gs_hw.cpp draws it in a depth-only pass)
void main()
{
    gl_FragDepth = params.x;
}
#endif
