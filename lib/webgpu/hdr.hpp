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

inline AuroraHdrMode resolve_mode(AuroraHdrMode requested, std::span<const wgpu::TextureFormat> offered,
                                  wgpu::BackendType backend) {
  if (requested == AURORA_HDR_OFF) {
    return AURORA_HDR_OFF;
  }
  // WebGPU has no HDR10 path on Metal: serve scRGB EDR instead of dropping to SDR.
  if (requested == AURORA_HDR_HDR10 && backend == wgpu::BackendType::Metal) {
    return AURORA_HDR_SCRGB;
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

// magic_enum reflection over the C enum AuroraHdrMode breaks on NDK clang
// (unsigned-int underlying type trips its range probe); a 3-value switch is exact.
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
// Apple EDR interprets 1.0 as the OS SDR white (paper white) and offers `headroom` times that.
// scRGB (1.0 = 80 nits) -> EDR, clamped to the screen's current headroom.
inline float edr_output(float scrgb, float paperWhiteNits, float headroom) {
  const float scale = (paperWhiteNits > 0.f && headroom > 1.f) ? 80.f / paperWhiteNits : 1.f;
  const float cap = std::max(headroom, 1.f);
  return std::min(scrgb * scale, cap);
}

inline float pq_encode(float nits) {
  const double y = std::pow(std::max<double>(nits, 0.0) / 10000.0, PQ_M1);
  return static_cast<float>(std::pow((PQ_C1 + PQ_C2 * y) / (1.0 + PQ_C3 * y), PQ_M2));
}

inline float pq_decode(float pq) {
  const double ep = std::pow(std::max<double>(pq, 0.0), 1.0 / PQ_M2);
  const double num = std::max(ep - PQ_C1, 0.0);
  const double den = PQ_C2 - PQ_C3 * ep;
  return static_cast<float>(std::pow(num / den, 1.0 / PQ_M1) * 10000.0);
}

inline float fallback_nits(float gammaValue, float paperWhiteNits) {
  return std::copysign(std::pow(std::abs(gammaValue), 2.2f), gammaValue) * paperWhiteNits;
}

inline float sanitize(float value) { return std::isfinite(value) ? value : 0.f; }

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
// macOS. shortcut: main screen, single window; upgrade for multi-monitor windows.
#if defined(__APPLE__)
float aurora_edr_headroom();
#else
inline float aurora_edr_headroom() { return 1.f; }
#endif
} // namespace aurora::webgpu
