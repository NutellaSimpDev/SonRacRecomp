// SonRac - the game's Direct3D busy-wait, made to sleep.
//
// sub_82CDDAD8 is the XDK Direct3D's wait for the GPU to reach a point in the
// command stream: it compares a GPU-written counter (device + 11024) with the
// value it needs and, until it gets there, calls sub_82CD3668 again and again.
// On the console that step was 32 `db16cyc` (16-cycle delays) at lowered thread
// priority, which yielded the core. Recompiled, the delays are gone and the loop
// spins flat out: the Direct3D tracer counted ~3 million calls per frame from a
// single return address (0x82CDDB70), a big core at 100 % doing nothing but heat
// while the emulated GPU catches up.
//
// The wrapper runs the original step (the game still checks its own condition
// on every pass, so behaviour is unchanged) and then sleeps briefly. Same fix as
// LibertyRecompAndroid's frame-wait hook and nfsmw-nx's "no Direct3D busy waits".
#include <chrono>
#include <thread>

#include <rex/cvar.h>

#include "generated/sonicracingtransformed_init.h"

REXCVAR_DEFINE_INT32(sonrac_d3d_wait_sleep_us, 50, "SonRac/Performance",
                     "Sleep (microseconds) in each pass of the game's Direct3D wait for the "
                     "GPU instead of spinning; 0 = spin like the original")
    .range(0, 2000);

extern "C" REX_FUNC(sub_82CD3668) {
  __imp__sub_82CD3668(ctx, base);
  const int32_t us = REXCVAR_GET(sonrac_d3d_wait_sleep_us);
  if (us > 0) {
    std::this_thread::sleep_for(std::chrono::microseconds(us));
  }
}
