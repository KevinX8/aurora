#pragma once

#include <aurora/aurora.h>

#include "wgpu.hpp"

#include <algorithm>
#include <cmath>
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

inline const char* colorspace_env(AuroraHdrMode mode) {
  switch (mode) {
  case AURORA_HDR_HDR10:
    return "hdr10";
  case AURORA_HDR_SCRGB:
    return "scrgb";
  default:
    return nullptr;
  }
}

// nits in, ST 2084 code value out
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
    pending = {false, false, false, cfgPaperWhite, cfgPeak};
  }
};

inline FrameState g_frameState;

inline const AuroraHdrFrame& active_unclip() { return g_frameState.unclip; }

} // namespace aurora::webgpu::hdr
