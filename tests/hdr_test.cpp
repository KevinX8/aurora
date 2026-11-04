#include <gtest/gtest.h>

#include "webgpu/hdr.hpp"
#include "gx/gx.hpp"
#include "gx/pipeline.hpp"

#include <array>
#include <cmath>
#include <string>

using namespace aurora::webgpu::hdr;

// TEV keeps hardware clamps in HDR (subtract stages, comparisons and x2/x4 scales depend on
// them), so overbright only comes from the post-TEV lighting gain.
TEST(HdrShader, TevClampsStayHardware) {
  const std::string color = aurora::gx::tev_color_op(GX_TEV_ADD, "", "", true, "a", "b", "c", "d");
  EXPECT_NE(color.find(", vec3f(0.0), vec3f(1.0))"), std::string::npos);
  const std::string alpha = aurora::gx::tev_alpha_op(GX_TEV_ADD, "", "", true, "a", "b", "c", "d");
  EXPECT_NE(alpha.find(", 0.0, 1.0)"), std::string::npos);
}

TEST(HdrShader, LightingUnclipKeepsClampAndEmitsGain) {
  aurora::gx::ColorChannelConfig cc{};
  cc.lightingEnabled = true;
  aurora::gx::ShaderConfig config{};
  const std::string off = aurora::gx::lighting_func(config, cc, 0, false);
  ASSERT_NE(off.find("clamp(lighting, vec4f(0.0), vec4f(1.0))"), std::string::npos);
  EXPECT_EQ(off.find("gain0"), std::string::npos);
  config.unclipLighting = true;
  const std::string on = aurora::gx::lighting_func(config, cc, 0, false);
  EXPECT_NE(on.find("clamp(lighting, vec4f(0.0), vec4f(1.0))"), std::string::npos);
  EXPECT_NE(on.find("out.gain0 = max(lighting.rgb, vec3f(1.0));"), std::string::npos);
  // Alpha lighting never produces a gain.
  EXPECT_EQ(aurora::gx::lighting_func(config, cc, 0, true).find("gain"), std::string::npos);
}

TEST(HdrValidate, AcceptsValid) {
  EXPECT_TRUE(validate_hdr_frame(sizeof(AuroraHdrFrame), sizeof(AuroraHdrFrame), 200.f, 1000.f));
}

TEST(HdrValidate, RejectsBadValues) {
  const auto expected = sizeof(AuroraHdrFrame);
  EXPECT_FALSE(validate_hdr_frame(expected, expected, NAN, 1000.f));
  EXPECT_FALSE(validate_hdr_frame(expected, expected, 0.f, 1000.f));
  EXPECT_FALSE(validate_hdr_frame(expected, expected, -5.f, 1000.f));
  EXPECT_FALSE(validate_hdr_frame(expected, expected, 200.f, 10001.f));
  EXPECT_FALSE(validate_hdr_frame(expected, expected, 1000.f, 200.f));
}

TEST(HdrValidate, RejectsShortStruct) { EXPECT_FALSE(validate_hdr_frame(4u, sizeof(AuroraHdrFrame), 200.f, 1000.f)); }

TEST(HdrShader, BuildSourceUnclipped) {
  // build_shader_source requires a mapped position attribute (pre-existing
  // constraint), so map the minimum to exercise the generated source.
  auto config = aurora::gx::ShaderConfig{};
  config.attrs[GX_VA_POS].attrType = GX_DIRECT;
  config.attrs[GX_VA_POS].cnt = 3;
  config.attrs[GX_VA_POS].compType = GX_F32;
  config.attrs[GX_VA_POS].stride = 12;
  config.tevStageCount = 1;
  auto& stage = config.tevStages[0];
  stage.colorOp.clamp = true;
  stage.colorPass.a = GX_CC_RASC;
  stage.channelId = GX_COLOR0;
  config.colorChannels[0].lightingEnabled = true;
  // Side buffers at locations 1/2 (no normal target).
  const std::string off = aurora::gx::build_shader_source(config, aurora::gx::DstAlphaMode::None, UINT32_MAX, 1, 2);
  EXPECT_NE(off.find("@location(1) hdr_gain: vec4f"), std::string::npos);
  EXPECT_NE(off.find("out.hdr_gain = vec4f(vec3f(1.0), hdr_src.a);"), std::string::npos);
  EXPECT_NE(off.find("out.hdr_glow = vec4f(vec3f(0.0), hdr_src.a);"), std::string::npos); // lit: no glow
  config.unclipLighting = true;
  const std::string on = aurora::gx::build_shader_source(config, aurora::gx::DstAlphaMode::None, UINT32_MAX, 1, 2);
  EXPECT_NE(on.find("gain0: vec3f"), std::string::npos);
  EXPECT_NE(on.find("out.hdr_gain = vec4f(in.gain0, hdr_src.a);"), std::string::npos);
  config.colorChannels[0].lightingEnabled = false;
  const std::string unlit = aurora::gx::build_shader_source(config, aurora::gx::DstAlphaMode::None, UINT32_MAX, 1, 2);
  EXPECT_NE(unlit.find("out.hdr_glow = vec4f(hdr_src.rgb, hdr_src.a);"), std::string::npos);
  // Without side buffers the output is unchanged (single color return).
  EXPECT_EQ(aurora::gx::build_shader_source(config, aurora::gx::DstAlphaMode::None).find("hdr_"), std::string::npos);
}

