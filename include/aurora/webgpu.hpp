#pragma once

#include <webgpu/webgpu_cpp.h>

#include <aurora/hdr.hpp>

struct SDL_Window;

namespace aurora::webgpu {

/// Creates an unconfigured WebGPU surface for an SDL window.
/// The SDL window must remain alive until the surface is released.
/// When hdrMode is not AURORA_HDR_OFF, chains a SurfaceColorManagement (scRGB extended tone
/// mapping, or fork-only Rec2100 PQ) so the backend presents the swapchain in that HDR space;
/// displays that cannot offer it report no matching surface formats.
wgpu::Surface create_window_surface(const wgpu::Instance& instance, SDL_Window* window,
                                    const char* label = nullptr, AuroraHdrMode hdrMode = AURORA_HDR_OFF);

/// Selects a presentation mode from the surface's capabilities matching the current vsync mode.
wgpu::PresentMode select_present_mode(const wgpu::SurfaceCapabilities& capabilities) noexcept;

/// Returns the application's current vsync mode.
bool vsync_enabled() noexcept;

} // namespace aurora::webgpu
