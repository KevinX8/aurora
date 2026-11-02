#pragma once

// Pure HDR helpers shared by the engine internals and mods (dusklight SDK include path).

#include <cstddef>
#include <cstdint>
#include <cmath>

namespace aurora::webgpu::hdr {

// Mod-side frame description guard: size covers the caller's struct, both nits
// values are finite and in [1, 10000], and peak never sits below paper white.
inline bool validate_hdr_frame(uint32_t structSize, size_t expected, float paperWhite, float peak) {
  if (structSize < expected) {
    return false;
  }
  if (!std::isfinite(paperWhite) || paperWhite < 1.f || paperWhite > 10000.f) {
    return false;
  }
  if (!std::isfinite(peak) || peak < 1.f || peak > 10000.f) {
    return false;
  }
  return peak >= paperWhite;
}

} // namespace aurora::webgpu::hdr
