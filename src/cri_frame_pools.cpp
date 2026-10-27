// SonRac - more decoded-picture buffers for CRI Movie (Sofdec2) videos.
//
// Measured on Android: the per-frame movie task runs 30 times a second, the
// movie clock runs in real time, a picture decodes in ~7 ms (three threads),
// and yet only ~15 pictures a second come out (the logos play at half speed).
// The video decoder (crivmpv) has a single spare picture buffer: picture N is
// decoded asynchronously after the task returns, the next task hands it out,
// and only the task after that can start N+1, so one picture every two game
// frames. On the console the logos ran at 60 frames a second, which hid it.
//
// The pool size is config->num_frame_pools (+16) on top of a global base, read
// the same way by the work size calculation (sub_82E75F98, config in r3) and by
// the decoder creation (sub_82E78F30, config in r4). Both are bumped equally so
// the work buffer the game allocates matches the pool CRI then builds.
#include <chrono>
#include <thread>

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/system/xthread.h>

#include "generated/sonicracingtransformed_init.h"

REXCVAR_DEFINE_INT32(sonrac_cri_extra_frame_pools, 0, "SonRac/Video",
                     "Extra decoded-picture buffers for CRI movies, so the decoder can work ahead "
                     "of the game (0 = as the game asks)")
    .range(0, 8);

namespace {

constexpr uint32_t kNumFramePoolsOffset = 16;

// num_frame_pools of 0 means "the base count", n means "base + n - 1".
uint32_t Bumped(uint32_t num_frame_pools, uint32_t extra) {
  return (num_frame_pools ? num_frame_pools : 1) + extra;
}

// Runs `call` with the config's num_frame_pools bumped, then restores the
// game's value.
template <typename Call>
void WithMorePools(uint8_t* base, uint32_t config, const char* what, Call&& call) {
  const int32_t extra = REXCVAR_GET(sonrac_cri_extra_frame_pools);
  if (extra <= 0 || !config) {
    call();
    return;
  }
  const uint32_t original = REX_LOAD_U32(config + kNumFramePoolsOffset);
  const uint32_t bumped = Bumped(original, uint32_t(extra));
  REX_STORE_U32(config + kNumFramePoolsOffset, bumped);
  call();
  REX_STORE_U32(config + kNumFramePoolsOffset, original);
  static int logged = 0;
  if (logged < 8) {
    ++logged;
    REXLOG_INFO("[CriFramePools] {}: config {:08X} num_frame_pools {} -> {} ({}x{})", what, config,
                original, bumped, REX_LOAD_U32(config), REX_LOAD_U32(config + 4));
  }
}

}  // namespace

extern "C" REX_FUNC(sub_82E75F98) {
  WithMorePools(base, ctx.r3.u32, "work size", [&] { __imp__sub_82E75F98(ctx, base); });
}

extern "C" REX_FUNC(sub_82E78F30) {
  WithMorePools(base, ctx.r4.u32, "create decoder", [&] { __imp__sub_82E78F30(ctx, base); });
}

// CriAu, CRI's movie audio output thread (start 82407670 -> sub_82E641B8),
// keeps only 3 blocks of 256 samples (16 ms) in its XAudio2 voice and tops it
// up after Sleep(4). Measured on Android it handed out ~25000 samples a second
// instead of 48000: the voice ran dry between top-ups, the movie audio played
// at about half speed, and the picture (demultiplexed in step with the audio)
// with it. Poll it more often.
REXCVAR_DEFINE_INT32(sonrac_cri_audio_poll_us, 100, "SonRac/Video",
                     "Wait (microseconds) of CRI's movie audio thread between top-ups instead of "
                     "the game's Sleep(4); 0 = as the game asks")
    .range(0, 4000);

extern "C" REX_FUNC(sub_82CB78C0) {
  const int32_t poll_us = REXCVAR_GET(sonrac_cri_audio_poll_us);
  if (poll_us > 0 && ctx.r3.u32 == 4) {
    const auto* thread = rex::system::XThread::GetCurrentThread();
    if (thread && thread->creation_params()->start_address == 0x82407670) {
      // Sleep(4) waits whole milliseconds; 1 ms was still too coarse (33000
      // samples a second), so wait on the host clock instead.
      std::this_thread::sleep_for(std::chrono::microseconds(poll_us));
      ctx.r3.u64 = 0;
      return;
    }
  }
  __imp__sub_82CB78C0(ctx, base);
}
