// SonRac - where the game's render thread spends each frame.
//
// sub_82CD9AE8 is the XDK Direct3D Present (it calls VdSwap; once per frame,
// and it waits for the vertical blank as the title asked), sub_82CDDAD8 the
// Direct3D wait for the GPU to reach a point of the command stream. With
// sonrac_frame_probe, every 2 seconds the log gets the presents per second and,
// per frame, the time inside Present, waiting for the GPU, and the rest (the
// game's own work).
#include <chrono>
#include <mutex>
#include <unordered_set>

#include <rex/cvar.h>
#include <rex/logging.h>

#include "generated/sonicracingtransformed_init.h"

REXCVAR_DEFINE_BOOL(sonrac_frame_probe, false, "SonRac/Diagnostics",
                    "Log every 2 s the game's presents per second and how each frame splits "
                    "between Present, waiting for the GPU and the game's own work");

namespace {

using Clock = std::chrono::steady_clock;

thread_local double gpu_wait_ms = 0.0;
Clock::time_point window_start;
Clock::time_point last_present_end;
uint32_t frames = 0;
double present_ms = 0.0;
double waited_ms = 0.0;
double frame_ms = 0.0;
double frame_ms_max = 0.0;
std::mutex present_callers_mutex;
std::unordered_set<uint32_t> present_callers;
std::unordered_set<uint32_t> render_callback_callers;
std::unordered_set<uint32_t> render_schedule_callers;
std::unordered_set<uint32_t> render_dispatch_callers;
std::unordered_set<uint32_t> render_loop_callers;
std::unordered_set<uint32_t> render_worker_callers;

double Ms(Clock::time_point a, Clock::time_point b) {
  return std::chrono::duration<double, std::milli>(b - a).count();
}

}  // namespace

extern "C" REX_FUNC(sub_82783D88) {
  if (REXCVAR_GET(sonrac_frame_probe)) {
    const uint32_t caller = static_cast<uint32_t>(ctx.lr);
    std::lock_guard<std::mutex> lock(present_callers_mutex);
    if (render_worker_callers.insert(caller).second) {
      REXLOG_INFO("[RenderWorkerCaller] lr={:08X} r3={:08X} r4={:08X} r5={:08X}", caller,
                  ctx.r3.u32, ctx.r4.u32, ctx.r5.u32);
    }
  }
  __imp__sub_82783D88(ctx, base);
}

extern "C" REX_FUNC(sub_825D4730) {
  if (REXCVAR_GET(sonrac_frame_probe)) {
    const uint32_t caller = static_cast<uint32_t>(ctx.lr);
    std::lock_guard<std::mutex> lock(present_callers_mutex);
    if (render_loop_callers.insert(caller).second) {
      REXLOG_INFO("[RenderLoopCaller] lr={:08X} r3={:08X} r4={:08X} r5={:08X}", caller,
                  ctx.r3.u32, ctx.r4.u32, ctx.r5.u32);
    }
  }
  __imp__sub_825D4730(ctx, base);
}

extern "C" REX_FUNC(sub_825B62A0) {
  if (REXCVAR_GET(sonrac_frame_probe)) {
    const uint32_t caller = static_cast<uint32_t>(ctx.lr);
    std::lock_guard<std::mutex> lock(present_callers_mutex);
    if (render_dispatch_callers.insert(caller).second) {
      REXLOG_INFO("[RenderDispatchCaller] lr={:08X} r3={:08X} r4={:08X}", caller,
                  ctx.r3.u32, ctx.r4.u32);
    }
  }
  __imp__sub_825B62A0(ctx, base);
}

extern "C" REX_FUNC(sub_8278A058) {
  if (REXCVAR_GET(sonrac_frame_probe)) {
    const uint32_t caller = static_cast<uint32_t>(ctx.lr);
    std::lock_guard<std::mutex> lock(present_callers_mutex);
    if (render_schedule_callers.insert(caller).second) {
      REXLOG_INFO("[RenderScheduleCaller] lr={:08X} r3={:08X} r4={:08X} r5={:08X}", caller,
                  ctx.r3.u32, ctx.r4.u32, ctx.r5.u32);
    }
  }
  __imp__sub_8278A058(ctx, base);
}

