// Counts the CRI worker wakeup helpers while profiling USM movie playback.
#include <algorithm>
#include <chrono>
#include <thread>
#include <atomic>
#include <cstdint>
#include <map>
#include <mutex>
#include <set>
#include <string_view>
#include <tuple>

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/system/xthread.h>

#include "generated/sonicracingtransformed_init.h"

REXCVAR_DEFINE_BOOL(sonrac_cri_wakeup_probe, false, "SonRac/Diagnostics",
                    "Count calls to the three CRI worker wakeup helpers by guest thread");

namespace {

using Clock = std::chrono::steady_clock;
using Key = std::pair<std::string_view, uint32_t>;
std::mutex counts_mutex;
std::map<Key, uint64_t> counts;
std::set<std::pair<std::string_view, uint32_t>> seen_callsites;
std::set<std::pair<uint32_t, uint32_t>> seen_thread_callbacks;
std::set<std::tuple<uint32_t, uint32_t, uint32_t>> seen_worker_jobs;
std::set<std::pair<uint32_t, uint32_t>> seen_dispatch_targets;
std::map<std::pair<uint32_t, uint32_t>, uint64_t> dispatch_counts;
Clock::time_point window_start;
Clock::time_point dispatch_window_start;

void Record(std::string_view function, uint32_t caller) {
  const auto* thread = rex::system::XThread::GetCurrentThread();
  const uint32_t thread_id = thread ? thread->thread_id() : 0;
  const auto now = Clock::now();
  std::map<Key, uint64_t> report;
  double seconds = 0.0;
  bool new_callsite = false;
  {
    std::lock_guard lock(counts_mutex);
    new_callsite = seen_callsites.emplace(function, caller).second;
    if (window_start == Clock::time_point()) {
      window_start = now;
    }
    ++counts[{function, thread_id}];
    seconds = std::chrono::duration<double>(now - window_start).count();
    if (seconds >= 2.0) {
      report.swap(counts);
      window_start = now;
    }
  }
  if (new_callsite) {
    REXLOG_INFO("[CriCaller] {} caller={:08X} thread={:08X}", function, caller, thread_id);
  }
  for (const auto& [key, calls] : report) {
    REXLOG_INFO("[CriWake] {} thread={:08X} {:.1f} calls/s", key.first, key.second,
                calls / seconds);
  }
}

// Wall time spent inside a CRI function, logged every 2 s.
struct CriTimer {
  const char* name;
  std::mutex mutex;
  Clock::time_point window{};
  double total_ms = 0.0, max_ms = 0.0;
  uint64_t calls = 0;
  void Add(double ms) {
    const auto now = Clock::now();
    std::lock_guard lock(mutex);
    if (window == Clock::time_point()) window = now;
    total_ms += ms;
    max_ms = std::max(max_ms, ms);
    ++calls;
    const double seconds = std::chrono::duration<double>(now - window).count();
    if (seconds >= 2.0) {
      REXLOG_INFO("[CriTime] {}: {:.1f} calls/s, {:.2f} ms avg, {:.2f} ms max, {:.0f} ms/s",
                  name, calls / seconds, total_ms / double(calls), max_ms, total_ms / seconds);
      total_ms = max_ms = 0.0;
      calls = 0;
      window = now;
    }
  }
};
CriTimer cri_time_task{"task sub_82E5A928"};
// Decode jobs (sub_82E98C58) currently running on any thread.
std::atomic<int32_t> cri_jobs_in_flight{0};
std::atomic<uint32_t> cri_jobs_started{0};
CriTimer cri_time_decode{"decode sub_82E649F8"};
CriTimer cri_time_frame{"frame sub_82E72518"};
CriTimer cri_time_job[3] = {{"job sub_82E98C58 thread 0F"}, {"job sub_82E98C58 worker A"},
                            {"job sub_82E98C58 worker B"}};

}  // namespace

