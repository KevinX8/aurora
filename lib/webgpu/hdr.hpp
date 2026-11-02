#pragma once

#include <aurora/aurora.h>

#include "wgpu.hpp"

#include <span>

namespace aurora::webgpu::hdr {

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

} // namespace aurora::webgpu::hdr
