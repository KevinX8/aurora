#pragma once

#include <aurora/aurora.h>
#include <aurora/hdr.hpp>

#include "wgpu.hpp"

#include <algorithm>
#include <cmath>
#include <optional>
#include <span>

namespace aurora::webgpu::hdr {

inline constexpr double PQ_M1 = 0.1593017578125;
inline constexpr double PQ_M2 = 78.84375;
inline constexpr double PQ_C1 = 0.8359375;
inline constexpr double PQ_C2 = 18.8515625;
inline constexpr double PQ_C3 = 18.6875;

inline wgpu::TextureFormat required_format(AuroraHdrMode mode) {
  switch (mode) {
  case AURORA_HDR_HDR10:
    return wgpu::TextureFormat::RGB10A2Unorm;
  case AURORA_HDR_SCRGB:
    return wgpu::TextureFormat::RGBA16Float;
  default:
    return wgpu::TextureFormat::Undefined;
  }
}

inline AuroraHdrMode resolve_mode(AuroraHdrMode requested, std::span<const wgpu::TextureFormat> offered) {
  if (requested == AURORA_HDR_OFF) {
    return AURORA_HDR_OFF;
  }
  const auto required = required_format(requested);
  for (const auto format : offered) {
    if (format == required) {
      return requested;
    }
  }
  return AURORA_HDR_OFF;
}

inline std::optional<wgpu::SurfaceColorManagement> color_management(AuroraHdrMode mode) {
  switch (mode) {
  case AURORA_HDR_HDR10:
    return wgpu::SurfaceColorManagement{wgpu::SurfaceColorManagement::Init{
        .nextInChain = nullptr,
        .colorSpace = wgpu::PredefinedColorSpace::Rec2100PQ,
        .toneMappingMode = wgpu::ToneMappingMode::Standard,
    }};
  case AURORA_HDR_SCRGB:
    return wgpu::SurfaceColorManagement{wgpu::SurfaceColorManagement::Init{
        .nextInChain = nullptr,
        .colorSpace = wgpu::PredefinedColorSpace::SRGBLinear,
        .toneMappingMode = wgpu::ToneMappingMode::Extended,
    }};
  default:
    return std::nullopt;
  }
}

// Not magic_enum::enum_name: its range probe fails to compile for this C enum on Android NDK clang.
inline const char* mode_name(AuroraHdrMode mode) noexcept {
  switch (mode) {
  case AURORA_HDR_HDR10:
    return "HDR10";
  case AURORA_HDR_SCRGB:
    return "scRGB";
  default:
    return "off";
  }
}

inline bool hdr_backend_supported(wgpu::BackendType backend) noexcept {
  return backend == wgpu::BackendType::Vulkan || backend == wgpu::BackendType::D3D12 ||
         backend == wgpu::BackendType::Metal;
}

// nits in, ST 2084 code value out
// CPU reference for the shader's PQ encode, pinning the shared PQ_* constants.
inline float pq_encode(float nits) {
  const double y = std::pow(std::max<double>(nits, 0.0) / 10000.0, PQ_M1);
  return static_cast<float>(std::pow((PQ_C1 + PQ_C2 * y) / (1.0 + PQ_C3 * y), PQ_M2));
}

// pending collects this frame's aurora_set_hdr_frame (reset each begin_frame); shader generation
// reads the previous frame's value so every draw in a frame agrees on unclipLighting.
struct FrameState {
  AuroraHdrFrame pending{};
  AuroraHdrFrame unclip{};
  float cfgPaperWhite;
  float cfgPeak;

  explicit FrameState(float paperWhite = 200.f, float peak = 1000.f) : cfgPaperWhite(paperWhite), cfgPeak(peak) {}

  void begin_frame() {
    unclip = pending;
    pending = {false, false, cfgPaperWhite, cfgPeak, nullptr};
  }
};

inline FrameState g_frameState;

inline const AuroraHdrFrame& active_unclip() { return g_frameState.unclip; }

} // namespace aurora::webgpu::hdr

namespace aurora::webgpu {
// Main screen's extended-range headroom (max value / SDR white): live on iOS, static max on
// macOS. Uses the main screen, so a window on a secondary display reads the wrong headroom.
#if defined(__APPLE__)
float edr_headroom();
#else
inline float edr_headroom() { return 1.f; }
#endif
} // namespace aurora::webgpu