extern "C" REX_FUNC(sub_82E98C58) {
  if (REXCVAR_GET(sonrac_cri_wakeup_probe)) {
    const uint32_t job_data = ctx.r3.u32;
    const uint32_t job_object = REX_LOAD_U32(job_data);
    const uint32_t stream_object = REX_LOAD_U32(job_data + 4);
    bool is_new = false;
    {
      std::lock_guard lock(counts_mutex);
      is_new = seen_worker_jobs.emplace(job_data, job_object, stream_object).second;
    }
    if (is_new) {
      const auto* thread = rex::system::XThread::GetCurrentThread();
      const uint32_t thread_id = thread ? thread->thread_id() : 0;
      REXLOG_INFO("[CriWorkerJob] thread={:08X} lr={:08X} data={:08X} object={:08X} stream={:08X}",
                  thread_id, uint32_t(ctx.lr), job_data, job_object, stream_object);
    }
  }
  const auto time_begin = Clock::now();
  cri_jobs_in_flight.fetch_add(1);
  cri_jobs_started.fetch_add(1);
  __imp__sub_82E98C58(ctx, base);
  cri_jobs_in_flight.fetch_sub(1);
  if (REXCVAR_GET(sonrac_cri_wakeup_probe)) {
    const auto* thread = rex::system::XThread::GetCurrentThread();
    const uint32_t thread_id = thread ? thread->thread_id() : 0;
    cri_time_job[thread_id == 0xF ? 0 : (thread_id & 1) ? 2 : 1].Add(
        std::chrono::duration<double, std::milli>(Clock::now() - time_begin).count());
  }
}

extern "C" REX_FUNC(sub_82E98D40) {
  if (REXCVAR_GET(sonrac_cri_wakeup_probe)) {
    const uint32_t caller = uint32_t(ctx.lr);
    bool is_new = false;
    {
      std::lock_guard lock(counts_mutex);
      is_new = seen_callsites.emplace("sub_82E98D40", caller).second;
    }
    if (is_new) {
      uint32_t frames[6]{};
      uint32_t sp = ctx.r1.u32;
      for (uint32_t i = 0; i < 6; ++i) {
        const uint32_t parent_sp = REX_LOAD_U32(sp);
        if (parent_sp <= sp || parent_sp - sp > 0x100000) {
          break;
        }
        frames[i] = REX_LOAD_U32(parent_sp - 8);
        sp = parent_sp;
      }
      const auto* thread = rex::system::XThread::GetCurrentThread();
      const uint32_t thread_id = thread ? thread->thread_id() : 0;
      REXLOG_INFO("[CriObjectEntry] thread={:08X} lr={:08X} stack={:08X},{:08X},{:08X},{:08X},{:08X},{:08X}",
                  thread_id, caller, frames[0], frames[1], frames[2], frames[3], frames[4], frames[5]);
    }
  }
  __imp__sub_82E98D40(ctx, base);
}

extern "C" REX_FUNC(sub_82E803C8) {
  const uint32_t caller = uint32_t(ctx.lr);
  if (REXCVAR_GET(sonrac_cri_wakeup_probe)) {
    bool is_new = false;
    {
      std::lock_guard lock(counts_mutex);
      is_new = seen_callsites.emplace("sub_82E803C8", caller).second;
    }
    if (is_new) {
      const auto* thread = rex::system::XThread::GetCurrentThread();
      const uint32_t thread_id = thread ? thread->thread_id() : 0;
      REXLOG_INFO("[CriConvertEntry] thread={:08X} caller={:08X}", thread_id, caller);
    }
  }
  __imp__sub_82E803C8(ctx, base);
  if (REXCVAR_GET(sonrac_cri_wakeup_probe)) {
    Record("sub_82E803C8", caller);
  }
}

extern "C" REX_FUNC(sub_82E72518) {
  const uint32_t caller = uint32_t(ctx.lr);
  if (REXCVAR_GET(sonrac_cri_wakeup_probe)) {
    bool is_new = false;
    {
      std::lock_guard lock(counts_mutex);
      is_new = seen_callsites.emplace("sub_82E72518", caller).second;
    }
    if (is_new) {
      const auto* thread = rex::system::XThread::GetCurrentThread();
      const uint32_t thread_id = thread ? thread->thread_id() : 0;
      REXLOG_INFO("[CriFrameEntry] thread={:08X} caller={:08X} r3={:08X} r4={:08X} r5={:08X}",
                  thread_id, caller, ctx.r3.u32, ctx.r4.u32, ctx.r5.u32);
    }
  }
  const auto time_begin = Clock::now();
  __imp__sub_82E72518(ctx, base);
  if (REXCVAR_GET(sonrac_cri_wakeup_probe)) {
    cri_time_frame.Add(std::chrono::duration<double, std::milli>(Clock::now() - time_begin).count());
    Record("sub_82E72518", caller);
  }
}

