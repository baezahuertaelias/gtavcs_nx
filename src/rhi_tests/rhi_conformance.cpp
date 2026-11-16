// See rhi_conformance.h. Each scene names the rhi::Device methods and shader variants it covers in its comment; the
// method / variant matrix in docs/milestones/M05 is checked against the `covers` calls (tools/rhi_compare.py).
#include "rhi_tests/rhi_conformance.h"

#include "render/gs_shaders.h"
#if defined(VCS_HAVE_OPENGL)
#include "rhi/gl/rhi_gl.h"
#endif

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace conformance
{
    namespace
    {
        using namespace rhi;

        struct Quad // gs_util.hlsl cbuffer Quad
        {
            float rect[4];
            float uvRect[4];
            float color[4];
            float params[4];
        };

        struct Draw // gs_draw.hlsl cbuffer Draw (gs_hw.cpp DrawConstants)
        {
            float targetRtFix[4];
            int32_t tmeTfxTccFst[4];
            int32_t linearIipFgeAte[4];
            int32_t atstArefAfailRt[4];
            int32_t texSizeWrap[4];
            int32_t region[4];
            float rtOrigin[4];
            int32_t blendCPremul[4];
            float fogColor[4];
            float filterSize[4];
            float filterDest[4];
        };
        static_assert(sizeof(Draw) == 176, "Draw constants: 11 x 16 bytes");

        struct PresentParams // present.hlsl cbuffer Params
        {
            float rect[4];
            float uvMax[2];
            float pad[2];
        };

        struct GsVertex // gs_hw.cpp HwVertex: POSITION float3, TEXCOORD0 float3, COLOR ubyte4n, TEXCOORD1 float
        {
            float x, y, z;
            float s, t, q;
            uint8_t r, g, b, a;
            float fog;
        };
        static_assert(sizeof(GsVertex) == 32, "GS vertex");

        struct WorldVertex
        {
            float pos[3];
            float uv[3];
            uint32_t col;
        };
        static_assert(sizeof(WorldVertex) == 28, "world vertex");

        struct LitVertex
        {
            float pos[3];
            float uv[3];
            float nrm[3];
        };

        struct UiVertex // ImDrawVert
        {
            float pos[2];
            float uv[2];
            uint32_t col;
        };

        uint32_t rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
        {
            return static_cast<uint32_t>(r) | (static_cast<uint32_t>(g) << 8) | (static_cast<uint32_t>(b) << 16) |
                   (static_cast<uint32_t>(a) << 24);
        }

        // a deterministic pattern: every texel different, every channel used
        std::vector<uint32_t> pattern(uint32_t w, uint32_t h, uint32_t seed)
        {
            std::vector<uint32_t> p(static_cast<size_t>(w) * h);
            uint32_t x = seed * 2654435761u + 12345u;
            for (auto &v : p)
            {
                x ^= x << 13;
                x ^= x >> 17;
                x ^= x << 5;
                v = x;
            }
            return p;
        }

        class Runner
        {
        public:
            Runner(Device &device, Sink &sink, const Options &options)
                : m_dev(device), m_sink(sink), m_options(options), m_shaders(render::gsShaders(device.backend()))
            {
            }

            int run()
            {
                if (!setUp())
                    return m_failures + 1;
                const struct
                {
                    const char *name;
                    void (Runner::*fn)();
                } scenes[] = {
                    {"textures", &Runner::sceneTextures},
                    {"mips", &Runner::sceneMips},
                    {"samplers", &Runner::sceneSamplers},
                    {"buffers", &Runner::sceneBuffers},
                    {"topology", &Runner::sceneTopology},
                    {"blend", &Runner::sceneBlend},
                    {"dual_source", &Runner::sceneDualSource},
                    {"write_mask", &Runner::sceneWriteMask},
                    {"depth", &Runner::sceneDepth},
                    {"passes", &Runner::scenePasses},
                    {"copies", &Runner::sceneCopies},
                    {"msaa", &Runner::sceneMsaa},
                    {"conventions", &Runner::sceneConventions},
                    {"pixel_centre", &Runner::scenePixelCentre},
                    {"present_ui", &Runner::scenePresentUi},
                    {"gs_util", &Runner::sceneGsUtil},
                    {"gs_modes", &Runner::sceneGsModes},
                    {"gs_tfx", &Runner::sceneGsTfx},
                    {"gs_wrap", &Runner::sceneGsWrap},
                    {"gs_rt_reads", &Runner::sceneGsRtReads},
                    {"gs_native", &Runner::sceneGsNative},
                    {"gl_checks", &Runner::sceneGlChecks},
                    {"free_threaded", &Runner::sceneFreeThreaded},
                    {"churn", &Runner::sceneChurn},
                };
                for (const auto &s : scenes)
                {
                    m_scene = s.name;
                    log("scene " + m_scene);
                    (this->*s.fn)();
                    m_dev.present(Device::PresentMode::Immediate);
                    if (m_sink.frame)
                        m_sink.frame();
                    deviceErrors(m_scene);
                }
                return m_failures;
            }

        private:
            // ---- plumbing ---------------------------------------------------------------------------------------
            void log(const std::string &text)
            {
                if (m_sink.log)
                    m_sink.log(text);
            }

            void check(const std::string &name, bool passed, const std::string &detail = "")
            {
                if (!passed)
                    ++m_failures;
                if (m_sink.check)
                    m_sink.check(m_scene + "/" + name, passed, detail);
            }

            void covers(const char *what) { log(std::string("covers ") + what); }

            void deviceErrors(const std::string &where)
            {
#if defined(VCS_HAVE_OPENGL)
                if (m_dev.backend() == Backend::OpenGL)
                {
                    const uint32_t errors = openGLCheckErrors(m_dev, where.c_str());
                    check("no_gl_errors", errors == 0, std::to_string(errors) + " GL errors");
                }
#else
                (void)where;
#endif
            }

            std::unique_ptr<Texture> texture(uint32_t w, uint32_t h, Format f, uint32_t usage, const char *name,
                                             const void *data = nullptr, uint32_t pitch = 0, uint32_t samples = 1, uint32_t mips = 1)
            {
                TextureDesc d;
                d.width = w;
                d.height = h;
                d.format = f;
                d.usage = usage;
                d.name = name;
                d.samples = samples;
                d.mipLevels = mips;
                auto t = m_dev.createTexture(d, data, pitch);
                if (!t)
                    check(std::string("create_") + name, false, "createTexture returned null");
                return t;
            }

            std::unique_ptr<Texture> target(uint32_t w, uint32_t h, const char *name)
            {
                return texture(w, h, Format::RGBA8, UsageRenderTarget | UsageSampled, name);
            }

            std::unique_ptr<Texture> depthTarget(uint32_t w, uint32_t h, const char *name, uint32_t samples = 1)
            {
                return texture(w, h, Format::D32F, UsageDepth | UsageSampled, name, nullptr, 0, samples);
            }

            std::vector<uint8_t> read(Texture &t, uint32_t x = 0, uint32_t y = 0, uint32_t w = 0, uint32_t h = 0)
            {
                w = w ? w : t.desc().width;
                h = h ? h : t.desc().height;
                std::vector<uint8_t> out(static_cast<size_t>(w) * h * 4, 0xCD);
                if (!m_dev.readTexture(t, x, y, w, h, out.data()))
                    check(std::string("read_") + (t.desc().name ? t.desc().name : "?"), false, "readTexture failed");
                return out;
            }

            void emit(const std::string &part, Texture &t, const std::string &tolerance = "exact")
            {
                Image img;
                img.name = m_scene + "/" + part;
                img.kind = t.desc().format == Format::RGBA8 ? "rgba8" : t.desc().format == Format::R32F ? "r32f" : "d32f";
                img.width = t.desc().width;
                img.height = t.desc().height;
                img.tolerance = tolerance;
                img.data = read(t);
                if (m_sink.image)
                    m_sink.image(img);
            }

            uint32_t pixel(const std::vector<uint8_t> &data, uint32_t w, uint32_t x, uint32_t y)
            {
                uint32_t v = 0;
                std::memcpy(&v, data.data() + (static_cast<size_t>(y) * w + x) * 4, 4);
                return v;
            }

            float depthAt(const std::vector<uint8_t> &data, uint32_t w, uint32_t x, uint32_t y)
            {
                float v = 0;
                std::memcpy(&v, data.data() + (static_cast<size_t>(y) * w + x) * 4, 4);
                return v;
            }

            static std::string hex(uint32_t v)
            {
                char b[16];
                std::snprintf(b, sizeof(b), "%08x", v);
                return b;
            }

            static bool near(uint32_t a, uint32_t b, int tol)
            {
                for (int i = 0; i < 4; ++i)
                    if (std::abs(static_cast<int>((a >> (8 * i)) & 0xFF) - static_cast<int>((b >> (8 * i)) & 0xFF)) > tol)
                        return false;
                return true;
            }

            std::unique_ptr<Buffer> constants(size_t size, const char *name)
            {
                return m_dev.createBuffer({size, BufferUsage::Constant, true, name});
            }

            bool setUp()
            {
                // the gs_util / present / ui families and samplers used by every scene
                PipelineDesc q;
                q.vertex = m_shaders.quadVS;
                q.topology = Topology::TriangleStrip;
                q.pixel = m_shaders.fillColorPS;
                q.name = "fill";
                m_fill = m_dev.createPipeline(q);
                q.pixel = m_shaders.blitPS;
                q.name = "blit";
                m_blit = m_dev.createPipeline(q);
                PipelineDesc p;
                p.vertex = m_shaders.presentVS;
                p.pixel = m_shaders.presentPS;
                p.topology = Topology::TriangleStrip;
                p.name = "present";
                m_present = m_dev.createPipeline(p);
                m_quadCb = constants(sizeof(Quad), "quad constants");
                m_drawCb = constants(sizeof(Draw), "draw constants");
                m_presentCb = constants(sizeof(PresentParams), "present constants");
                m_nativeCb = constants(32 * 16, "native constants");
                m_nearest = m_dev.createSampler({Filter::Nearest, Wrap::Clamp, Wrap::Clamp});
                m_linear = m_dev.createSampler({Filter::Linear, Wrap::Clamp, Wrap::Clamp});
                m_gsVb = m_dev.createBuffer({sizeof(GsVertex) * 4096, BufferUsage::Vertex, true, "gs vertices"});
                const bool ok = m_fill && m_blit && m_present && m_quadCb && m_drawCb && m_presentCb && m_nativeCb && m_nearest &&
                                m_linear && m_gsVb;
                check("set_up", ok, "base pipelines / buffers / samplers");
                return ok;
            }

            // a solid fill of a target rectangle in pixels (gs_util fill_color), through the given pipeline
            void fillRect(Pipeline &pipe, uint32_t tw, uint32_t th, float x0, float y0, float x1, float y1, const float color[4],
                          float depth = 0.0f)
            {
                Quad c{};
                c.rect[0] = x0 / tw * 2.0f - 1.0f;
                c.rect[1] = 1.0f - y0 / th * 2.0f;
                c.rect[2] = x1 / tw * 2.0f - 1.0f;
                c.rect[3] = 1.0f - y1 / th * 2.0f;
                std::memcpy(c.color, color, sizeof(c.color));
                c.params[0] = depth;
                m_dev.updateBuffer(*m_quadCb, &c, sizeof(c));
                m_dev.setPipeline(pipe);
                m_dev.setConstants(0, *m_quadCb);
                m_dev.draw(4);
            }

            void fill(uint32_t tw, uint32_t th, float x0, float y0, float x1, float y1, const float color[4])
            {
                fillRect(*m_fill, tw, th, x0, y0, x1, y1, color);
            }

            // the present shader: `tex` letterboxed into rect (pixels) of a tw x th target, uv 0..uvMax
            void presentRect(Texture &tex, Sampler &s, uint32_t tw, uint32_t th, float x0, float y0, float x1, float y1, float uvU,
                             float uvV, Pipeline *pipe = nullptr)
            {
                PresentParams c{};
                c.rect[0] = x0 / tw * 2.0f - 1.0f;
                c.rect[1] = 1.0f - y0 / th * 2.0f;
                c.rect[2] = x1 / tw * 2.0f - 1.0f;
                c.rect[3] = 1.0f - y1 / th * 2.0f;
                c.uvMax[0] = uvU;
                c.uvMax[1] = uvV;
                m_dev.updateBuffer(*m_presentCb, &c, sizeof(c));
                m_dev.setPipeline(pipe ? *pipe : *m_present);
                m_dev.setConstants(0, *m_presentCb);
                m_dev.setTexture(0, &tex, &s);
                m_dev.draw(4);
                m_dev.setTexture(0, nullptr, nullptr);
            }

            void begin(Texture *color, Texture *depth = nullptr, bool clear = true, float r = 0, float g = 0, float b = 0, float a = 0,
                       bool clearDepth = false, float depthValue = 0.0f, bool depthOnly = false, const char *name = nullptr)
            {
                PassDesc p;
                p.color = color;
                p.depth = depth;
                p.depthOnly = depthOnly;
                p.clearColor = clear;
                p.clearRgba[0] = r;
                p.clearRgba[1] = g;
                p.clearRgba[2] = b;
                p.clearRgba[3] = a;
                p.clearDepth = clearDepth;
                p.clearDepthValue = depthValue;
                p.name = name ? name : m_scene.c_str();
                m_dev.beginPass(p);
            }

            // ---- GS draw helpers --------------------------------------------------------------------------------
            std::unique_ptr<Pipeline> gsPipeline(int mode, bool dual, bool shaderDepth, const BlendState &blend, const DepthState &depth,
                                                 Topology topo = Topology::Triangles)
            {
                PipelineDesc d;
                d.vertex = m_shaders.drawVS;
                d.pixel = shaderDepth ? m_shaders.drawPS[mode][dual ? 1 : 0] : m_shaders.drawPSNoDepth[mode][dual ? 1 : 0];
                d.attributes = {
                    {"POSITION", 0, VertexFormat::Float3, offsetof(GsVertex, x)},
                    {"TEXCOORD", 0, VertexFormat::Float3, offsetof(GsVertex, s)},
                    {"COLOR", 0, VertexFormat::UByte4Norm, offsetof(GsVertex, r)},
                    {"TEXCOORD", 1, VertexFormat::Float1, offsetof(GsVertex, fog)},
                };
                d.stride = sizeof(GsVertex);
                d.topology = topo;
                d.blend = blend;
                d.depth = depth;
                d.name = "gs draw";
                auto p = m_dev.createPipeline(d);
                if (!p)
                    check("gs_pipeline_" + std::to_string(mode) + (dual ? "_dual" : "") + (shaderDepth ? "" : "_noz"), false,
                          "createPipeline returned null");
                return p;
            }

            Draw drawConstants(uint32_t tw, uint32_t th)
            {
                Draw d{};
                d.targetRtFix[0] = static_cast<float>(tw);
                d.targetRtFix[1] = static_cast<float>(th);
                d.targetRtFix[2] = 1.0f;
                d.targetRtFix[3] = 128.0f;
                d.rtOrigin[3] = 1.0f;
                d.linearIipFgeAte[1] = 1;
                d.atstArefAfailRt[0] = 1; // ALWAYS
                d.blendCPremul[0] = 1;    // C = Ad (unused unless blended)
                return d;
            }

            // two triangles covering pixels [x0, x1) x [y0, y1) in PS2 pixels; texture coordinates per corner (FST U/V
            // texels or STQ), one colour, fog, depth
            void gsRect(std::vector<GsVertex> &v, float x0, float y0, float x1, float y1, float s0, float t0, float s1, float t1,
                        uint32_t color, float z = 0.5f, float fog = 1.0f, float q = 1.0f)
            {
                auto vert = [&](float x, float y, float s, float t) {
                    GsVertex g{};
                    g.x = x;
                    g.y = y;
                    g.z = z;
                    g.s = s * q;
                    g.t = t * q;
                    g.q = q;
                    g.r = color & 0xFF;
                    g.g = (color >> 8) & 0xFF;
                    g.b = (color >> 16) & 0xFF;
                    g.a = color >> 24;
                    g.fog = fog;
                    return g;
                };
                const GsVertex a = vert(x0, y0, s0, t0), b = vert(x1, y0, s1, t0), c = vert(x0, y1, s0, t1), d = vert(x1, y1, s1, t1);
                v.insert(v.end(), {a, b, c, b, d, c});
            }

            void gsDraw(Pipeline &pipe, const Draw &c, const std::vector<GsVertex> &v, Texture *tex = nullptr, Sampler *s = nullptr)
            {
                if (v.empty())
                    return;
                m_dev.updateBuffer(*m_drawCb, &c, sizeof(c));
                m_dev.updateBuffer(*m_gsVb, v.data(), v.size() * sizeof(GsVertex));
                m_dev.setPipeline(pipe);
                m_dev.setConstants(0, *m_drawCb);
                m_dev.setVertexBuffer(*m_gsVb, 0);
                m_dev.setTexture(0, tex, tex ? (s ? s : m_nearest.get()) : nullptr);
                m_dev.draw(static_cast<uint32_t>(v.size()));
                m_dev.setTexture(0, nullptr, nullptr);
            }

            // ---- scenes -----------------------------------------------------------------------------------------
            // createTexture (data, row pitch), updateTexture (sub-rectangle, pitch), readTexture (sub-rectangle, pitch),
            // RGBA8 / R32F formats, UsageDynamic rewrites; the bounds checks of the GL backend in gl_checks
            void sceneTextures()
            {
                covers("createTexture updateTexture readTexture Format::RGBA8 Format::R32F UsageDynamic");
                const uint32_t w = 37, h = 23;
                auto src = pattern(w, h, 1);
                // initial data with a padded row pitch
                const uint32_t pitch = w * 4 + 12;
                std::vector<uint8_t> padded(static_cast<size_t>(pitch) * h, 0xEE);
                for (uint32_t y = 0; y < h; ++y)
                    std::memcpy(padded.data() + static_cast<size_t>(y) * pitch, src.data() + static_cast<size_t>(y) * w, w * 4);
                auto t = texture(w, h, Format::RGBA8, UsageSampled, "upload", padded.data(), pitch);
                if (!t)
                    return;
                auto back = read(*t);
                check("upload_pitch_roundtrip", std::memcmp(back.data(), src.data(), back.size()) == 0);
                emit("upload", *t);
                // a sub-rectangle update with a padded pitch, then a padded sub-rectangle read
                auto patch = pattern(20, 11, 2);
                const uint32_t ppitch = 20 * 4 + 8;
                std::vector<uint8_t> pp(static_cast<size_t>(ppitch) * 11, 0x11);
                for (uint32_t y = 0; y < 11; ++y)
                    std::memcpy(pp.data() + static_cast<size_t>(y) * ppitch, patch.data() + static_cast<size_t>(y) * 20, 80);
                m_dev.updateTexture(*t, 5, 7, 20, 11, pp.data(), ppitch);
                for (uint32_t y = 0; y < 11; ++y)
                    std::memcpy(src.data() + static_cast<size_t>(y + 7) * w + 5, patch.data() + static_cast<size_t>(y) * 20, 80);
                back = read(*t);
                check("update_subrect", std::memcmp(back.data(), src.data(), back.size()) == 0);
                const uint32_t rpitch = 16 * 4 + 20;
                std::vector<uint8_t> part(static_cast<size_t>(rpitch) * 9, 0x5A);
                const bool readOk = m_dev.readTexture(*t, 3, 4, 16, 9, part.data(), rpitch);
                bool partOk = readOk;
                for (uint32_t y = 0; y < 9 && partOk; ++y)
                {
                    partOk = std::memcmp(part.data() + static_cast<size_t>(y) * rpitch, src.data() + static_cast<size_t>(y + 4) * w + 3, 64) == 0;
                    for (uint32_t i = 64; i < rpitch && partOk && y + 1 < 9; ++i)
                        partOk = part[static_cast<size_t>(y) * rpitch + i] == 0x5A; // the padding is left alone
                }
                check("read_subrect_pitch", partOk);
                emit("updated", *t);
                // R32F: values a unorm format would lose
                std::vector<float> f(16 * 16);
                for (size_t i = 0; i < f.size(); ++i)
                    f[i] = (static_cast<float>(i) - 100.0f) * 0.371f + (i % 7 == 0 ? 1.0e6f : 0.0f);
                auto r = texture(16, 16, Format::R32F, UsageSampled, "r32f", f.data());
                if (r)
                {
                    auto rb = read(*r);
                    check("r32f_roundtrip", std::memcmp(rb.data(), f.data(), rb.size()) == 0);
                    emit("r32f", *r);
                }
                // dynamic textures are rewritten whole
                auto dyn = texture(32, 32, Format::RGBA8, UsageSampled | UsageDynamic, "dynamic");
                if (dyn)
                {
                    auto a = pattern(32, 32, 3), b = pattern(32, 32, 4);
                    m_dev.updateTexture(*dyn, 0, 0, 32, 32, a.data());
                    m_dev.updateTexture(*dyn, 0, 0, 32, 32, b.data());
                    auto db = read(*dyn);
                    check("dynamic_rewrite", std::memcmp(db.data(), b.data(), db.size()) == 0);
                }
            }

            // createTextureLevels (given chain), createTexture mipLevels 0 + data (generated chain), generateMips of a
            // render target, level selection by the sampler (present shader)
            void sceneMips()
            {
                covers("createTextureLevels createTexture(mipLevels=0) generateMips");
                const uint32_t colors[7] = {rgba(255, 0, 0, 255), rgba(0, 255, 0, 255), rgba(0, 0, 255, 255), rgba(255, 255, 0, 255),
                                            rgba(0, 255, 255, 255), rgba(255, 0, 255, 255), rgba(128, 128, 128, 255)};
                std::vector<std::vector<uint32_t>> levels;
                std::vector<const void *> ptrs;
                for (uint32_t i = 0; i < 7; ++i)
                {
                    const uint32_t s = 64u >> i;
                    levels.emplace_back(static_cast<size_t>(s) * s, colors[i]);
                }
                for (auto &l : levels)
                    ptrs.push_back(l.data());
                TextureDesc d;
                d.width = d.height = 64;
                d.name = "levels";
                auto tex = m_dev.createTextureLevels(d, ptrs.data(), 7);
                check("create_levels", tex != nullptr);
                if (!tex)
                    return;
                // drawn at 64 >> i: the sampler picks level i (nearest mip, exact)
                for (uint32_t i = 0; i < 7; ++i)
                {
                    const uint32_t s = 64u >> i;
                    auto t = target(s, s, "mip level");
                    begin(t.get());
                    presentRect(*tex, *m_nearest, s, s, 0, 0, static_cast<float>(s), static_cast<float>(s), 1, 1);
                    m_dev.endPass();
                    auto px = read(*t);
                    check("level_" + std::to_string(i), pixel(px, s, s / 2, s / 2) == colors[i], hex(pixel(px, s, s / 2, s / 2)));
                    emit("level_" + std::to_string(i), *t);
                }
                // a generated chain from data: the box filter is the driver's (D3D11 GenerateMips, vkCmdBlitImage, GL
                // glGenerateMipmap): averaged 2x2 per level; a smooth gradient keeps implementations within rounding
                std::vector<uint32_t> grad(64 * 64);
                for (uint32_t y = 0; y < 64; ++y)
                    for (uint32_t x = 0; x < 64; ++x)
                        grad[y * 64 + x] = rgba(static_cast<uint8_t>(x * 4), static_cast<uint8_t>(y * 4), static_cast<uint8_t>((x + y) * 2), 255);
                auto gen = texture(64, 64, Format::RGBA8, UsageSampled, "generated", grad.data(), 0, 1, 0);
                if (gen)
                    for (uint32_t i : {1u, 3u})
                    {
                        const uint32_t s = 64u >> i;
                        auto t = target(s, s, "generated level");
                        begin(t.get());
                        presentRect(*gen, *m_nearest, s, s, 0, 0, static_cast<float>(s), static_cast<float>(s), 1, 1);
                        m_dev.endPass();
                        emit("generated_" + std::to_string(i), *t, "unorm:2");
                    }
                // generateMips on a render target after drawing into it
                auto rt = texture(32, 32, Format::RGBA8, UsageSampled | UsageRenderTarget, "rt mips", nullptr, 0, 1, 0);
                if (rt)
                {
                    begin(rt.get(), nullptr, true, 0, 0, 0, 1);
                    const float red[4] = {1, 0, 0, 1}, blue[4] = {0, 0, 1, 1};
                    fill(32, 32, 0, 0, 16, 32, red);
                    fill(32, 32, 16, 0, 32, 32, blue);
                    m_dev.endPass();
                    m_dev.generateMips(*rt);
                    auto t = target(4, 4, "rt level 3");
                    begin(t.get());
                    presentRect(*rt, *m_nearest, 4, 4, 0, 0, 4, 4, 1, 1);
                    m_dev.endPass();
                    auto px = read(*t);
                    check("rt_generate_mips", pixel(px, 4, 0, 0) == rgba(255, 0, 0, 255) && pixel(px, 4, 3, 3) == rgba(0, 0, 255, 255),
                          hex(pixel(px, 4, 0, 0)) + " " + hex(pixel(px, 4, 3, 3)));
                    emit("rt_level3", *t, "unorm:1");
                }
            }

            // createSampler: Nearest / Linear, Clamp / Repeat per axis, anisotropy (implementation-defined footprint)
            void sceneSamplers()
            {
                covers("createSampler Filter::Nearest Filter::Linear Wrap::Clamp Wrap::Repeat anisotropy");
                auto src = pattern(4, 4, 7);
                for (auto &p : src)
                    p |= 0xFF000000u;
                auto tex = texture(4, 4, Format::RGBA8, UsageSampled, "sampler source", src.data());
                if (!tex)
                    return;
                const struct
                {
                    const char *name;
                    SamplerDesc desc;
                    const char *tolerance;
                } cases[] = {
                    {"nearest_clamp", {Filter::Nearest, Wrap::Clamp, Wrap::Clamp}, "exact"},
                    {"nearest_repeat", {Filter::Nearest, Wrap::Repeat, Wrap::Repeat}, "exact"},
                    {"nearest_mixed", {Filter::Nearest, Wrap::Repeat, Wrap::Clamp}, "exact"},
                    {"linear_clamp", {Filter::Linear, Wrap::Clamp, Wrap::Clamp}, "unorm:2"},
                    {"linear_repeat", {Filter::Linear, Wrap::Repeat, Wrap::Repeat}, "unorm:2"},
                };
                for (const auto &c : cases)
                {
                    auto s = m_dev.createSampler(c.desc);
                    auto t = target(32, 32, "sampler target");
                    if (!s || !t)
                        continue;
                    begin(t.get());
                    // uv 0..2: two texture widths over the target - the wrap mode shows in the right / bottom halves
                    presentRect(*tex, *s, 32, 32, 0, 0, 32, 32, 2, 2);
                    m_dev.endPass();
                    emit(c.name, *t, c.tolerance);
                    if (c.desc.filter == Filter::Nearest)
                    {
                        auto px = read(*t);
                        // pixel (x, y) samples uv ((x + .5) / 16): texel floor((x + .5) / 4) wrapped or clamped
                        bool ok = true;
                        for (uint32_t y = 0; y < 32 && ok; ++y)
                            for (uint32_t x = 0; x < 32 && ok; ++x)
                            {
                                uint32_t u = x / 4, v = y / 4;
                                u = c.desc.wrapU == Wrap::Repeat ? u % 4 : std::min(u, 3u);
                                v = c.desc.wrapV == Wrap::Repeat ? v % 4 : std::min(v, 3u);
                                ok = pixel(px, 32, x, y) == src[v * 4 + u];
                            }
                        check(std::string(c.name) + "_texels", ok);
                    }
                }
                // anisotropic: a mipmapped checker on a receding plane (STQ perspective) through the GS hardware-sampler
                // path (gs_draw SAMPLE_MODE 4); the footprint is the implementation's (spec: "approximation")
                std::vector<uint32_t> checker(64 * 64);
                for (uint32_t y = 0; y < 64; ++y)
                    for (uint32_t x = 0; x < 64; ++x)
                        checker[y * 64 + x] = ((x / 4 + y / 4) & 1) ? rgba(255, 255, 255, 128) : rgba(0, 0, 0, 128);
                auto ck = texture(64, 64, Format::RGBA8, UsageSampled, "checker", checker.data(), 0, 1, 0);
                auto aniso = m_dev.createSampler({Filter::Linear, Wrap::Repeat, Wrap::Repeat, 16});
                auto tri = m_dev.createSampler({Filter::Linear, Wrap::Repeat, Wrap::Repeat, 0});
                auto pipe = gsPipeline(4, false, true, {}, {});
                if (!ck || !aniso || !tri || !pipe)
                    return;
                for (int i = 0; i < 2; ++i)
                {
                    auto t = target(64, 64, "aniso target");
                    begin(t.get());
                    Draw c = drawConstants(64, 64);
                    c.tmeTfxTccFst[0] = 1;
                    c.tmeTfxTccFst[1] = 1; // DECAL
                    c.tmeTfxTccFst[2] = 1;
                    c.texSizeWrap[0] = c.texSizeWrap[1] = 64;
                    c.linearIipFgeAte[0] = 2;
                    std::vector<GsVertex> v;
                    // the far edge (top) at q 0.125, the near edge at 1: S/T/Q interpolated linearly, divided per pixel
                    auto vert = [&](float x, float y, float s, float t, float q) {
                        GsVertex g{};
                        g.x = x, g.y = y, g.z = 0.5f, g.s = s * q, g.t = t * q, g.q = q, g.r = g.g = g.b = 128, g.a = 128, g.fog = 1;
                        return g;
                    };
                    const GsVertex a = vert(0, 0, 0, 0, 0.125f), b = vert(64, 0, 8, 0, 0.125f), cc = vert(0, 64, 0, 4, 1.0f),
                                   d = vert(64, 64, 8, 4, 1.0f);
                    v = {a, b, cc, b, d, cc};
                    gsDraw(*pipe, c, v, ck.get(), i ? aniso.get() : tri.get());
                    m_dev.endPass();
                    // per pixel the LOD / footprint is the implementation's ("an approximation"); per band of 8 rows the
                    // mean and spread are not (tools/rhi_compare.py bands; ANV, lavapipe, iris and nouveau agree within
                    // 1.1 / 6)
                    emit(i ? "anisotropic_16" : "trilinear", *t, "bands:8,mean:3,stdev:8");
                    // the mip chain is used: the far band (q 1/8) is filtered towards the checker's grey (spread under 25; 0 trilinear, 17-18 anisotropic on every implementation),
                    // the middle (rows 24..31) still shows it (spread over 30); level 0 alone would alias the far band
                    auto px = read(*t);
                    auto spread = [&](uint32_t y0) {
                        double sum = 0, sq = 0;
                        for (uint32_t y = y0; y < y0 + 8; ++y)
                            for (uint32_t x = 0; x < 64; ++x)
                            {
                                const double v = pixel(px, 64, x, y) & 0xFF;
                                sum += v;
                                sq += v * v;
                            }
                        const double n = 512.0, m = sum / n;
                        return std::sqrt(std::max(0.0, sq / n - m * m));
                    };
                    const double far = spread(0), mid = spread(24);
                    check(std::string(i ? "anisotropic_16" : "trilinear") + "_uses_mips", far < 25.0 && mid > 30.0,
                          "far " + std::to_string(far) + " middle " + std::to_string(mid));
                }
            }

            // createBuffer (dynamic / static, with / without data), updateBuffer discard semantics between draws in one
            // pass, setVertexBuffer offset, setIndexBuffer 16 / 32 bit, drawIndexed firstIndex / baseVertex, setConstants
            void sceneBuffers()
            {
                covers("createBuffer updateBuffer setVertexBuffer(offset) setIndexBuffer(16,32) drawIndexed(firstIndex,baseVertex) setConstants");
                auto pipe = gsPipeline(1, false, false, {}, {});
                auto t = target(64, 64, "buffers");
                if (!pipe || !t)
                    return;
                begin(t.get());
                Draw c = drawConstants(64, 64);
                // two updates of the same vertex buffer and constant buffer inside one pass: each draw keeps its own data
                std::vector<GsVertex> v;
                gsRect(v, 0, 0, 16, 16, 0, 0, 0, 0, rgba(255, 0, 0, 255));
                gsDraw(*pipe, c, v);
                v.clear();
                gsRect(v, 16, 0, 32, 16, 0, 0, 0, 0, rgba(0, 255, 0, 255));
                gsDraw(*pipe, c, v);
                // constant updates between draws: the fog colour (FGE) changes per draw
                c.linearIipFgeAte[2] = 1;
                for (int i = 0; i < 2; ++i)
                {
                    c.fogColor[0] = i ? 0.0f : 255.0f;
                    c.fogColor[2] = i ? 255.0f : 0.0f;
                    v.clear();
                    gsRect(v, 32.0f + 16 * i, 0, 48.0f + 16 * i, 16, 0, 0, 0, 0, rgba(255, 255, 255, 255), 0.5f, 0.0f);
                    gsDraw(*pipe, c, v);
                }
                c.linearIipFgeAte[2] = 0;
                // a static vertex buffer made with data, then rewritten (UpdateSubresource), drawn from an offset
                std::vector<GsVertex> sv;
                gsRect(sv, 0, 0, 1, 1, 0, 0, 0, 0, 0); // a dummy vertex block before the drawn one (the offset skips it)
                gsRect(sv, 0, 16, 16, 32, 0, 0, 0, 0, rgba(0, 0, 255, 255));
                auto sb = m_dev.createBuffer({sv.size() * sizeof(GsVertex), BufferUsage::Vertex, false, "static vertices"}, sv.data());
                std::vector<GsVertex> sv2 = sv;
                for (size_t i = 6; i < 12; ++i)
                    sv2[i].r = 255;
                if (sb)
                {
                    m_dev.updateBuffer(*sb, sv2.data(), sv2.size() * sizeof(GsVertex));
                    m_dev.updateBuffer(*m_drawCb, &c, sizeof(c));
                    m_dev.setPipeline(*pipe);
                    m_dev.setConstants(0, *m_drawCb);
                    m_dev.setVertexBuffer(*sb, 6 * sizeof(GsVertex));
                    m_dev.draw(6);
                }
                // indexed: 16-bit with firstIndex, 32-bit with baseVertex
                std::vector<GsVertex> iv;
                auto corner = [&](float x, float y, uint32_t col) {
                    GsVertex g{};
                    g.x = x, g.y = y, g.z = 0.5f, g.q = 1, g.r = col & 0xFF, g.g = (col >> 8) & 0xFF, g.b = (col >> 16) & 0xFF, g.a = 255;
                    g.fog = 1;
                    iv.push_back(g);
                };
                corner(16, 16, rgba(255, 128, 0, 255)), corner(32, 16, rgba(255, 128, 0, 255)), corner(16, 32, rgba(255, 128, 0, 255)),
                    corner(32, 32, rgba(255, 128, 0, 255));
                corner(32, 16, rgba(0, 128, 255, 255)), corner(48, 16, rgba(0, 128, 255, 255)), corner(32, 32, rgba(0, 128, 255, 255)),
                    corner(48, 32, rgba(0, 128, 255, 255));
                auto ivb = m_dev.createBuffer({iv.size() * sizeof(GsVertex), BufferUsage::Vertex, true, "indexed vertices"}, iv.data());
                const uint16_t i16[] = {9, 9, 9, 0, 1, 2, 1, 3, 2};
                const uint32_t i32[] = {0, 1, 2, 1, 3, 2};
                auto ib16 = m_dev.createBuffer({sizeof(i16), BufferUsage::Index, false, "indices 16"}, i16);
                auto ib32 = m_dev.createBuffer({sizeof(i32), BufferUsage::Index, true, "indices 32"});
                if (ivb && ib16 && ib32)
                {
                    m_dev.updateBuffer(*ib32, i32, sizeof(i32));
                    m_dev.setPipeline(*pipe);
                    m_dev.setConstants(0, *m_drawCb);
                    m_dev.setVertexBuffer(*ivb, 0);
                    m_dev.setIndexBuffer(*ib16, false);
                    m_dev.drawIndexed(6, 3, 0);
                    m_dev.setIndexBuffer(*ib32, true);
                    m_dev.drawIndexed(6, 0, 4);
                }
                m_dev.endPass();
                auto px = read(*t);
                const struct
                {
                    uint32_t x, y, expect;
                    const char *what;
                } probes[] = {
                    {8, 8, rgba(255, 0, 0, 255), "vb_update_1"},     {24, 8, rgba(0, 255, 0, 255), "vb_update_2"},
                    {40, 8, rgba(255, 0, 0, 255), "cb_update_1"},    {56, 8, rgba(0, 0, 255, 255), "cb_update_2"},
                    {8, 24, rgba(255, 0, 255, 255), "static_offset"}, {24, 24, rgba(255, 128, 0, 255), "index16_first"},
                    {40, 24, rgba(0, 128, 255, 255), "index32_base"},
                };
                for (const auto &p : probes)
                    check(p.what, pixel(px, 64, p.x, p.y) == p.expect, hex(pixel(px, 64, p.x, p.y)));
                emit("result", *t);
            }

            // Topology::Triangles / TriangleStrip / Lines / Points through the GS draw pipeline (gs_hw.cpp's modes)
            void sceneTopology()
            {
                covers("Topology::Triangles Topology::TriangleStrip Topology::Lines Topology::Points");
                const Topology topos[] = {Topology::Triangles, Topology::TriangleStrip, Topology::Lines, Topology::Points};
                const char *names[] = {"triangles", "strip", "lines", "points"};
                for (int i = 0; i < 4; ++i)
                {
                    auto pipe = gsPipeline(1, false, false, {}, {}, topos[i]);
                    auto t = target(32, 32, "topology");
                    if (!pipe || !t)
                        continue;
                    begin(t.get());
                    std::vector<GsVertex> v;
                    auto add = [&](float x, float y, uint32_t col) {
                        GsVertex g{};
                        g.x = x, g.y = y, g.z = 0.5f, g.q = 1, g.r = col & 0xFF, g.g = (col >> 8) & 0xFF, g.b = (col >> 16) & 0xFF;
                        g.a = 255, g.fog = 1;
                        v.push_back(g);
                    };
                    // the same 8 vertices: a zigzag of distinct colours, at pixel centres for lines and points
                    for (int k = 0; k < 8; ++k)
                        add(2.5f + k * 4.0f, (k & 1) ? 26.5f : 4.5f, rgba(static_cast<uint8_t>(32 * k), static_cast<uint8_t>(255 - 32 * k), 128, 255));
                    Draw c = drawConstants(32, 32);
                    gsDraw(*pipe, c, v);
                    m_dev.endPass();
                    // lines: GL and D3D both use the diamond-exit rule but may pick a different pixel at an exact tie
                    emit(names[i], *t, i == 2 ? "unorm:255,pixels:8" : "exact");
                }
            }

            // BlendState: every factor (not Src1, see dual_source) for colour and alpha, every op, the blend constant
            void sceneBlend()
            {
                covers("BlendState BlendFactor BlendOp setBlendConstant");
                const BlendFactor factors[] = {BlendFactor::Zero,     BlendFactor::One,         BlendFactor::SrcAlpha, BlendFactor::InvSrcAlpha,
                                               BlendFactor::DstAlpha, BlendFactor::InvDstAlpha, BlendFactor::Constant, BlendFactor::InvConstant};
                const BlendOp ops[] = {BlendOp::Add, BlendOp::Subtract, BlendOp::ReverseSubtract, BlendOp::Min, BlendOp::Max};
                const char *opNames[] = {"add", "subtract", "reverse_subtract", "min", "max"};
                // k / 255 inputs (no float -> unorm rounding ties before the blend)
                const float dst[4] = {51 / 255.0f, 102 / 255.0f, 153 / 255.0f, 102 / 255.0f}, src[4] = {204 / 255.0f, 77 / 255.0f, 26 / 255.0f, 179 / 255.0f};
                const float constant[4] = {0.25f, 0.5f, 0.75f, 0.6f};
                for (int o = 0; o < 5; ++o)
                    for (int alpha = 0; alpha < 2; ++alpha)
                    {
                        auto t = target(64, 64, "blend");
                        if (!t)
                            return;
                        std::vector<std::unique_ptr<Pipeline>> pipes;
                        begin(t.get(), nullptr, true, dst[0], dst[1], dst[2], dst[3]);
                        m_dev.setBlendConstant(constant);
                        for (int s = 0; s < 8; ++s)
                            for (int d = 0; d < 8; ++d)
                            {
                                PipelineDesc pd;
                                pd.vertex = m_shaders.quadVS;
                                pd.pixel = m_shaders.fillColorPS;
                                pd.topology = Topology::TriangleStrip;
                                pd.blend.enable = true;
                                // alpha == 0: the grid on colour (alpha One / Zero); 1: on alpha (colour One / Zero)
                                pd.blend.srcColor = alpha ? BlendFactor::One : factors[s];
                                pd.blend.dstColor = alpha ? BlendFactor::Zero : factors[d];
                                pd.blend.srcAlpha = alpha ? factors[s] : BlendFactor::One;
                                pd.blend.dstAlpha = alpha ? factors[d] : BlendFactor::Zero;
                                pd.blend.colorOp = alpha ? BlendOp::Add : ops[o];
                                pd.blend.alphaOp = alpha ? ops[o] : BlendOp::Add;
                                pd.name = "blend grid";
                                pipes.push_back(m_dev.createPipeline(pd));
                                if (pipes.back())
                                    fillRect(*pipes.back(), 64, 64, s * 8.0f, d * 8.0f, s * 8.0f + 8, d * 8.0f + 8, src);
                            }
                        m_dev.endPass();
                        // a blend rounds once to 8 bits; implementations may round the intermediate differently
                        emit(std::string(opNames[o]) + (alpha ? "_alpha" : "_color"), *t, "unorm:1");
                        if (o == 0 && !alpha)
                        {
                            // one analytic probe: src * SrcAlpha + dst * InvSrcAlpha (s 2, d 3)
                            auto px = read(*t);
                            const float a = src[3];
                            const uint32_t expect = rgba(static_cast<uint8_t>(std::lround((src[0] * a + dst[0] * (1 - a)) * 255)),
                                                         static_cast<uint8_t>(std::lround((src[1] * a + dst[1] * (1 - a)) * 255)),
                                                         static_cast<uint8_t>(std::lround((src[2] * a + dst[2] * (1 - a)) * 255)),
                                                         static_cast<uint8_t>(std::lround(src[3] * 255)));
                            check("src_alpha_over", near(pixel(px, 64, 2 * 8 + 4, 3 * 8 + 4), expect, 1),
                                  hex(pixel(px, 64, 20, 28)) + " expected " + hex(expect));
                        }
                    }
            }

            // Src1Alpha / InvSrc1Alpha (dual-source): gs_hw.cpp's GS blend equations (A - B) * C + D for C from the
            // second output, with every gs_draw dual variant's output path
            void sceneDualSource()
            {
                covers("BlendFactor::Src1Alpha BlendFactor::InvSrc1Alpha gs_draw DUAL_SOURCE=1");
                // gs_hw.cpp toBlend: (src, dst, op, premul) for the equations that need C as a factor
                struct Eq
                {
                    const char *name;
                    BlendFactor src, dst;
                    BlendOp op;
                    int premul;
                } eqs[] = {
                    {"cs_c_plus_cd_1mc", BlendFactor::Src1Alpha, BlendFactor::InvSrc1Alpha, BlendOp::Add, 0},
                    {"cs_1pc_minus_cd_c", BlendFactor::One, BlendFactor::Src1Alpha, BlendOp::Subtract, 2},
                    {"cs_minus_cd_times_c", BlendFactor::Src1Alpha, BlendFactor::Src1Alpha, BlendOp::Subtract, 0},
                    {"cs_1mc_plus_cd_c", BlendFactor::InvSrc1Alpha, BlendFactor::Src1Alpha, BlendOp::Add, 0},
                    {"cd_1mc", BlendFactor::Zero, BlendFactor::InvSrc1Alpha, BlendOp::Add, 0},
                    {"cs_minus_cd_c", BlendFactor::One, BlendFactor::Src1Alpha, BlendOp::ReverseSubtract, 0},
                    {"cs_plus_cd_c", BlendFactor::One, BlendFactor::Src1Alpha, BlendOp::Add, 0},
                    {"cd_c", BlendFactor::Zero, BlendFactor::Src1Alpha, BlendOp::Add, 0},
                };
                for (int mode = 0; mode < render::GsShaders::kSampleModes; ++mode)
                {
                    auto t = target(64, 64, "dual");
                    if (!t)
                        return;
                    begin(t.get(), nullptr, true, 64 / 255.0f, 128 / 255.0f, 191 / 255.0f, 128 / 255.0f);
                    for (int e = 0; e < 8; ++e)
                    {
                        BlendState b;
                        b.enable = true;
                        b.srcColor = eqs[e].src;
                        b.dstColor = eqs[e].dst;
                        b.colorOp = eqs[e].op;
                        b.srcAlpha = BlendFactor::One;
                        b.dstAlpha = BlendFactor::Zero;
                        for (int z = 0; z < 2; ++z)
                        {
                            auto pipe = gsPipeline(mode, true, z == 0, b, {});
                            if (!pipe)
                                continue;
                            // C from FIX (bc 2) at 4 levels down the column, As (bc 0) in the last
                            for (int k = 0; k < 4; ++k)
                            {
                                Draw c = drawConstants(64, 64);
                                c.blendCPremul[0] = k < 3 ? 2 : 0;
                                c.blendCPremul[1] = eqs[e].premul;
                                c.targetRtFix[3] = static_cast<float>(32 + 48 * k);
                                std::vector<GsVertex> v;
                                gsRect(v, e * 8.0f + z * 4, k * 16.0f, e * 8.0f + z * 4 + 4, k * 16.0f + 16, 0, 0, 0, 0,
                                       rgba(200, 100, 50, static_cast<uint8_t>(40 + 60 * k)));
                                gsDraw(*pipe, c, v);
                            }
                        }
                    }
                    m_dev.endPass();
                    emit("mode_" + std::to_string(mode), *t, "unorm:1");
                }
            }

            // BlendState::writeMask: all 16 masks over a cleared target
            void sceneWriteMask()
            {
                covers("BlendState::writeMask");
                auto t = target(64, 16, "mask");
                if (!t)
                    return;
                begin(t.get(), nullptr, true, 26 / 255.0f, 51 / 255.0f, 77 / 255.0f, 102 / 255.0f);
                std::vector<std::unique_ptr<Pipeline>> pipes;
                // k / 255 exactly: no float -> unorm rounding tie (an exact .5 rounds either way by the APIs' rules)
                const float color[4] = {230 / 255.0f, 204 / 255.0f, 179 / 255.0f, 153 / 255.0f};
                for (int m = 0; m < 16; ++m)
                {
                    PipelineDesc pd;
                    pd.vertex = m_shaders.quadVS;
                    pd.pixel = m_shaders.fillColorPS;
                    pd.topology = Topology::TriangleStrip;
                    pd.blend.writeMask = static_cast<uint8_t>(m);
                    pd.name = "mask";
                    pipes.push_back(m_dev.createPipeline(pd));
                    if (pipes.back())
                        fillRect(*pipes.back(), 64, 16, m * 4.0f, 0, m * 4.0f + 4, 16, color);
                }
                m_dev.endPass();
                auto px = read(*t);
                bool ok = true;
                for (int m = 0; m < 16 && ok; ++m)
                {
                    const uint8_t on[4] = {230, 204, 179, 153}, off[4] = {26, 51, 77, 102};
                    uint8_t e[4];
                    for (int c = 0; c < 4; ++c)
                        e[c] = (m >> c) & 1 ? on[c] : off[c];
                    ok = near(pixel(px, 64, m * 4 + 2, 8), rgba(e[0], e[1], e[2], e[3]), 1);
                }
                check("masks", ok);
                emit("masks", *t);
            }

            // DepthState (every Compare, test / write), D32F targets, depth readback, clearDepth, shader depth (SV_Depth)
            // and rasterised depth (NoZ variants) giving the same values, depth clamp (depthClip false)
            void sceneDepth()
            {
                covers("DepthState Compare Format::D32F clearDepth readTexture(D32F) depthClip SHADER_DEPTH");
                const Compare funcs[] = {Compare::Never, Compare::Less, Compare::Equal, Compare::LessEqual,
                                         Compare::Greater, Compare::NotEqual, Compare::GreaterEqual, Compare::Always};
                for (int shaderDepth = 0; shaderDepth < 2; ++shaderDepth)
                {
                    auto t = target(64, 48, "depth color");
                    auto d = depthTarget(64, 48, "depth");
                    if (!t || !d)
                        return;
                    begin(t.get(), d.get(), true, 0, 0, 0, 1, true, 0.5f);
                    std::vector<std::unique_ptr<Pipeline>> pipes;
                    for (int f = 0; f < 8; ++f)
                    {
                        pipes.push_back(gsPipeline(1, false, shaderDepth != 0, {}, {true, true, funcs[f]}));
                        for (int k = 0; k < 3; ++k)
                        {
                            // z 0.25 / 0.5 / 0.75 against the cleared 0.5; exact binary fractions
                            std::vector<GsVertex> v;
                            gsRect(v, f * 8.0f, k * 16.0f, f * 8.0f + 8, k * 16.0f + 16, 0, 0, 0, 0,
                                   rgba(static_cast<uint8_t>(30 * f), static_cast<uint8_t>(80 * k + 40), 200, 255), 0.25f * (k + 1));
                            if (pipes.back())
                                gsDraw(*pipes.back(), drawConstants(64, 48), v);
                        }
                    }
                    m_dev.endPass();
                    auto px = read(*t);
                    auto dz = read(*d);
                    bool ok = true;
                    std::string detail;
                    for (int f = 0; f < 8; ++f)
                        for (int k = 0; k < 3; ++k)
                        {
                            const float z = 0.25f * (k + 1);
                            bool pass = false;
                            switch (funcs[f])
                            {
                            case Compare::Never: pass = false; break;
                            case Compare::Less: pass = z < 0.5f; break;
                            case Compare::Equal: pass = z == 0.5f; break;
                            case Compare::LessEqual: pass = z <= 0.5f; break;
                            case Compare::Greater: pass = z > 0.5f; break;
                            case Compare::NotEqual: pass = z != 0.5f; break;
                            case Compare::GreaterEqual: pass = z >= 0.5f; break;
                            case Compare::Always: pass = true; break;
                            }
                            const uint32_t x = f * 8 + 4, y = k * 16 + 8;
                            const bool drawn = pixel(px, 64, x, y) != rgba(0, 0, 0, 255);
                            const float depth = depthAt(dz, 64, x, y);
                            if (drawn != pass || depth != (pass ? z : 0.5f))
                            {
                                ok = false;
                                detail += std::to_string(f) + "/" + std::to_string(k) + ": depth " + std::to_string(depth) + " ";
                            }
                        }
                    const std::string part = shaderDepth ? "shader_depth" : "raster_depth";
                    check(part, ok, detail);
                    emit(part + "_color", *t);
                    emit(part + "_depth", *d);
                }
                // depth written without a test (D3D11: DepthEnable with ALWAYS), and a test without writes
                auto t = target(16, 16, "depth only write");
                auto d = depthTarget(16, 16, "depth only write");
                if (!t || !d)
                    return;
                begin(t.get(), d.get(), true, 0, 0, 0, 1, true, 1.0f);
                auto writeOnly = gsPipeline(1, false, false, {}, {false, true, Compare::Never});
                auto testOnly = gsPipeline(1, false, false, {}, {true, false, Compare::Less});
                std::vector<GsVertex> v;
                gsRect(v, 0, 0, 16, 16, 0, 0, 0, 0, rgba(255, 0, 0, 255), 0.75f);
                if (writeOnly)
                    gsDraw(*writeOnly, drawConstants(16, 16), v);
                v.clear();
                gsRect(v, 0, 0, 8, 16, 0, 0, 0, 0, rgba(0, 255, 0, 255), 0.5f); // passes 0.5 < 0.75, writes nothing
                gsRect(v, 8, 0, 16, 16, 0, 0, 0, 0, rgba(0, 0, 255, 255), 0.875f); // fails
                if (testOnly)
                    gsDraw(*testOnly, drawConstants(16, 16), v);
                m_dev.endPass();
                auto px = read(*t);
                auto dz = read(*d);
                check("write_without_test", depthAt(dz, 16, 12, 8) == 0.75f && depthAt(dz, 16, 4, 8) == 0.75f);
                check("test_without_write", pixel(px, 16, 4, 8) == rgba(0, 255, 0, 255) && pixel(px, 16, 12, 8) == rgba(255, 0, 0, 255));
                emit("write_test_color", *t);
                emit("write_test_depth", *d);
            }

            // beginPass / endPass, clears (colour, depth, values), viewport and scissor rectangles (top-left origin),
            // depth-only passes (gs_util fill_depth), pass markers
            void scenePasses()
            {
                covers("beginPass endPass setViewport setScissor PassDesc::clearColor PassDesc::clearDepth PassDesc::depthOnly pushMarker popMarker");
                auto t = target(64, 64, "pass");
                if (!t)
                    return;
                begin(t.get(), nullptr, true, 0.0f, 0.0f, 1.0f, 1.0f);
                m_dev.pushMarker("nested marker");
                // a viewport in the top-left quarter: the whole-NDC fill lands there
                m_dev.setViewport(0, 0, 32, 32);
                const float red[4] = {1, 0, 0, 1}, green[4] = {0, 1, 0, 1}, white[4] = {1, 1, 1, 1};
                fill(64, 64, 0, 0, 64, 64, red);
                // a viewport in the bottom-right quarter with a scissor cutting its right half
                m_dev.setViewport(32, 32, 32, 32);
                m_dev.setScissor(32, 32, 16, 32);
                fill(64, 64, 0, 0, 64, 64, green);
                // a fractional viewport (D3D takes floats): a whole-NDC fill of a 10.5 x 10.5 rectangle
                m_dev.setScissor(0, 0, 64, 64);
                m_dev.setViewport(40.25f, 4.5f, 10.5f, 10.5f);
                fill(64, 64, 0, 0, 64, 64, white);
                m_dev.popMarker();
                m_dev.endPass();
                auto px = read(*t);
                check("viewport_top_left", pixel(px, 64, 10, 10) == rgba(255, 0, 0, 255) && pixel(px, 64, 40, 10) != rgba(255, 0, 0, 255));
                check("scissor", pixel(px, 64, 40, 40) == rgba(0, 255, 0, 255) && pixel(px, 64, 56, 40) == rgba(0, 0, 255, 255));
                emit("viewport_scissor", *t, "unorm:255,pixels:24"); // the fractional viewport's edge pixels
                // depth-only pass: fill_depth writes SV_Depth, no colour target
                auto d = depthTarget(32, 32, "depth only");
                PipelineDesc q;
                q.vertex = m_shaders.quadVS;
                q.pixel = m_shaders.fillDepthPS;
                q.topology = Topology::TriangleStrip;
                q.depth = {false, true, Compare::Always};
                q.blend.writeMask = 0;
                q.name = "fill depth";
                auto fd = m_dev.createPipeline(q);
                if (d && fd)
                {
                    begin(nullptr, d.get(), false, 0, 0, 0, 0, true, 0.125f, true);
                    fillRect(*fd, 32, 32, 8, 8, 24, 24, white, 0.625f);
                    m_dev.endPass();
                    auto dz = read(*d);
                    check("depth_only_fill", depthAt(dz, 32, 16, 16) == 0.625f && depthAt(dz, 32, 2, 2) == 0.125f,
                          std::to_string(depthAt(dz, 32, 16, 16)) + " " + std::to_string(depthAt(dz, 32, 2, 2)));
                    emit("depth_only", *d);
                }
                // a second pass on the same target without a clear keeps its contents
                begin(t.get(), nullptr, false);
                fill(64, 64, 0, 56, 8, 64, white);
                m_dev.endPass();
                px = read(*t);
                check("load_without_clear", pixel(px, 64, 10, 10) == rgba(255, 0, 0, 255) && pixel(px, 64, 4, 60) == rgba(255, 255, 255, 255));
            }

            // copyTexture (colour and depth regions), readback of the copy
            void sceneCopies()
            {
                covers("copyTexture(RGBA8) copyTexture(D32F)");
                auto src = pattern(40, 30, 9);
                auto a = texture(40, 30, Format::RGBA8, UsageSampled | UsageRenderTarget, "copy source", src.data());
                auto b = target(40, 30, "copy target");
                if (!a || !b)
                    return;
                begin(b.get(), nullptr, true, 0, 0, 0, 0);
                m_dev.endPass();
                m_dev.copyTexture(*b, 3, 5, *a, 10, 7, 21, 13);
                auto px = read(*b);
                bool ok = true;
                for (uint32_t y = 0; y < 30 && ok; ++y)
                    for (uint32_t x = 0; x < 40 && ok; ++x)
                    {
                        const bool in = x >= 3 && x < 24 && y >= 5 && y < 18;
                        ok = pixel(px, 40, x, y) == (in ? src[(y - 5 + 7) * 40 + (x - 3 + 10)] : 0u);
                    }
                check("color_region", ok);
                emit("color", *b);
                // depth: a pass writes depth, then a region copies to another depth texture
                auto d0 = depthTarget(32, 32, "copy depth source");
                auto d1 = depthTarget(32, 32, "copy depth target");
                auto pipe = gsPipeline(1, false, false, {}, {false, true, Compare::Always});
                if (!d0 || !d1 || !pipe)
                    return;
                auto c = target(32, 32, "copy depth color");
                begin(c.get(), d0.get(), true, 0, 0, 0, 0, true, 0.0f);
                std::vector<GsVertex> v;
                for (int k = 0; k < 4; ++k)
                    gsRect(v, k * 8.0f, 0, k * 8.0f + 8, 32, 0, 0, 0, 0, rgba(255, 255, 255, 255), 0.125f * (k + 1));
                gsDraw(*pipe, drawConstants(32, 32), v);
                m_dev.endPass();
                begin(nullptr, d1.get(), false, 0, 0, 0, 0, true, 1.0f, true);
                m_dev.endPass();
                m_dev.copyTexture(*d1, 0, 0, *d0, 8, 0, 16, 32);
                auto dz = read(*d1);
                check("depth_region", depthAt(dz, 32, 4, 4) == 0.25f && depthAt(dz, 32, 12, 4) == 0.375f && depthAt(dz, 32, 20, 4) == 1.0f);
                emit("depth", *d1);
            }

            // samples > 1: multisampled colour + depth targets, resolveTexture (box resolve; sample positions are the
            // implementation's), MSAA texture creation limits
            void sceneMsaa()
            {
                covers("TextureDesc::samples resolveTexture");
                auto ms = texture(32, 32, Format::RGBA8, UsageRenderTarget, "msaa color", nullptr, 0, 4);
                auto md = depthTarget(32, 32, "msaa depth", 4);
                auto rs = target(32, 32, "resolved");
                auto pipe = gsPipeline(1, false, false, {}, {true, true, Compare::Less});
                if (!ms || !md || !rs || !pipe)
                    return;
                begin(ms.get(), md.get(), true, 0, 0, 0, 1, true, 1.0f);
                std::vector<GsVertex> v;
                auto add = [&](float x, float y, uint32_t col, float z) {
                    GsVertex g{};
                    g.x = x, g.y = y, g.z = z, g.q = 1, g.r = col & 0xFF, g.g = (col >> 8) & 0xFF, g.b = (col >> 16) & 0xFF, g.a = 255;
                    g.fog = 1;
                    v.push_back(g);
                };
                add(2, 2, rgba(255, 255, 255, 255), 0.5f), add(30, 6, rgba(255, 255, 255, 255), 0.5f), add(8, 30, rgba(255, 255, 255, 255), 0.5f);
                // an interior quad in front: fully covered pixels are exact after the resolve
                gsRect(v, 12, 12, 20, 20, 0, 0, 0, 0, rgba(255, 0, 0, 255), 0.25f);
                gsDraw(*pipe, drawConstants(32, 32), v);
                m_dev.endPass();
                m_dev.resolveTexture(*rs, *ms);
                auto px = read(*rs);
                check("interior_exact", pixel(px, 32, 16, 16) == rgba(255, 0, 0, 255) && pixel(px, 32, 30, 30) == rgba(0, 0, 0, 255));
                // edge pixels average their covered samples; the Switch's 25 differing pixels are the 127.5 of a
                // half-covered pixel rounded down (7f) where the reference rounds up (80): unorm:1
                emit("resolved", *rs, "unorm:1"); // half-covered edge pixels average 255 and 0: 127.5 rounds either way
            }

            // Y origin (row 0 = top in every texture), clip-space depth 0..1, gl_FragCoord vs SV_Position, UNORM / sRGB
            void sceneConventions()
            {
                covers("conventions: Y origin, clip depth, pixel centres, UNORM encode");
                auto t = target(32, 16, "origin");
                auto d = depthTarget(32, 16, "origin depth");
                auto pipe = gsPipeline(1, false, false, {}, {false, true, Compare::Always});
                if (!t || !d || !pipe)
                    return;
                begin(t.get(), d.get(), true, 0, 0, 0, 1, true, 1.0f);
                // a right triangle in the top-left corner: (0,0) (16,0) (0,8) in PS2 pixels; depth 0.25 / 0.75 corners
                std::vector<GsVertex> v;
                auto add = [&](float x, float y, float z) {
                    GsVertex g{};
                    g.x = x, g.y = y, g.z = z, g.q = 1, g.r = 255, g.g = 128, g.b = 0, g.a = 255, g.fog = 1;
                    v.push_back(g);
                };
                add(0, 0, 0.25f), add(16, 0, 0.25f), add(0, 8, 0.75f);
                gsDraw(*pipe, drawConstants(32, 16), v);
                m_dev.endPass();
                auto px = read(*t);
                auto dz = read(*d);
                check("row0_is_top", pixel(px, 32, 1, 0) == rgba(255, 128, 0, 255) && pixel(px, 32, 1, 15) == rgba(0, 0, 0, 255));
                check("left_is_x0", pixel(px, 32, 14, 0) == rgba(255, 128, 0, 255) && pixel(px, 32, 30, 0) == rgba(0, 0, 0, 255));
                // the rasterised depth at row 0 (pixel centre y 0.5 of 8: z = 0.25 + 0.5 / 8 * 0.5)
                const float z0 = depthAt(dz, 32, 0, 0);
                check("clip_depth_0_to_1", std::fabs(z0 - 0.28125f) < 1e-6f, std::to_string(z0));
                emit("origin_color", *t);
                emit("origin_depth", *d, "float:1e-6");
                // UNORM: 0.6 -> 153 and 128 / 255 -> 128, no sRGB encode on write or decode on read
                auto u = target(4, 4, "unorm");
                begin(u.get());
                const float c[4] = {0.6f, 128 / 255.0f, 0.2f, 1.0f};
                fill(4, 4, 0, 0, 4, 4, c);
                m_dev.endPass();
                px = read(*u);
                check("unorm_no_srgb", pixel(px, 4, 1, 1) == rgba(153, 128, 51, 255), hex(pixel(px, 4, 1, 1)));
            }

            // The game's own configuration: gs_hw.cpp moves every GS draw by half a pixel (rtOrigin.z = 0.5,
            // gsPixelCentreOffset), so integer GS edges lie exactly on pixel centres and D3D's top-left fill rule
            // decides them: a sprite from (x0, y0) to (x1, y1) covers pixels x0..x1-1 and y0..y1-1, adjacent sprites
            // meet without gaps or double coverage, and a 1:1 copy (u = x + 0.5... sampled at x) is exact. The first M05
            // review found nouveau dropping the top row of such an edge with the shader Y flip (GL's lower-left rule).
            void scenePixelCentre()
            {
                covers("pixel-centre ties (rtOrigin.z 0.5): sprites, shared edges, triangles, lines, points, 1:1 copy");
                auto add = gsPipeline(1, false, false, {true, BlendFactor::One, BlendFactor::One, BlendOp::Add, BlendFactor::One,
                                                         BlendFactor::One, BlendOp::Add, 0xF}, {});
                auto plain = gsPipeline(1, false, false, {}, {});
                auto t = target(64, 48, "pixel centre");
                if (!add || !plain || !t)
                    return;
                begin(t.get(), nullptr, true, 0, 0, 0, 0);
                Draw c = drawConstants(64, 48);
                c.rtOrigin[2] = 0.5f;
                // a 4 x 3 grid of sprites sharing edges (each adds 64 to red), and lone sprites of odd sizes
                std::vector<GsVertex> v;
                const int xs[] = {2, 7, 9, 16, 22}, ys[] = {3, 6, 11, 13};
                for (int j = 0; j + 1 < 4; ++j)
                    for (int i = 0; i + 1 < 5; ++i)
                        gsRect(v, static_cast<float>(xs[i]), static_cast<float>(ys[j]), static_cast<float>(xs[i + 1]),
                               static_cast<float>(ys[j + 1]), 0, 0, 0, 0, rgba(64, 0, 0, 0));
                gsRect(v, 30, 2, 33, 5, 0, 0, 0, 0, rgba(0, 64, 0, 0));
                gsRect(v, 35, 1, 36, 2, 0, 0, 0, 0, rgba(0, 64, 0, 0)); // a single pixel
                gsDraw(*add, c, v);
                m_dev.endPass();
                auto px = read(*t);
                bool grid = true, lone = true;
                std::string detail;
                for (uint32_t y = 0; y < 48; ++y)
                    for (uint32_t x = 0; x < 64; ++x)
                    {
                        const uint32_t r = pixel(px, 64, x, y) & 0xFF, g = (pixel(px, 64, x, y) >> 8) & 0xFF;
                        const bool inGrid = x >= 2 && x < 22 && y >= 3 && y < 13;
                        if (r != (inGrid ? 64u : 0u))
                        {
                            if (grid)
                                detail += "grid " + std::to_string(x) + "," + std::to_string(y) + "=" + std::to_string(r) + " ";
                            grid = false;
                        }
                        const bool inLone = (x >= 30 && x < 33 && y >= 2 && y < 5) || (x == 35 && y == 1);
                        if (g != (inLone ? 64u : 0u))
                        {
                            if (lone)
                                detail += "lone " + std::to_string(x) + "," + std::to_string(y) + "=" + std::to_string(g) + " ";
                            lone = false;
                        }
                    }
                check("sprites_top_left_rule", grid && lone, detail);
                emit("sprites", *t);
                // a triangle, lines and points with integer vertices; a 1:1 nearest copy (FST u = x, sampled at x)
                auto tri = target(64, 48, "pixel centre 2");
                auto lines = gsPipeline(1, false, false, {}, {}, Topology::Lines);
                auto points = gsPipeline(1, false, false, {}, {}, Topology::Points);
                auto copy = gsPipeline(2, false, false, {}, {});
                auto src = pattern(16, 16, 31);
                auto tex = texture(16, 16, Format::RGBA8, UsageSampled, "pixel centre source", src.data());
                if (!tri || !lines || !points || !copy || !tex)
                    return;
                begin(tri.get(), nullptr, true, 0, 0, 0, 0);
                auto vert = [](float x, float y, uint32_t col) {
                    GsVertex g{};
                    g.x = x, g.y = y, g.z = 0.5f, g.q = 1, g.r = col & 0xFF, g.g = (col >> 8) & 0xFF, g.b = (col >> 16) & 0xFF, g.a = 255;
                    g.fog = 1;
                    return g;
                };
                std::vector<GsVertex> tv = {vert(2, 2, rgba(255, 0, 0, 255)), vert(14, 2, rgba(255, 0, 0, 255)), vert(2, 10, rgba(255, 0, 0, 255))};
                gsDraw(*plain, c, tv);
                std::vector<GsVertex> lv = {vert(18, 2, rgba(0, 255, 0, 255)), vert(30, 2, rgba(0, 255, 0, 255)),
                                            vert(18, 4, rgba(0, 255, 0, 255)), vert(18, 12, rgba(0, 255, 0, 255)),
                                            vert(20, 4, rgba(0, 255, 0, 255)), vert(28, 12, rgba(0, 255, 0, 255))};
                gsDraw(*lines, c, lv);
                std::vector<GsVertex> pv = {vert(34, 2, rgba(0, 0, 255, 255)), vert(36, 3, rgba(0, 0, 255, 255)), vert(38, 5, rgba(0, 0, 255, 255))};
                gsDraw(*points, c, pv);
                Draw cc = c;
                cc.tmeTfxTccFst[0] = 1, cc.tmeTfxTccFst[1] = 1, cc.tmeTfxTccFst[2] = 1, cc.tmeTfxTccFst[3] = 1;
                cc.texSizeWrap[0] = cc.texSizeWrap[1] = 16;
                cc.texSizeWrap[2] = cc.texSizeWrap[3] = 1;
                std::vector<GsVertex> cv;
                gsRect(cv, 40, 20, 56, 36, 0, 0, 16, 16, rgba(128, 128, 128, 128));
                gsDraw(*copy, cc, cv, tex.get());
                m_dev.endPass();
                px = read(*tri);
                // the triangle: pixel (x, y) with centre (x + 0.5, y + 0.5) after the +0.5 shift lies in the original
                // triangle at (x, y): x >= 2, y >= 2, (x - 2) / 12 + (y - 2) / 8 < 1 (the hypotenuse is not a tie here
                // except where it passes exactly through pixel positions; those are left to the comparison)
                bool triOk = true;
                for (uint32_t y = 0; y < 14; ++y)
                    for (uint32_t x = 0; x < 16; ++x)
                    {
                        const double e = (x - 2.0) / 12.0 + (y - 2.0) / 8.0;
                        if (std::fabs(e - 1.0) < 1e-9)
                            continue;
                        const bool in = x >= 2 && y >= 2 && e < 1.0;
                        if ((pixel(px, 64, x, y) == rgba(255, 0, 0, 255)) != in)
                            triOk = false;
                    }
                check("triangle_top_left_rule", triOk);
                bool copyOk = true;
                for (uint32_t y = 0; y < 16; ++y)
                    for (uint32_t x = 0; x < 16; ++x)
                        if (pixel(px, 64, 40 + x, 20 + y) != src[y * 16 + x])
                            copyOk = false;
                check("copy_1to1", copyOk);
                emit("triangle_lines_points_copy", *tri);
            }

            // present.hlsl and ui.hlsl variants: the presenter's letterboxed picture with Linear / Nearest samplers and
            // the alpha blend over black; the UI pipeline with UByte4Norm colours, a scissor and an index buffer
            void scenePresentUi()
            {
                covers("present VS/PS ui VS/PS VertexFormat::Float2 VertexFormat::UByte4Norm");
                auto src = pattern(16, 12, 11);
                auto tex = texture(16, 12, Format::RGBA8, UsageSampled, "picture", src.data());
                auto t = target(64, 48, "present");
                PipelineDesc pd;
                pd.vertex = m_shaders.presentVS;
                pd.pixel = m_shaders.presentPS;
                pd.topology = Topology::TriangleStrip;
                pd.blend.enable = true;
                pd.blend.srcColor = pd.blend.srcAlpha = BlendFactor::SrcAlpha;
                pd.blend.dstColor = pd.blend.dstAlpha = BlendFactor::InvSrcAlpha;
                pd.name = "present";
                auto pipe = m_dev.createPipeline(pd);
                if (!tex || !t || !pipe)
                    return;
                begin(t.get(), nullptr, true, 0, 0, 0, 1);
                presentRect(*tex, *m_nearest, 64, 48, 0, 0, 32, 24, 1, 1, pipe.get());
                presentRect(*tex, *m_linear, 64, 48, 32, 24, 64, 48, 0.75f, 0.75f, pipe.get());
                m_dev.endPass();
                emit("letterbox", *t, "unorm:2"); // bilinear: sub-texel weight precision
                // UI: two textured triangles with vertex colours, clipped by a scissor
                PipelineDesc ud;
                ud.vertex = m_shaders.uiVS;
                ud.pixel = m_shaders.uiPS;
                ud.attributes = {{"POSITION", 0, VertexFormat::Float2, offsetof(UiVertex, pos)},
                                 {"TEXCOORD", 0, VertexFormat::Float2, offsetof(UiVertex, uv)},
                                 {"COLOR", 0, VertexFormat::UByte4Norm, offsetof(UiVertex, col)}};
                ud.stride = sizeof(UiVertex);
                ud.blend.enable = true;
                ud.blend.srcColor = BlendFactor::SrcAlpha;
                ud.blend.dstColor = BlendFactor::InvSrcAlpha;
                ud.blend.srcAlpha = BlendFactor::One;
                ud.blend.dstAlpha = BlendFactor::InvSrcAlpha;
                ud.scissor = true;
                ud.name = "ui";
                auto ui = m_dev.createPipeline(ud);
                const UiVertex verts[] = {{{4, 4}, {0, 0}, rgba(255, 255, 255, 255)},
                                          {{60, 4}, {1, 0}, rgba(255, 0, 0, 200)},
                                          {{4, 44}, {0, 1}, rgba(0, 255, 0, 128)},
                                          {{60, 44}, {1, 1}, rgba(0, 0, 255, 255)}};
                const uint16_t idx[] = {0, 1, 2, 1, 3, 2};
                auto vb = m_dev.createBuffer({sizeof(verts), BufferUsage::Vertex, true, "ui vertices"}, verts);
                auto ib = m_dev.createBuffer({sizeof(idx), BufferUsage::Index, true, "ui indices"}, idx);
                auto cb = constants(16, "ui constants");
                auto ut = target(64, 48, "ui");
                if (!ui || !vb || !ib || !cb || !ut)
                    return;
                const float so[4] = {2.0f / 64, -2.0f / 48, -1.0f, 1.0f}; // ImGui's projection (top-left origin)
                m_dev.updateBuffer(*cb, so, sizeof(so));
                begin(ut.get(), nullptr, true, 26 / 255.0f, 26 / 255.0f, 26 / 255.0f, 1.0f); // k / 255: no rounding tie
                m_dev.setPipeline(*ui);
                m_dev.setConstants(0, *cb);
                m_dev.setVertexBuffer(*vb, 0);
                m_dev.setIndexBuffer(*ib, false);
                m_dev.setTexture(0, tex.get(), m_linear.get());
                m_dev.setScissor(10, 8, 40, 30);
                m_dev.drawIndexed(6, 0, 0);
                m_dev.setTexture(0, nullptr, nullptr);
                m_dev.endPass();
                emit("ui", *ut, "unorm:2"); // bilinear: sub-texel weight precision
            }

            // gs_util.hlsl: blit (alpha kept / forced to 1), fill_color, fill_depth (in passes / depth above)
            void sceneGsUtil()
            {
                covers("gs_quad VS gs_blit PS gs_fill_color PS gs_fill_depth PS");
                auto src = pattern(32, 32, 13);
                auto tex = texture(32, 32, Format::RGBA8, UsageSampled, "blit source", src.data());
                auto t = target(64, 32, "blit");
                if (!tex || !t)
                    return;
                begin(t.get(), nullptr, true, 0, 0, 0, 0);
                for (int forceAlpha = 0; forceAlpha < 2; ++forceAlpha)
                {
                    Quad c{};
                    const float x0 = forceAlpha ? 32.0f : 0.0f;
                    c.rect[0] = x0 / 64 * 2 - 1, c.rect[1] = 1, c.rect[2] = (x0 + 32) / 64 * 2 - 1, c.rect[3] = -1;
                    c.uvRect[0] = 0, c.uvRect[1] = 0, c.uvRect[2] = 1, c.uvRect[3] = 1;
                    c.params[1] = forceAlpha ? 1.0f : 0.0f;
                    m_dev.updateBuffer(*m_quadCb, &c, sizeof(c));
                    m_dev.setPipeline(*m_blit);
                    m_dev.setConstants(0, *m_quadCb);
                    m_dev.setTexture(0, tex.get(), m_nearest.get());
                    m_dev.draw(4);
                }
                m_dev.setTexture(0, nullptr, nullptr);
                m_dev.endPass();
                auto px = read(*t);
                bool ok = true;
                for (uint32_t y = 0; y < 32 && ok; ++y)
                    for (uint32_t x = 0; x < 32 && ok; ++x)
                        ok = pixel(px, 64, x, y) == src[y * 32 + x] && pixel(px, 64, x + 32, y) == (src[y * 32 + x] | 0xFF000000u);
                check("blit_1to1", ok);
                emit("blit", *t);
            }

            // every gs_draw pixel-shader variant (SAMPLE_MODE 0..6 x dual x shader depth) drawing a textured quad the
            // way gs_hw.cpp's drawSampleMode picks it, plus mode 0 through each of its paths
            void sceneGsModes()
            {
                covers("gs_draw VS gs_draw PS modes 0-6 (x dual x NoZ)");
                auto src = pattern(16, 16, 17);
                for (auto &p : src)
                    p = (p & 0x00FFFFFFu) | ((p >> 1) & 0x7F000000u); // PS2 alpha 0..127
                auto tex = texture(16, 16, Format::RGBA8, UsageSampled, "gs texture", src.data());
                auto repeat = m_dev.createSampler({Filter::Nearest, Wrap::Repeat, Wrap::Repeat});
                if (!tex || !repeat)
                    return;
                // mode 0's paths: linearIipFgeAte.x 0 nearest, 1 bilinear, 2 hardware, 3 gather, 4 gather + verify
                for (int mode = 0; mode < render::GsShaders::kSampleModes; ++mode)
                    for (int variant = 0; variant < 4; ++variant)
                    {
                        const bool dual = variant & 1, shaderDepth = !(variant & 2);
                        BlendState b;
                        if (dual)
                        {
                            b.enable = true;
                            b.srcColor = BlendFactor::Src1Alpha;
                            b.dstColor = BlendFactor::InvSrc1Alpha;
                        }
                        auto pipe = gsPipeline(mode, dual, shaderDepth, b, {true, true, Compare::GreaterEqual});
                        auto t = target(80, 16, "gs mode");
                        auto d = depthTarget(80, 16, "gs mode depth");
                        if (!pipe || !t || !d)
                            continue;
                        begin(t.get(), d.get(), true, 128 / 255.0f, 64 / 255.0f, 32 / 255.0f, 128 / 255.0f, true, 0.0f);
                        const int paths = mode == 0 ? 5 : 1;
                        for (int path = 0; path < paths; ++path)
                        {
                            Draw c = drawConstants(80, 16);
                            c.tmeTfxTccFst[0] = mode == 1 ? 0 : 1;
                            c.tmeTfxTccFst[1] = 0; // MODULATE
                            c.tmeTfxTccFst[2] = 1;
                            c.texSizeWrap[0] = c.texSizeWrap[1] = 16;
                            c.region[1] = c.region[3] = 15;
                            c.blendCPremul[0] = 0;
                            int linear = 0;
                            if (mode == 0)
                                linear = path;
                            else if (mode == 3)
                                linear = 1;
                            else if (mode == 4)
                                linear = 2;
                            else if (mode == 5)
                                linear = 3;
                            c.linearIipFgeAte[0] = linear;
                            if (mode == 6)
                            {
                                c.atstArefAfailRt[3] = 2; // a render target read 1:1 (texture = the target's copy)
                                c.targetRtFix[2] = 1.0f;
                                c.rtOrigin[3] = 1.0f;
                            }
                            std::vector<GsVertex> v;
                            // UV texels (FST) spanning 1.5 texture widths; pixel centres sample u = 0.85 + 1.5 x,
                            // v = 0.65 + 1.5 y: never on a texel boundary, where a last-ulp interpolation difference
                            // between implementations would pick the neighbour texel
                            c.tmeTfxTccFst[3] = 1;
                            gsRect(v, path * 16.0f, 0, path * 16.0f + 16, 16, 0.1f, -0.1f, 24.1f, 23.9f,
                                   rgba(128, 96, 160, 112), 0.5f + 0.0625f * path);
                            Sampler *s = (linear >= 2 || mode == 4 || mode == 5) ? repeat.get() : m_nearest.get();
                            gsDraw(*pipe, c, v, tex.get(), s);
                        }
                        m_dev.endPass();
                        auto px = read(*t);
                        bool noMagenta = true;
                        for (size_t i = 0; i + 3 < px.size(); i += 4)
                            if (px[i] == 255 && px[i + 1] == 0 && px[i + 2] == 255 && px[i + 3] == 255)
                                noMagenta = false;
                        const std::string part = "mode" + std::to_string(mode) + (dual ? "_dual" : "") + (shaderDepth ? "" : "_noz");
                        if (mode == 0)
                            check(part + "_gather_verify", noMagenta, "PS2X_HW_GATHER=verify path painted a mismatch");
                        // mode 4 / path 2 use the hardware sampler: nearest here, so exact
                        emit(part, *t, "unorm:1");
                        emit(part + "_depth", *d);
                    }
            }

            // gs_draw texture functions: TFX 0..3 x TCC, gouraud / flat (IIP), fog (FGE), every alpha test (ATST x AFAIL),
            // STQ perspective vs FST, the premultiply modes
            void sceneGsTfx()
            {
                covers("gs_draw TFX TCC IIP FGE ATST AFAIL FST STQ premultiply");
                auto src = pattern(8, 8, 19);
                auto tex = texture(8, 8, Format::RGBA8, UsageSampled, "tfx texture", src.data());
                auto pipe = gsPipeline(0, false, true, {}, {});
                auto t = target(64, 64, "tfx");
                if (!tex || !pipe || !t)
                    return;
                begin(t.get(), nullptr, true, 51 / 255.0f, 51 / 255.0f, 51 / 255.0f, 0.0f);
                int cell = 0;
                auto at = [&](std::vector<GsVertex> &v, uint32_t color, float fog = 1.0f) {
                    const float x = static_cast<float>((cell % 8) * 8), y = static_cast<float>((cell / 8) * 8);
                    ++cell;
                    gsRect(v, x, y, x + 8, y + 8, 0, 0, 8, 8, color, 0.5f, fog);
                };
                for (int tfx = 0; tfx < 4; ++tfx)
                    for (int tcc = 0; tcc < 2; ++tcc)
                    {
                        Draw c = drawConstants(64, 64);
                        c.tmeTfxTccFst[0] = 1, c.tmeTfxTccFst[1] = tfx, c.tmeTfxTccFst[2] = tcc, c.tmeTfxTccFst[3] = 1;
                        c.texSizeWrap[0] = c.texSizeWrap[1] = 8;
                        c.region[1] = c.region[3] = 7;
                        std::vector<GsVertex> v;
                        at(v, rgba(100, 140, 200, 90));
                        gsDraw(*pipe, c, v, tex.get());
                    }
                // gouraud vs flat: per-vertex colours
                for (int iip = 0; iip < 2; ++iip)
                {
                    Draw c = drawConstants(64, 64);
                    c.linearIipFgeAte[1] = iip;
                    std::vector<GsVertex> v;
                    at(v, rgba(255, 0, 0, 128));
                    v[1].r = 0, v[1].g = 255, v[3].r = 0, v[3].g = 255; // the right corners green
                    v[4].b = 255;
                    gsDraw(*pipe, c, v);
                }
                // fog: amount per vertex, colour per draw
                for (int k = 0; k < 4; ++k)
                {
                    Draw c = drawConstants(64, 64);
                    c.linearIipFgeAte[2] = 1;
                    c.fogColor[0] = 30, c.fogColor[1] = 200, c.fogColor[2] = 90;
                    std::vector<GsVertex> v;
                    at(v, rgba(200, 50, 50, 128), 0.25f * k + 0.1f);
                    gsDraw(*pipe, c, v);
                }
                // alpha tests: ATST 0..7 x AFAIL (pass / fail kept) against AREF 64, vertex alpha 32 / 64 / 96 per third
                for (int atst = 0; atst < 8; ++atst)
                    for (int afail = 0; afail < 2; ++afail)
                    {
                        Draw c = drawConstants(64, 64);
                        c.linearIipFgeAte[3] = 1;
                        c.atstArefAfailRt[0] = atst, c.atstArefAfailRt[1] = 64, c.atstArefAfailRt[2] = afail;
                        std::vector<GsVertex> v;
                        const float x = static_cast<float>((cell % 8) * 8), y = static_cast<float>((cell / 8) * 8);
                        ++cell;
                        for (int third = 0; third < 3; ++third)
                            gsRect(v, x, y + third * 2.0f + 1, x + 8, y + third * 2.0f + 3, 0, 0, 0, 0,
                                   rgba(255, static_cast<uint8_t>(80 * third), 0, static_cast<uint8_t>(32 + 32 * third)));
                        gsDraw(*pipe, c, v);
                    }
                // STQ perspective (FST 0) with varying Q, and the premultiply modes (C = As, FIX)
                {
                    Draw c = drawConstants(64, 64);
                    c.tmeTfxTccFst[0] = 1, c.tmeTfxTccFst[1] = 1, c.tmeTfxTccFst[2] = 1, c.tmeTfxTccFst[3] = 0;
                    c.texSizeWrap[0] = c.texSizeWrap[1] = 8;
                    c.region[1] = c.region[3] = 7;
                    std::vector<GsVertex> v;
                    at(v, rgba(128, 128, 128, 128));
                    for (size_t i = 0; i < v.size(); ++i)
                    {
                        const float q = (v[i].x > (v[0].x + 1)) ? 0.5f : 1.0f;
                        v[i].s = (v[i].x > v[0].x + 1 ? 1.0f : 0.0f) * q;
                        v[i].t = (v[i].y > v[0].y + 1 ? 1.0f : 0.0f) * q;
                        v[i].q = q;
                    }
                    gsDraw(*pipe, c, v, tex.get());
                }
                for (int premul = 0; premul < 3; ++premul)
                    for (int bc = 0; bc < 3; ++bc)
                    {
                        Draw c = drawConstants(64, 64);
                        c.blendCPremul[0] = bc, c.blendCPremul[1] = premul;
                        c.targetRtFix[3] = 96.0f;
                        std::vector<GsVertex> v;
                        at(v, rgba(120, 60, 30, 100));
                        gsDraw(*pipe, c, v);
                    }
                m_dev.endPass();
                emit("grid", *t, "unorm:1");
            }

            // fetchTexel wrapping: REPEAT (power of two and not), CLAMP, REGION_CLAMP, REGION_REPEAT, negative coordinates
            void sceneGsWrap()
            {
                covers("gs_draw wrapc REPEAT CLAMP REGION_CLAMP REGION_REPEAT non-power-of-two");
                for (int npot = 0; npot < 2; ++npot)
                {
                    const uint32_t tw = npot ? 12 : 16, th = npot ? 10 : 16;
                    auto src = pattern(tw, th, 23 + npot);
                    auto tex = texture(tw, th, Format::RGBA8, UsageSampled, "wrap texture", src.data());
                    auto pipe = gsPipeline(2, false, true, {}, {});
                    auto t = target(96, 48, "wrap");
                    if (!tex || !pipe || !t)
                        return;
                    begin(t.get(), nullptr, true, 0, 0, 0, 0);
                    for (int mode = 0; mode < 4; ++mode)
                        for (int linear = 0; linear < 2; ++linear)
                        {
                            Draw c = drawConstants(96, 48);
                            c.tmeTfxTccFst[0] = 1, c.tmeTfxTccFst[1] = 1, c.tmeTfxTccFst[2] = 1, c.tmeTfxTccFst[3] = 1;
                            c.texSizeWrap[0] = static_cast<int>(tw), c.texSizeWrap[1] = static_cast<int>(th);
                            c.texSizeWrap[2] = c.texSizeWrap[3] = mode;
                            // REGION_CLAMP: MINU..MAXU; REGION_REPEAT: (c & MINU) | MAXU (mask / fix)
                            c.region[0] = mode == 3 ? 3 : 2, c.region[1] = mode == 3 ? 4 : 9;
                            c.region[2] = mode == 3 ? 7 : 1, c.region[3] = mode == 3 ? 0 : 6;
                            std::vector<GsVertex> v;
                            const float x = mode * 24.0f, y = linear * 24.0f;
                            // from -1.5 texture widths to +1.5 (negative texel coordinates included), shifted by 0.3
                            // texel so no pixel centre samples on a texel boundary
                            gsRect(v, x, y, x + 24, y + 24, -1.5f * tw + 0.3f, -1.5f * th + 0.3f, 1.5f * tw + 0.3f, 1.5f * th + 0.3f,
                                   rgba(128, 128, 128, 128));
                            auto pl = linear ? gsPipeline(3, false, true, {}, {}) : nullptr;
                            gsDraw(linear ? *pl : *pipe, c, v, tex.get());
                        }
                    m_dev.endPass();
                    emit(npot ? "npot" : "pot", *t, "unorm:1");
                }
            }

            // render-target reads: per PS2 texel from a scaled target (Rt 1), 1:1 at full resolution (Rt 2, mode 6 and 0),
            // the filtered reflection read (Rt 3, gl_FragCoord / SV_Position)
            void sceneGsRtReads()
            {
                covers("gs_draw texIsRt 1/2/3 rtScale rtOrigin filterSize filterDest gl_FragCoord");
                // a 2x scaled "render target" holding a 16x16 PS2 picture as 32x32 texels
                auto src = pattern(32, 32, 29);
                auto rt = texture(32, 32, Format::RGBA8, UsageSampled | UsageRenderTarget, "scaled rt", src.data());
                if (!rt)
                    return;
                const struct
                {
                    const char *name;
                    int mode, rtKind, linear;
                } cases[] = {
                    {"rt_texel", 2, 1, 0},     {"rt_full_nearest", 6, 2, 0}, {"rt_full_linear", 6, 2, 1},
                    {"rt_full_mode0", 0, 2, 1}, {"rt_filtered", 0, 3, 1},
                };
                for (const auto &k : cases)
                {
                    auto pipe = gsPipeline(k.mode, false, true, {}, {});
                    auto t = target(32, 32, "rt read");
                    if (!pipe || !t)
                        continue;
                    begin(t.get(), nullptr, true, 0, 0, 0, 0);
                    Draw c = drawConstants(16, 16); // PS2 pixels; the target is 2x
                    c.targetRtFix[2] = 2.0f;
                    c.rtOrigin[0] = 1.0f, c.rtOrigin[1] = 2.0f, c.rtOrigin[3] = 2.0f;
                    c.tmeTfxTccFst[0] = 1, c.tmeTfxTccFst[1] = 1, c.tmeTfxTccFst[2] = 1, c.tmeTfxTccFst[3] = 1;
                    c.texSizeWrap[0] = c.texSizeWrap[1] = 16;
                    c.texSizeWrap[2] = c.texSizeWrap[3] = 1;
                    c.atstArefAfailRt[3] = k.rtKind;
                    c.linearIipFgeAte[0] = k.linear;
                    c.filterSize[0] = 2.5f, c.filterSize[1] = 1.75f, c.filterSize[2] = 15.0f, c.filterSize[3] = 13.0f;
                    c.filterDest[0] = 2.0f, c.filterDest[1] = 2.0f;
                    std::vector<GsVertex> v;
                    gsRect(v, 0, 0, 16, 16, 0.25f, 0.5f, 14.75f, 13.5f, rgba(128, 128, 128, 128));
                    gsDraw(*pipe, c, v, rt.get());
                    m_dev.endPass();
                    emit(k.name, *t, "unorm:1");
                }
            }

            // the native world / lit vertex shaders (UInt1 colours, clip distances, depth clamp, fog), constant slot 1
            void sceneGsNative()
            {
                covers("gs_world VS gs_lit VS VertexFormat::UInt1 VertexFormat::Float3 setConstants(slot 1) depthClip=false clip distances");
                // an identity-like GS window matrix: S = (x, y, z, 1) in PS2 pixels; Z format scale 1/16
                float nat[32][4] = {};
                auto row = [&](int r, float a, float b, float c, float d) { nat[r][0] = a, nat[r][1] = b, nat[r][2] = c, nat[r][3] = d; };
                row(0, 1, 0, 0, 0), row(1, 0, 1, 0, 0), row(2, 0, 0, 1, 0), row(3, 0, 0, 0, 1); // S rows
                row(4, 1.0f / 255, 0, 0, 0), row(5, 0, 1.0f / 255, 0, 0), row(6, 0, 0, 1.0f / 255, 0);
                row(31, 0, 0, 1.0f / 16, 0);
                // world: prelit colour scale / offset / clamp, fog on, clip rows: C.z = z - 8, C.w = 8 (near / far at 0 / 16)
                row(7, 1, 1, 1, 1), row(8, 0, 0, 0, 0), row(9, 255, 255, 255, 255), row(10, 1, 64, -8, 0);
                row(11, 0, 0, 0, 0), row(12, 0, 0, 0, 0), row(13, 0, 0, 1, 0), row(14, 0, 0, -8, 8);
                PipelineDesc wd;
                wd.vertex = m_shaders.worldVS;
                wd.pixel = m_shaders.drawPS[1][0];
                wd.attributes = {{"POSITION", 0, VertexFormat::Float3, 0}, {"TEXCOORD", 0, VertexFormat::Float3, 12}, {"COLOR", 0, VertexFormat::UInt1, 24}};
                wd.stride = sizeof(WorldVertex);
                wd.depth = {true, true, Compare::GreaterEqual};
                wd.depthClip = false;
                wd.name = "native world";
                auto world = m_dev.createPipeline(wd);
                auto t = target(64, 32, "native");
                auto d = depthTarget(64, 32, "native depth");
                if (!world || !t || !d)
                    return;
                // triangles whose z crosses the clip planes (z < 0 / z > 16 clipped by distance), depth clamped otherwise
                std::vector<WorldVertex> wv;
                auto w = [&](float x, float y, float z, uint32_t col) { wv.push_back({{x, y, z}, {x, y, 0}, col}); };
                w(0, 0, -8, rgba(255, 0, 0, 128)), w(32, 0, 8, rgba(0, 255, 0, 128)), w(0, 32, 24, rgba(0, 0, 255, 128));
                w(32, 0, 4, rgba(250, 250, 10, 128)), w(32, 32, 12, rgba(10, 250, 250, 128)), w(0, 32, 15, rgba(250, 10, 250, 128));
                auto vb = m_dev.createBuffer({wv.size() * sizeof(WorldVertex), BufferUsage::Vertex, false, "world vertices"}, wv.data());
                begin(t.get(), d.get(), true, 0, 0, 0, 0, true, 0.0f);
                Draw c = drawConstants(64, 32);
                m_dev.updateBuffer(*m_drawCb, &c, sizeof(c));
                m_dev.updateBuffer(*m_nativeCb, nat, sizeof(nat));
                m_dev.setPipeline(*world);
                m_dev.setConstants(0, *m_drawCb);
                m_dev.setConstants(1, *m_nativeCb);
                m_dev.setVertexBuffer(*vb, 0);
                m_dev.draw(6);
                // lit: directional light rows, colours, ambient, clamp, material alpha; normals per vertex
                float lit[32][4] = {};
                std::memcpy(lit, nat, sizeof(lit));
                auto lrow = [&](int r, float a, float b, float cc, float dd) { lit[r][0] = a, lit[r][1] = b, lit[r][2] = cc, lit[r][3] = dd; };
                lrow(7, 0, 0, 0, 0); // fog off
                lrow(8, 0, 0, 0, 0), lrow(9, 0, 0, 0, 0), lrow(10, 0, 0, 1, 0), lrow(11, 0, 0, -8, 8);
                lrow(12, 1, 0, 0, 0), lrow(13, 0, 1, 0, 0), lrow(14, 0, 0, 1, 0); // light directions (lanes)
                lrow(15, 200, 0, 0, 0), lrow(16, 0, 200, 0, 0), lrow(17, 0, 0, 200, 0), lrow(18, 0, 0, 0, 0);
                lrow(19, 30, 30, 30, 0), lrow(20, 255, 255, 255, 255), lrow(21, 100, 0, 0, 0);
                PipelineDesc ld = wd;
                ld.vertex = m_shaders.litVS;
                ld.pixel = m_shaders.drawPSNoDepth[1][0];
                ld.attributes = {{"POSITION", 0, VertexFormat::Float3, 0}, {"TEXCOORD", 0, VertexFormat::Float3, 12}, {"NORMAL", 0, VertexFormat::Float3, 24}};
                ld.stride = sizeof(LitVertex);
                ld.name = "native lit";
                auto litPipe = m_dev.createPipeline(ld);
                std::vector<LitVertex> lv = {{{36, 2, 6}, {0, 0, 0}, {1, 0, 0}},   {{62, 2, 6}, {0, 0, 0}, {0, 1, 0}},
                                             {{36, 30, 6}, {0, 0, 0}, {0, 0, 1}},  {{62, 2, 6}, {0, 0, 0}, {0, 1, 0}},
                                             {{62, 30, 6}, {0, 0, 0}, {0.6f, 0.8f, 0}}, {{36, 30, 6}, {0, 0, 0}, {0, 0, 1}}};
                auto lvb = m_dev.createBuffer({lv.size() * sizeof(LitVertex), BufferUsage::Vertex, false, "lit vertices"}, lv.data());
                if (litPipe && lvb)
                {
                    m_dev.updateBuffer(*m_nativeCb, lit, sizeof(lit));
                    m_dev.setPipeline(*litPipe);
                    m_dev.setConstants(1, *m_nativeCb);
                    m_dev.setVertexBuffer(*lvb, 0);
                    m_dev.draw(6);
                }
                m_dev.endPass();
                emit("color", *t, "unorm:1");
                emit("depth", *d, "float:1e-6");
            }

            // GL-only: the backend's refusals (bounds, alignment, hazards, owner thread) count and leave no GL error
            void sceneGlChecks()
            {
#if defined(VCS_HAVE_OPENGL)
                if (m_dev.backend() != Backend::OpenGL)
                    return;
                covers("GL bounds / hazard / owner-thread refusals");
                GlStats before{};
                openGLStats(m_dev, before);
                auto t = texture(8, 8, Format::RGBA8, UsageSampled | UsageRenderTarget, "bounds");
                std::vector<uint32_t> px(64, 0xFFFFFFFFu);
                if (!t)
                    return;
                m_dev.updateTexture(*t, 4, 4, 8, 8, px.data()); // past the edge
                std::vector<uint8_t> out(4 * 64 * 4);
                const bool readPast = m_dev.readTexture(*t, 0, 0, 9, 1, out.data()); // past the edge
                auto small = m_dev.createBuffer({16, BufferUsage::Vertex, true, "small"});
                std::vector<uint8_t> big(64, 1);
                if (small)
                    m_dev.updateBuffer(*small, big.data(), big.size()); // larger than the buffer: clamped
                // a draw sampling its own target: refused (D3D11 would read zeros through an unbound view)
                begin(t.get());
                m_dev.setPipeline(*m_blit);
                m_dev.setConstants(0, *m_quadCb);
                m_dev.setTexture(0, t.get(), m_nearest.get());
                m_dev.draw(4);
                m_dev.setTexture(0, nullptr, nullptr);
                m_dev.endPass();
                // a draw past the end of its vertex buffer: refused
                auto pipe = gsPipeline(1, false, false, {}, {});
                begin(t.get());
                if (pipe && small)
                {
                    m_dev.updateBuffer(*m_drawCb, &before, 0);
                    m_dev.setPipeline(*pipe);
                    m_dev.setConstants(0, *m_drawCb);
                    m_dev.setVertexBuffer(*small, 0);
                    m_dev.draw(3);
                }
                m_dev.endPass();
                GlStats after{};
                openGLStats(m_dev, after);
                check("refusals_counted", after.rejected >= before.rejected + 5 && !readPast,
                      std::to_string(after.rejected - before.rejected) + " refusals");
                check("hazard_counted", after.hazards == before.hazards + 1);
                check("no_gl_error_from_refusals", openGLCheckErrors(m_dev, "gl_checks") == 0);
#endif
            }

            // freeThreadedCreation: textures made on another thread while the owner draws, then sampled by the owner
            // (the GL backend's loader context and fences when PS2X_GL_WORKER_CONTEXT=1; D3D11 / Vulkan always)
            void sceneFreeThreaded()
            {
                covers("freeThreadedCreation createTexture/createTextureLevels off the owner thread");
                log(std::string("freeThreadedCreation ") + (m_dev.freeThreadedCreation() ? "1" : "0"));
                if (!m_dev.freeThreadedCreation())
                    return;
                constexpr int kCount = 24;
                std::vector<std::vector<uint32_t>> data(kCount), mip1(kCount);
                for (int i = 0; i < kCount; ++i)
                {
                    data[i] = pattern(32, 32, 100 + i);
                    mip1[i] = pattern(16, 16, 200 + i);
                }
                std::vector<std::unique_ptr<Texture>> made(kCount);
                std::thread worker([&] {
                    for (int i = 0; i < kCount; ++i)
                    {
                        TextureDesc d;
                        d.width = d.height = 32;
                        d.name = "worker texture";
                        if (i & 1)
                        {
                            const void *levels[2] = {data[i].data(), mip1[i].data()};
                            made[i] = m_dev.createTextureLevels(d, levels, 2);
                        }
                        else
                            made[i] = m_dev.createTexture(d, data[i].data());
                    }
                });
                // the owner keeps drawing meanwhile
                auto busy = target(64, 64, "owner busy");
                for (int k = 0; k < 20 && busy; ++k)
                {
                    begin(busy.get(), nullptr, true, k / 20.0f, 0, 0, 1);
                    const float c[4] = {0, 1, 0, 1};
                    fill(64, 64, 0, 0, 32, 32, c);
                    m_dev.endPass();
                }
                worker.join();
                bool ok = true;
                std::string detail;
                for (int i = 0; i < kCount; ++i)
                {
                    if (!made[i])
                    {
                        ok = false;
                        detail += "null " + std::to_string(i) + " ";
                        continue;
                    }
                    auto t = target(32, 32, "worker copy");
                    begin(t.get());
                    presentRect(*made[i], *m_nearest, 32, 32, 0, 0, 32, 32, 1, 1);
                    m_dev.endPass();
                    auto px = read(*t);
                    if (std::memcmp(px.data(), data[i].data(), px.size()) != 0)
                    {
                        ok = false;
                        detail += "content " + std::to_string(i) + " ";
                    }
                }
                check("sampled_by_owner", ok, detail);
                made.clear();
            }

            // resource churn: textures (all kinds), buffers, samplers, pipelines made and released repeatedly; GL
            // objects and process memory return to their level (M05-14)
            void sceneChurn()
            {
                covers("resource churn, deferred destruction, offscreen resize");
                const uint64_t mem0 = m_sink.processMemory ? m_sink.processMemory() : 0;
#if defined(VCS_HAVE_OPENGL)
                GlStats s0{};
                const bool gl = openGLStats(m_dev, s0);
#endif
                uint64_t peak = mem0;
                for (uint32_t round = 0; round < m_options.churnRounds; ++round)
                {
                    std::vector<std::unique_ptr<Texture>> textures;
                    std::vector<std::unique_ptr<Buffer>> buffers;
                    const uint32_t size = 64u + (round % 7) * 32u;
                    auto data = pattern(size, size, round);
                    textures.push_back(texture(size, size, Format::RGBA8, UsageSampled, "churn", data.data()));
                    textures.push_back(texture(size, size, Format::RGBA8, UsageSampled | UsageRenderTarget, "churn rt"));
                    textures.push_back(depthTarget(size, size, "churn depth"));
                    textures.push_back(texture(size, size, Format::RGBA8, UsageSampled | UsageDynamic, "churn dynamic"));
                    textures.push_back(texture(size, size, Format::RGBA8, UsageSampled, "churn mips", data.data(), 0, 1, 0));
                    if (round % 10 == 0)
                        textures.push_back(texture(size, size, Format::RGBA8, UsageRenderTarget, "churn msaa", nullptr, 0, 4));
                    buffers.push_back(m_dev.createBuffer({size * 64u, BufferUsage::Vertex, true, "churn vb"}, data.data()));
                    buffers.push_back(m_dev.createBuffer({size * 4u, BufferUsage::Index, false, "churn ib"}, data.data()));
                    buffers.push_back(constants(176, "churn cb"));
                    auto sampler = m_dev.createSampler({Filter::Linear, Wrap::Repeat, Wrap::Clamp, round % 2 ? 4u : 0u});
                    auto pipe = gsPipeline(static_cast<int>(round % 7), (round & 1) != 0, (round & 2) != 0, {}, {});
                    // use them: a draw into the target with the texture, so framebuffers and bindings are made
                    if (textures[1] && textures[2] && textures[0] && pipe)
                    {
                        begin(textures[1].get(), textures[2].get(), true, 0, 0, 0, 0, true, 0.0f);
                        Draw c = drawConstants(size, size);
                        c.tmeTfxTccFst[0] = 1;
                        c.texSizeWrap[0] = c.texSizeWrap[1] = static_cast<int>(size);
                        std::vector<GsVertex> v;
                        gsRect(v, 0, 0, static_cast<float>(size), static_cast<float>(size), 0, 0, 1, 1, rgba(128, 128, 128, 128));
                        BlendState none;
                        (void)none;
                        gsDraw(*pipe, c, v, textures[0].get(), sampler.get());
                        m_dev.endPass();
                    }
                    textures.clear();
                    buffers.clear();
                    pipe.reset();
                    sampler.reset();
                    if (round % 8 == 0)
                        m_dev.resize(320 + (round % 5) * 64, 240 + (round % 3) * 32);
                    m_dev.present(Device::PresentMode::Immediate);
                    if (m_sink.processMemory)
                        peak = std::max(peak, m_sink.processMemory());
                }
                m_dev.resize(640, 448);
                m_dev.present(Device::PresentMode::Immediate);
                const uint64_t mem1 = m_sink.processMemory ? m_sink.processMemory() : 0;
                log("churn: process memory " + std::to_string(mem0) + " -> " + std::to_string(mem1) + " bytes, peak " + std::to_string(peak));
#if defined(VCS_HAVE_OPENGL)
                if (gl)
                {
                    GlStats s1{};
                    openGLStats(m_dev, s1);
                    check("objects_return", s1.textures == s0.textures && s1.buffers == s0.buffers && s1.samplers == s0.samplers &&
                                                s1.pipelines == s0.pipelines && s1.pendingDeletes == 0,
                          "textures " + std::to_string(s0.textures) + "->" + std::to_string(s1.textures) + ", buffers " +
                              std::to_string(s0.buffers) + "->" + std::to_string(s1.buffers) + ", pending " + std::to_string(s1.pendingDeletes));
                    check("bytes_return", s1.textureBytes == s0.textureBytes && s1.bufferBytes == s0.bufferBytes,
                          std::to_string(s0.textureBytes) + "->" + std::to_string(s1.textureBytes));
                    // framebuffers of released textures are released with them; programs are cached by shader (bounded
                    // by the 39 variants)
                    check("framebuffers_bounded", s1.framebuffers <= s0.framebuffers + 2,
                          std::to_string(s0.framebuffers) + "->" + std::to_string(s1.framebuffers));
                    check("programs_bounded", s1.programs <= 64, std::to_string(s1.programs));
                }
#endif
                if (mem0 && mem1 && m_dev.backend() != Backend::Vulkan) // the reference's own driver caches are not ours
                {
                    // the budget: after churn the process may hold at most 8 MiB more than before (allocator / driver
                    // caches); unbounded growth would be ~rounds x the per-round allocation (several MiB per round)
                    const int64_t growth = static_cast<int64_t>(mem1) - static_cast<int64_t>(mem0);
                    check("memory_budget", growth <= 8ll * 1024 * 1024, std::to_string(growth) + " bytes growth");
                }
            }

            Device &m_dev;
            Sink &m_sink;
            Options m_options;
            const render::GsShaders &m_shaders;
            std::string m_scene;
            int m_failures = 0;
            std::unique_ptr<Pipeline> m_fill, m_blit, m_present;
            std::unique_ptr<Buffer> m_quadCb, m_drawCb, m_presentCb, m_nativeCb, m_gsVb;
            std::unique_ptr<Sampler> m_nearest, m_linear;
        };
    }

    int run(rhi::Device &device, Sink &sink, const Options &options)
    {
        Runner r(device, sink, options);
        return r.run();
    }
}
