#include <gtest/gtest.h>

#include "webgpu/hdr.hpp"
#include "gx/gx.hpp"

#include <array>
#include <cmath>
#include <string>

using namespace aurora::webgpu::hdr;

// TEV keeps hardware clamps in HDR: Twilight Princess materials rely on them (subtract stages,
// comparisons, x2/x4 scales), so overbright only comes from the post-TEV lighting gain.
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

TEST(HdrValidate, RejectsShortStruct) {
  EXPECT_FALSE(validate_hdr_frame(4u, sizeof(AuroraHdrFrame), 200.f, 1000.f));
}

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
  const std::string off = aurora::gx::build_shader_source(config, aurora::gx::DstAlphaMode::None, 1);
  EXPECT_EQ(off.find("gain0"), std::string::npos);
  config.unclipLighting = true;
  const std::string on = aurora::gx::build_shader_source(config, aurora::gx::DstAlphaMode::None, 1);
  EXPECT_NE(on.find("gain0: vec3f"), std::string::npos);
  EXPECT_NE(on.find("prev = vec4f(prev.rgb * in.gain0, prev.a);"), std::string::npos);
  EXPECT_EQ(on.find("vec3f(4.0)"), std::string::npos);
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

TEST(HdrMode, EnvNames) {
  EXPECT_STREQ(colorspace_env(AURORA_HDR_HDR10), "hdr10");
  EXPECT_STREQ(colorspace_env(AURORA_HDR_SCRGB), "scrgb");
  EXPECT_EQ(colorspace_env(AURORA_HDR_OFF), nullptr);
}

TEST(HdrPq, Endpoints) {
  EXPECT_NEAR(pq_encode(0.f), 0.f, 1e-6);
  EXPECT_NEAR(pq_encode(10000.f), 1.f, 1e-5);
  EXPECT_NEAR(pq_encode(100.f), 0.508078f, 1e-4);
}

TEST(HdrPq, RoundTrip) {
  for (float n : {0.1f, 1.f, 80.f, 200.f, 1000.f, 4000.f, 10000.f}) {
    EXPECT_NEAR(pq_decode(pq_encode(n)), n, n * 1e-4f);
  }
}

TEST(HdrFallback, WhiteIsPaperWhite) {
  EXPECT_FLOAT_EQ(fallback_nits(1.f, 200.f), 200.f);
  EXPECT_NEAR(fallback_nits(0.5f, 200.f), 200.f * 0.217638f, 1e-2);
  EXPECT_LT(fallback_nits(-0.5f, 200.f), 0.f);
}

TEST(HdrSanitize, NanInf) {
  EXPECT_EQ(sanitize(NAN), 0.f);
  EXPECT_EQ(sanitize(INFINITY), 0.f);
  EXPECT_EQ(sanitize(2.f), 2.f);
}

TEST(HdrFrame, ResetsEachFrame) {
  FrameState s;
  s.pending.sceneLinear = true;
  s.pending.unclipLighting = true;
  s.begin_frame();
  EXPECT_FALSE(s.pending.sceneLinear);
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
