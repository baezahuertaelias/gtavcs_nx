// GLSL 330 port of upstream src/render/shaders/present.hlsl: the game's picture letterboxed into the target, one quad
// from the vertex id (triangle strip of 4). Conventions: README.md.
layout(std140) uniform Params_b0
{
    vec4 rect;  // NDC: left, top, right, bottom
    vec2 uvMax; // the used part of the texture
    vec2 pad;
};

#if defined(ENTRY_vs_main)
out vec2 v_uv;

void main()
{
    vec2 c = vec2(float(gl_VertexID & 1), float(gl_VertexID >> 1));
    rhiPosition(vec4(mix(rect.x, rect.z, c.x), mix(rect.y, rect.w, c.y), 0.0, 1.0));
    v_uv = c * uvMax;
}
#endif

#if defined(ENTRY_ps_main)
in vec2 v_uv;
uniform sampler2D picture_t0;
layout(location = 0) out vec4 o_color;

void main()
{
    o_color = texture(picture_t0, v_uv);
}
#endif