extern "C" REX_FUNC(sub_82E72748) {
  const uint32_t caller = uint32_t(ctx.lr);
  if (REXCVAR_GET(sonrac_cri_wakeup_probe)) {
    bool is_new = false;
    {
      std::lock_guard lock(counts_mutex);
      is_new = seen_callsites.emplace("sub_82E72748", caller).second;
    }
    if (is_new) {
      const auto* thread = rex::system::XThread::GetCurrentThread();
      const uint32_t thread_id = thread ? thread->thread_id() : 0;
      REXLOG_INFO("[CriDecodeEntry] thread={:08X} caller={:08X} r3={:08X} r4={:08X} r5={:08X}",
                  thread_id, caller, ctx.r3.u32, ctx.r4.u32, ctx.r5.u32);
    }
  }
  __imp__sub_82E72748(ctx, base);
  if (REXCVAR_GET(sonrac_cri_wakeup_probe)) {
    Record("sub_82E72748", caller);
  }
}

std::atomic<bool> cri_frame_copied_this_task{false};

extern "C" REX_FUNC(sub_82E649F8) {
  cri_frame_copied_this_task.store(true);
  const uint32_t caller = uint32_t(ctx.lr);
  if (REXCVAR_GET(sonrac_cri_wakeup_probe)) {
    bool is_new = false;
    {
      std::lock_guard lock(counts_mutex);
      is_new = seen_callsites.emplace("sub_82E649F8", caller).second;
    }
    if (is_new) {
      const auto* thread = rex::system::XThread::GetCurrentThread();
      const uint32_t thread_id = thread ? thread->thread_id() : 0;
      REXLOG_INFO("[CriApiEntry] thread={:08X} caller={:08X} r3={:08X} r4={:08X}",
                  thread_id, caller, ctx.r3.u32, ctx.r4.u32);
    }
  }
  const auto time_begin = Clock::now();
  __imp__sub_82E649F8(ctx, base);
  if (REXCVAR_GET(sonrac_cri_wakeup_probe)) {
    cri_time_decode.Add(std::chrono::duration<double, std::milli>(Clock::now() - time_begin).count());
    Record("sub_82E649F8", caller);
  }
}

REXCVAR_DEFINE_BOOL(sonrac_cri_trace, false, "SonRac/Diagnostics",
                    "Log one line per movie task call (first 240 calls)");
REXCVAR_DEFINE_BOOL(sonrac_cri_rekick, false, "SonRac/Video",
                    "After the movie task hands out a picture, run the CRI server again so the "
                    "next picture starts decoding in the same game frame");
REXCVAR_DEFINE_INT32(sonrac_cri_task_repeat_gap_ms, 0, "SonRac/Diagnostics",
                     "Experiment: sleep this long between repeated movie task runs");
REXCVAR_DEFINE_INT32(sonrac_cri_task_repeat, 1, "SonRac/Diagnostics",
                     "Experiment: run the per-frame CRI movie task this many times per call");