extern "C" REX_FUNC(sub_827824A8) {
  if (REXCVAR_GET(sonrac_frame_probe)) {
    const uint32_t caller = static_cast<uint32_t>(ctx.lr);
    std::lock_guard<std::mutex> lock(present_callers_mutex);
    if (render_callback_callers.insert(caller).second) {
      REXLOG_INFO("[RenderCallbackCaller] lr={:08X} r3={:08X} r4={:08X} r5={:08X} r6={:08X}",
                  caller, ctx.r3.u32, ctx.r4.u32, ctx.r5.u32, ctx.r6.u32);
    }
  }
  __imp__sub_827824A8(ctx, base);
}

extern "C" REX_FUNC(sub_82CDDAD8) {
  if (!REXCVAR_GET(sonrac_frame_probe)) {
    __imp__sub_82CDDAD8(ctx, base);
    return;
  }
  auto start = Clock::now();
  __imp__sub_82CDDAD8(ctx, base);
  gpu_wait_ms += Ms(start, Clock::now());
}

void SonracShaderDumpPoll();

extern "C" REX_FUNC(sub_82CD9AE8) {
  SonracShaderDumpPoll();
  if (!REXCVAR_GET(sonrac_frame_probe)) {
    __imp__sub_82CD9AE8(ctx, base);
    return;
  }
  {
    const uint32_t caller = static_cast<uint32_t>(ctx.lr);
    std::lock_guard<std::mutex> lock(present_callers_mutex);
    if (present_callers.insert(caller).second) {
      const uint32_t params = ctx.r4.u32;
      REXLOG_INFO(
          "[PresentCaller] lr={:08X} arg4={:08X} arg5={:08X} params={:08X}/{:08X}/{:08X}/"
          "{:08X}/{:08X}/{:08X}/{:08X}/{:08X}/{:08X}/{:08X}/{:08X}/{:08X}/{:08X}",
          caller, params, ctx.r5.u32, REX_LOAD_U32(params + 0), REX_LOAD_U32(params + 4),
          REX_LOAD_U32(params + 8), REX_LOAD_U32(params + 12), REX_LOAD_U32(params + 16),
          REX_LOAD_U32(params + 20), REX_LOAD_U32(params + 24), REX_LOAD_U32(params + 28),
          REX_LOAD_U32(params + 32), REX_LOAD_U32(params + 36), REX_LOAD_U32(params + 40),
          REX_LOAD_U32(params + 44), REX_LOAD_U32(params + 48));
    }
  }
  auto start = Clock::now();
  __imp__sub_82CD9AE8(ctx, base);
  auto end = Clock::now();
  if (frames == 0 && window_start == Clock::time_point()) {
    window_start = start;
  } else if (last_present_end != Clock::time_point()) {
    double frame = Ms(last_present_end, end);
    frame_ms += frame;
    frame_ms_max = std::max(frame_ms_max, frame);
  }
  present_ms += Ms(start, end);
  waited_ms += gpu_wait_ms;
  gpu_wait_ms = 0.0;
  last_present_end = end;
  ++frames;
  double seconds = std::chrono::duration<double>(end - window_start).count();
  if (seconds >= 2.0) {
    double n = double(frames);
    REXLOG_INFO(
        "[FrameProbe] {:.1f} presents/s; per frame: {:.2f} ms (max {:.2f}) = Present {:.2f} + "
        "GPU waits {:.2f} + game {:.2f}",
        n / seconds, frame_ms / n, frame_ms_max, present_ms / n, waited_ms / n,
        frame_ms / n - present_ms / n - waited_ms / n);
    window_start = end;
    frames = 0;
    present_ms = waited_ms = frame_ms = frame_ms_max = 0.0;
  }
}
