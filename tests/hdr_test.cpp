#include <gtest/gtest.h>

#include "webgpu/hdr.hpp"

#include <array>
#include <cmath>

using namespace aurora::webgpu::hdr;

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
  s.pending.unclipTev = true;
  s.begin_frame();
  EXPECT_FALSE(s.pending.sceneLinear);
  EXPECT_TRUE(s.unclip.unclipTev);
  s.begin_frame();
  EXPECT_FALSE(s.unclip.unclipTev);
}

TEST(HdrFrame, PendingDefaultsToConfigNits) {
  FrameState s{250.f, 800.f};
  s.begin_frame();
  EXPECT_FLOAT_EQ(s.pending.paperWhiteNits, 250.f);
  EXPECT_FLOAT_EQ(s.pending.peakNits, 800.f);
}