extern "C" REX_FUNC(sub_82E5A928) {
  const uint32_t caller = uint32_t(ctx.lr);
  const int32_t repeat = REXCVAR_GET(sonrac_cri_task_repeat);
  // The callee clobbers the argument registers; give every run the original ones.
  const auto saved_r3 = ctx.r3, saved_r4 = ctx.r4, saved_r5 = ctx.r5, saved_r6 = ctx.r6,
             saved_r7 = ctx.r7, saved_r8 = ctx.r8, saved_r9 = ctx.r9, saved_r10 = ctx.r10;
  const auto saved_r1 = ctx.r1;
  const auto saved_lr = ctx.lr;
  for (int32_t i = 1; i < repeat; ++i) {
    __imp__sub_82E5A928(ctx, base);
    // Let the decode started by this run finish on the worker threads, so the
    // next run sees the picture done (and can hand it out and start the next).
    const int32_t gap_ms = REXCVAR_GET(sonrac_cri_task_repeat_gap_ms);
    if (gap_ms > 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(gap_ms));
    } else {
      const auto wait_end = Clock::now() + std::chrono::milliseconds(20);
      std::this_thread::sleep_for(std::chrono::microseconds(200));
      while (cri_jobs_in_flight.load() > 0 && Clock::now() < wait_end) {
        std::this_thread::sleep_for(std::chrono::microseconds(200));
      }
    }
    ctx.r3 = saved_r3; ctx.r4 = saved_r4; ctx.r5 = saved_r5; ctx.r6 = saved_r6;
    ctx.r7 = saved_r7; ctx.r8 = saved_r8; ctx.r9 = saved_r9; ctx.r10 = saved_r10;
    ctx.r1 = saved_r1; ctx.lr = saved_lr;
  }
  const auto time_begin = Clock::now();
  cri_frame_copied_this_task.store(false);
  const uint32_t jobs_before = cri_jobs_started.load();
  const auto trace_begin = Clock::now();
  __imp__sub_82E5A928(ctx, base);
  if (REXCVAR_GET(sonrac_cri_trace)) {
    // One line per task call: when, how long, decode jobs started inside it,
    // jobs started elsewhere since the last call, and whether a picture came out.
    static Clock::time_point last_end{};
    static uint32_t last_jobs = 0;
    static int lines = 0;
    const auto now = Clock::now();
    if (lines < 240) {
      ++lines;
      const uint32_t jobs_after = cri_jobs_started.load();
      const uint32_t player = REX_LOAD_U32(0x8320D83C);
      const uint32_t stream = player ? REX_LOAD_U32(player + 8) : 0;
      extern std::atomic<uint32_t> cri_audio_object;
      const uint32_t audio = cri_audio_object.load();
      if (audio) {
        REXLOG_INFO("[CriAudioState] mode {} callback {:08X} arg {:08X}", REX_LOAD_U32(audio + 4),
                    REX_LOAD_U32(audio + 8), REX_LOAD_U32(audio + 12));
      }
      REXLOG_INFO("[CriLoader] in_flight {} state {} mode {} stream[3184] {} stream[3188] {} "
                  "req {} {}",
                  player ? REX_LOAD_U32(player + 1000) : 0, player ? REX_LOAD_U32(player + 1168) : 0,
                  player ? REX_LOAD_U32(player + 1164) : 0, stream ? REX_LOAD_U32(stream + 3184) : 0,
                  stream ? REX_LOAD_U32(stream + 3188) : 0, player ? REX_LOAD_U32(player + 984) : 0,
                  player ? REX_LOAD_U32(player + 988) : 0);
      REXLOG_INFO("[CriTrace] gap {:.1f} ms, task {:.1f} ms, jobs inside {}, jobs between {}, picture {}",
                  last_end == Clock::time_point()
                      ? 0.0
                      : std::chrono::duration<double, std::milli>(trace_begin - last_end).count(),
                  std::chrono::duration<double, std::milli>(now - trace_begin).count(),
                  jobs_after - jobs_before, jobs_before - last_jobs,
                  cri_frame_copied_this_task.load() ? 1 : 0);
      last_jobs = jobs_after;
    }
    last_end = now;
  }
  // The task hands out a picture and releases it after the CRI server ran for
  // this frame, so the decode of the next one would wait for the next game
  // frame. Run the server steps once more right away.
  if (REXCVAR_GET(sonrac_cri_rekick) && cri_frame_copied_this_task.load()) {
    const uint32_t player = REX_LOAD_U32(0x8320D83C);
    if (player) {
      const auto keep_r1 = ctx.r1;
      const auto keep_lr = ctx.lr;
      const auto keep_r3 = ctx.r3;
      ctx.r3.u64 = player;
      ctx.r4.u64 = 0x8320D980;
      sub_823CE370(ctx, base);
      ctx.r3.u64 = player;
      ctx.r4.u64 = 0x8320D980;
      sub_82414120(ctx, base);
      ctx.r1 = keep_r1;
      ctx.lr = keep_lr;
      ctx.r3 = keep_r3;
    }
  }
  if (REXCVAR_GET(sonrac_cri_wakeup_probe)) {
    cri_time_task.Add(std::chrono::duration<double, std::milli>(Clock::now() - time_begin).count());
    Record("sub_82E5A928", caller);
  }
}

// These CRI scheduler targets were observed during video startup. Count their
// actual execution separately from the indirect-dispatch registration event.
extern "C" REX_FUNC(sub_82E5A630) {
  const uint32_t caller = uint32_t(ctx.lr);
  __imp__sub_82E5A630(ctx, base);
  if (REXCVAR_GET(sonrac_cri_wakeup_probe)) {
    Record("sub_82E5A630", caller);
  }
}

