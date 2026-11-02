#include <gtest/gtest.h>

#include "webgpu/hdr.hpp"

#include <array>

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
