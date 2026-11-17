// GLSL 330 port of upstream src/render/shaders/ui.hlsl: Dear ImGui's draw lists through the RHI (render/ui_renderer.cpp).
layout(std140) uniform Ui_b0
{
    vec4 scaleOffset; // ImGui display coordinates -> NDC: xy scale, zw offset
};

#if defined(ENTRY_vs_main)
layout(location = 0) in vec2 i_pos;
layout(location = 1) in vec2 i_uv;
layout(location = 2) in vec4 i_col;
out vec4 v_col;
out vec2 v_uv;

void main()
{
    rhiPosition(vec4(i_pos * scaleOffset.xy + scaleOffset.zw, 0.0, 1.0));
    v_col = i_col;
    v_uv = i_uv;
}
#endif

#if defined(ENTRY_ps_main)
in vec4 v_col;
in vec2 v_uv;
uniform sampler2D uiTexture_t0;
layout(location = 0) out vec4 o_color;

void main()
{
    o_color = v_col * texture(uiTexture_t0, v_uv);
}
#endif