extern "C" REX_FUNC(sub_82E5AC80) {
  const uint32_t caller = uint32_t(ctx.lr);
  __imp__sub_82E5AC80(ctx, base);
  if (REXCVAR_GET(sonrac_cri_wakeup_probe)) {
    Record("sub_82E5AC80", caller);
  }
}

extern "C" void sonrac_probe_scheduled_callback(uint32_t target, uint32_t r3, uint32_t r4,
                                                 uint32_t r5, uint32_t callsite) {
  if (!REXCVAR_GET(sonrac_cri_wakeup_probe)) {
    return;
  }
  bool is_new = false;
  std::map<std::pair<uint32_t, uint32_t>, uint64_t> report;
  double seconds = 0.0;
  const auto now = Clock::now();
  const auto* thread = rex::system::XThread::GetCurrentThread();
  const uint32_t thread_id = thread ? thread->thread_id() : 0;
  {
    std::lock_guard lock(counts_mutex);
    is_new = seen_dispatch_targets.emplace(target, callsite).second;
    if (dispatch_window_start == Clock::time_point{}) {
      dispatch_window_start = now;
    }
    ++dispatch_counts[{target, thread_id}];
    seconds = std::chrono::duration<double>(now - dispatch_window_start).count();
    if (seconds >= 2.0) {
      report.swap(dispatch_counts);
      dispatch_window_start = now;
    }
  }
  if (is_new) {
    REXLOG_INFO("[FrameDispatch] thread={:08X} target={:08X} site={:08X} args={:08X},{:08X},{:08X}",
                thread_id, target, callsite, r3, r4, r5);
  }
  for (const auto& [key, calls] : report) {
    if (calls / seconds >= 1.0) {
      REXLOG_INFO("[FrameDispatchRate] thread={:08X} target={:08X} {:.1f} calls/s",
                  key.second, key.first, calls / seconds);
    }
  }
}

extern "C" REX_FUNC(sub_82EA67C8) {
  const uint32_t caller = uint32_t(ctx.lr);
  __imp__sub_82EA67C8(ctx, base);
  if (REXCVAR_GET(sonrac_cri_wakeup_probe)) {
    Record("sub_82EA67C8", caller);
  }
}

extern "C" REX_FUNC(sub_82EA6880) {
  const uint32_t caller = uint32_t(ctx.lr);
  const uint32_t callback = ctx.r4.u32;
  const uint32_t callback_data = ctx.r5.u32;
  const uint32_t thread_record = ctx.r3.u32;
  bool new_callback = false;
  if (REXCVAR_GET(sonrac_cri_wakeup_probe)) {
    std::lock_guard lock(counts_mutex);
    new_callback = seen_thread_callbacks.emplace(callback, callback_data).second;
  }
  __imp__sub_82EA6880(ctx, base);
  if (REXCVAR_GET(sonrac_cri_wakeup_probe)) {
    Record("sub_82EA6880", caller);
    if (new_callback) {
      const auto* thread = rex::system::XThread::GetCurrentThread();
      const uint32_t thread_id = thread ? thread->thread_id() : 0;
      REXLOG_INFO("[CriCallback] thread={:08X} record={:08X} callback={:08X} data={:08X} caller={:08X}",
                  thread_id, thread_record, callback, callback_data, caller);
    }
  }
}

extern "C" REX_FUNC(sub_82EA6970) {
  const uint32_t caller = uint32_t(ctx.lr);
  __imp__sub_82EA6970(ctx, base);
  if (REXCVAR_GET(sonrac_cri_wakeup_probe)) {
    Record("sub_82EA6970", caller);
  }
}

extern "C" REX_FUNC(sub_82EA6D78) {
  const uint32_t caller = uint32_t(ctx.lr);
  __imp__sub_82EA6D78(ctx, base);
  if (REXCVAR_GET(sonrac_cri_wakeup_probe)) {
    Record("sub_82EA6D78", caller);
  }
}

extern "C" REX_FUNC(sub_82EA6E90) {
  const uint32_t caller = uint32_t(ctx.lr);
  __imp__sub_82EA6E90(ctx, base);
  if (REXCVAR_GET(sonrac_cri_wakeup_probe)) {
    Record("sub_82EA6E90", caller);
  }
}

extern "C" REX_FUNC(sub_82EA6ED8) {
  const uint32_t caller = uint32_t(ctx.lr);
  __imp__sub_82EA6ED8(ctx, base);
  if (REXCVAR_GET(sonrac_cri_wakeup_probe)) {
    Record("sub_82EA6ED8", caller);
  }
}