TEST(HdrMode, KeepsWhenOffered) {
  std::array f{wgpu::TextureFormat::BGRA8Unorm, wgpu::TextureFormat::RGB10A2Unorm};
  EXPECT_EQ(resolve_mode(AURORA_HDR_HDR10, f), AURORA_HDR_HDR10);
}

TEST(HdrMode, FallsBackWhenMissing) {
  std::array f{wgpu::TextureFormat::BGRA8Unorm};
  EXPECT_EQ(resolve_mode(AURORA_HDR_SCRGB, f), AURORA_HDR_OFF);
}

TEST(HdrMode, OffStaysOff) {
  std::array f{wgpu::TextureFormat::RGBA16Float};
  EXPECT_EQ(resolve_mode(AURORA_HDR_OFF, f), AURORA_HDR_OFF);
}

TEST(HdrMode, ColorManagement) {
  EXPECT_FALSE(color_management(AURORA_HDR_OFF).has_value());
  const auto scrgb = color_management(AURORA_HDR_SCRGB);
  EXPECT_EQ(scrgb->colorSpace, wgpu::PredefinedColorSpace::SRGBLinear);
  EXPECT_EQ(scrgb->toneMappingMode, wgpu::ToneMappingMode::Extended);
  const auto hdr10 = color_management(AURORA_HDR_HDR10);
  EXPECT_EQ(hdr10->colorSpace, wgpu::PredefinedColorSpace::Rec2100PQ);
  EXPECT_EQ(hdr10->toneMappingMode, wgpu::ToneMappingMode::Standard);
}

TEST(HdrMode, Backends) {
  EXPECT_TRUE(hdr_backend_supported(wgpu::BackendType::Vulkan));
  EXPECT_TRUE(hdr_backend_supported(wgpu::BackendType::D3D12));
  EXPECT_TRUE(hdr_backend_supported(wgpu::BackendType::Metal));
  EXPECT_FALSE(hdr_backend_supported(wgpu::BackendType::D3D11));
  EXPECT_FALSE(hdr_backend_supported(wgpu::BackendType::OpenGL));
  EXPECT_FALSE(hdr_backend_supported(wgpu::BackendType::OpenGLES));
  EXPECT_FALSE(hdr_backend_supported(wgpu::BackendType::Null));
}

TEST(HdrPq, Endpoints) {
  EXPECT_NEAR(pq_encode(0.f), 0.f, 1e-6);
  EXPECT_NEAR(pq_encode(10000.f), 1.f, 1e-5);
  EXPECT_NEAR(pq_encode(100.f), 0.508078f, 1e-4);
}

TEST(HdrFrame, ResetsEachFrame) {
  FrameState s;
  s.pending.sceneLinear = true;
  s.pending.unclipLighting = true;
  int texture = 0;
  s.pending.sceneTexture = &texture;
  s.begin_frame();
  EXPECT_FALSE(s.pending.sceneLinear);
  // A mod that stops grading must not leave a stale texture presented.
  EXPECT_EQ(s.pending.sceneTexture, nullptr);
  EXPECT_TRUE(s.unclip.unclipLighting);
  s.begin_frame();
  EXPECT_FALSE(s.unclip.unclipLighting);
}

TEST(HdrFrame, PendingDefaultsToConfigNits) {
  FrameState s{250.f, 800.f};
  s.begin_frame();
  EXPECT_FLOAT_EQ(s.pending.paperWhiteNits, 250.f);
  EXPECT_FLOAT_EQ(s.pending.peakNits, 800.f);
}

// HDR side buffers: blend factors per draw class (gain: lighting gain, glow: unsaturated additive color).
TEST(HdrSideBlend, PerDrawClass) {
  aurora::gx::PipelineConfig config{};
  config.blendMode = GX_BM_NONE;
  auto b = aurora::gx::hdr_side_blends(config);
  EXPECT_TRUE(b.gain.write && b.gain.replace);
  EXPECT_TRUE(b.glow.write);
  EXPECT_EQ(b.glow.state.color.srcFactor, wgpu::BlendFactor::Zero); // opaque clears glow behind it
  EXPECT_EQ(b.glow.state.color.dstFactor, wgpu::BlendFactor::Zero);

  config.blendMode = GX_BM_BLEND;
  config.blendFacSrc = GX_BL_SRCALPHA;
  config.blendFacDst = GX_BL_INVSRCALPHA;
  b = aurora::gx::hdr_side_blends(config);
  EXPECT_EQ(b.gain.state.color.srcFactor, wgpu::BlendFactor::SrcAlpha);
  EXPECT_EQ(b.gain.state.color.dstFactor, wgpu::BlendFactor::OneMinusSrcAlpha);
  EXPECT_EQ(b.glow.state.color.srcFactor, wgpu::BlendFactor::Zero); // translucent fades glow, adds none
  EXPECT_EQ(b.glow.state.color.dstFactor, wgpu::BlendFactor::OneMinusSrcAlpha);

  config.blendFacDst = GX_BL_ONE; // additive (SrcAlpha, One)
  b = aurora::gx::hdr_side_blends(config);
  EXPECT_FALSE(b.gain.write);
  EXPECT_TRUE(b.glow.write);
  EXPECT_EQ(b.glow.state.color.srcFactor, wgpu::BlendFactor::SrcAlpha);
  EXPECT_EQ(b.glow.state.color.dstFactor, wgpu::BlendFactor::One);

  config.blendMode = GX_BM_SUBTRACT;
  b = aurora::gx::hdr_side_blends(config);
  EXPECT_FALSE(b.gain.write || b.glow.write);
}