// CriSmpMvSystemTimer: GetTime(this, u64* count, u64* unit) and SetPause(this, int).
// Logs how fast the movie clock advances against the host clock, and how often
// the game pauses / resumes it.
namespace {
struct CriClockStats {
  std::mutex mutex;
  Clock::time_point window{};
  double first_seconds = -1.0, last_seconds = 0.0;
  uint64_t gets = 0, pauses = 0, resumes = 0;
} cri_clock_stats;
}  // namespace

extern "C" REX_FUNC(sub_82E65EA0) {
  const uint32_t count_ptr = ctx.r4.u32, unit_ptr = ctx.r5.u32;
  __imp__sub_82E65EA0(ctx, base);
  if (!REXCVAR_GET(sonrac_cri_wakeup_probe)) {
    return;
  }
  const uint64_t count = REX_LOAD_U64(count_ptr);
  const uint64_t unit = REX_LOAD_U64(unit_ptr);
  const double movie_seconds = unit ? double(count) / double(unit) : 0.0;
  const auto now = Clock::now();
  std::lock_guard lock(cri_clock_stats.mutex);
  auto& st = cri_clock_stats;
  if (st.window == Clock::time_point()) st.window = now;
  if (st.first_seconds < 0.0) st.first_seconds = movie_seconds;
  st.last_seconds = movie_seconds;
  ++st.gets;
  const double host = std::chrono::duration<double>(now - st.window).count();
  if (host >= 2.0) {
    REXLOG_INFO("[CriClock] movie clock {:.3f} s per host s ({:.1f} gets/s, {:.1f} pauses/s, "
                "{:.1f} resumes/s, now {:.3f} s, unit {})",
                (st.last_seconds - st.first_seconds) / host, st.gets / host, st.pauses / host,
                st.resumes / host, movie_seconds, unit);
    st.window = now;
    st.first_seconds = movie_seconds;
    st.gets = st.pauses = st.resumes = 0;
  }
}

extern "C" REX_FUNC(sub_82E65DF8) {
  const uint32_t pause = ctx.r4.u32;
  __imp__sub_82E65DF8(ctx, base);
  if (REXCVAR_GET(sonrac_cri_wakeup_probe)) {
    std::lock_guard lock(cri_clock_stats.mutex);
    ++(pause ? cri_clock_stats.pauses : cri_clock_stats.resumes);
  }
}

// CriAu output thread (start 82407670 -> sub_82E641B8, r3 = the CriAu object).
// Kept so the movie trace can show its mode (+4, 1 = feeding PCM, otherwise
// silence) and PCM callback (+8).
std::atomic<uint32_t> cri_audio_object{0};

extern "C" REX_FUNC(sub_82E641B8) {
  cri_audio_object.store(ctx.r3.u32);
  REXLOG_INFO("[CriAudio] output thread object {:08X}", ctx.r3.u32);
  __imp__sub_82E641B8(ctx, base);
}

// CriAu's PCM pull (r6 = samples wanted, returns samples given).
namespace {
struct CriPcmStats {
  std::mutex mutex;
  Clock::time_point window{};
  uint64_t calls = 0, wanted = 0, given = 0, empty = 0;
} cri_pcm_stats;
}  // namespace

extern "C" REX_FUNC(sub_82E65898) {
  const uint32_t wanted = ctx.r6.u32;
  __imp__sub_82E65898(ctx, base);
  if (!REXCVAR_GET(sonrac_cri_wakeup_probe)) {
    return;
  }
  const uint32_t given = ctx.r3.u32;
  const auto now = Clock::now();
  std::lock_guard lock(cri_pcm_stats.mutex);
  auto& st = cri_pcm_stats;
  if (st.window == Clock::time_point()) st.window = now;
  ++st.calls;
  st.wanted += wanted;
  st.given += given;
  st.empty += given == 0;
  const double seconds = std::chrono::duration<double>(now - st.window).count();
  if (seconds >= 2.0) {
    REXLOG_INFO("[CriPcm] {:.1f} pulls/s, {:.0f} samples/s wanted, {:.0f} given, {:.1f} empty/s",
                st.calls / seconds, st.wanted / seconds, st.given / seconds, st.empty / seconds);
    st.window = now;
    st.calls = st.wanted = st.given = st.empty = 0;
  }
}
