// SonRac - the CRI movie frame conversion (YUV 4:2:0 to ARGB8888), native.
//
// sub_82E7F918 is CRI's colour conversion of each decoded movie frame (the boot
// logos and the intro are CRI USM movies): for every pixel it adds lookup-table
// terms for luma and chroma, shifts them right by 20 and clamps them through
// another table. Recompiled it took half of the game's render thread
// (SlRenderer) during the logos - ~920,000 pixels per 1280x720 frame through
// the emulated registers and byte-swapped loads.
//
// The native version reads the same tables from the game's memory and does the
// same integer arithmetic, so its output is bit-identical. Guard (as in
// nfsmw-nx's native functions): the first calls run both versions and compare
// every output pixel; the native one is only used after they all matched, and
// any later difference (checked again now and then) turns it off for good.
//
// Arguments, from the generated code:
//   r3: +0 luma plane, +4 chroma plane 2, +8 chroma plane 1, +12 luma pitch,
//       +16 chroma 2 pitch, +20 chroma 1 pitch (bytes)
//   r4: +0 output, +4 width, +8 height, +12 output pitch (bytes)
// The loop covers width / 8 blocks of 8 pixels and height / 2 pairs of rows.
#include <algorithm>
#include <chrono>
#include <climits>
#include <cstdint>
#include <cstring>
#include <vector>

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/system/kernel_state.h>
#include <rex/system/xmemory.h>
#include <rex/thread/mutex.h>

#include "generated/sonicracingtransformed_init.h"

REXCVAR_DEFINE_BOOL(sonrac_native_video_convert, true, "SonRac/Performance",
                    "Convert the CRI movie frames (YUV to ARGB) with native code instead of the "
                    "recompiled function (checked against it first)");

namespace {

constexpr uint32_t kTables = 0x832ECF50;
constexpr uint32_t kVerifyCalls = 16;
constexpr uint32_t kReverifyInterval = 4096;

uint32_t calls = 0;
uint32_t verified_calls = 0;
bool disabled = false;

// Report: conversions per second (one per new movie frame) and their cost.
std::chrono::steady_clock::time_point report_start;
uint32_t report_calls = 0;
double report_ms = 0.0;

void Report(int32_t width, int32_t height, double ms) {
  auto now = std::chrono::steady_clock::now();
  if (!report_calls) {
    report_start = now;
  }
  ++report_calls;
  report_ms += ms;
  double seconds = std::chrono::duration<double>(now - report_start).count();
  if (seconds >= 2.0) {
    REXLOG_INFO("[NativeVideo] {:.1f} conversions/s ({}x{}), {:.2f} ms each", report_calls / seconds,
                width, height, report_ms / report_calls);
    report_calls = 0;
    report_ms = 0.0;
  }
}

inline uint32_t LoadBE32(const uint8_t* base, uint32_t address) {
  uint32_t value;
  std::memcpy(&value, base + address, 4);
  return __builtin_bswap32(value);
}

struct ConvertArgs {
  uint32_t luma, chroma2, chroma1;
  uint32_t luma_pitch, chroma2_pitch, chroma1_pitch;
  uint32_t output;
  int32_t width, height;
  uint32_t output_pitch;
};

ConvertArgs ReadArgs(const uint8_t* base, uint32_t r3, uint32_t r4) {
  ConvertArgs a;
  a.luma = LoadBE32(base, r3 + 0);
  a.chroma2 = LoadBE32(base, r3 + 4);
  a.chroma1 = LoadBE32(base, r3 + 8);
  a.luma_pitch = LoadBE32(base, r3 + 12);
  a.chroma2_pitch = LoadBE32(base, r3 + 16);
  a.chroma1_pitch = LoadBE32(base, r3 + 20);
  a.output = LoadBE32(base, r4 + 0);
  a.width = int32_t(LoadBE32(base, r4 + 4));
  a.height = int32_t(LoadBE32(base, r4 + 8));
  a.output_pitch = LoadBE32(base, r4 + 12);
  return a;
}

// Converts into `out` (host order words, row after row), or straight into the
// guest output if `out` is null. The tables are copied in host order first
// (luma and chroma terms, and the clamp range the frame can reach), so the
// inner loop is integer arithmetic on cached data.
void ConvertNative(uint8_t* base, const ConvertArgs& a, std::vector<uint32_t>* out) {
  const uint8_t* table = base + kTables;
  auto entry = [table](int32_t index) {
    uint32_t value;
    std::memcpy(&value, table + int64_t(index) * 4, 4);
    return __builtin_bswap32(value);
  };
  int32_t t_c1_a[256], t_c1_b[256], t_c2_a[256], t_c2_c[256], t_y[256];
  for (int32_t i = 0; i < 256; ++i) {
    t_c1_b[i] = int32_t(entry(i));
    t_c2_c[i] = int32_t(entry(256 + i));
    t_c1_a[i] = int32_t(entry(512 + i));
    t_y[i] = int32_t(entry(768 + i));
    t_c2_a[i] = int32_t(entry(1024 + i));
  }
  // The clamp indices the arithmetic can reach, from the table extremes.
  auto min_max = [](const int32_t* t, int64_t& lo, int64_t& hi) {
    lo = t[0];
    hi = t[0];
    for (int32_t i = 1; i < 256; ++i) {
      lo = std::min<int64_t>(lo, t[i]);
      hi = std::max<int64_t>(hi, t[i]);
    }
  };
  int64_t y_lo, y_hi, b_lo, b_hi, c_lo, c_hi, a1_lo, a1_hi, a2_lo, a2_hi;
  min_max(t_y, y_lo, y_hi);
  min_max(t_c1_b, b_lo, b_hi);
  min_max(t_c2_c, c_lo, c_hi);
  min_max(t_c1_a, a1_lo, a1_hi);
  min_max(t_c2_a, a2_lo, a2_hi);
  int64_t sum_lo = std::min({y_lo + b_lo, y_lo - (a1_hi + a2_hi), y_lo + c_lo});
  int64_t sum_hi = std::max({y_hi + b_hi, y_hi - (a1_lo + a2_lo), y_hi + c_hi});
  bool fits_int32 = sum_lo >= INT32_MIN && sum_hi <= INT32_MAX && a1_lo + a2_lo >= INT32_MIN &&
                    a1_hi + a2_hi <= INT32_MAX;
  int32_t clamp_lo = int32_t(sum_lo >> 20), clamp_hi = int32_t(sum_hi >> 20);
  if (!fits_int32 || clamp_hi - clamp_lo > 8192) {
    clamp_lo = 0;
    clamp_hi = -1;  // Fall back to reading the clamp table from guest memory.
  }
  std::vector<uint32_t> clamp_table;
  if (clamp_hi >= clamp_lo) {
    clamp_table.resize(size_t(clamp_hi - clamp_lo + 1));
    for (int32_t i = clamp_lo; i <= clamp_hi; ++i) {
      clamp_table[size_t(i - clamp_lo)] = entry(1280 + i);
    }
  }
  const uint32_t* clamp = clamp_table.empty() ? nullptr : clamp_table.data() - clamp_lo;
  auto clamped = [&](int32_t index) { return clamp ? clamp[index] : entry(1280 + index); };

  int32_t blocks = a.width / 8;
  int32_t row_pairs = a.height / 2;
  uint32_t columns = uint32_t(blocks) * 8;
  uint32_t luma_pitch = a.luma_pitch & ~3u;
  uint32_t output_pitch = (a.output_pitch >> 2) << 2;
  if (out) {
    out->assign(size_t(row_pairs) * 2 * columns, 0);
  }
  for (int32_t pair = 0; pair < row_pairs; ++pair) {
    const uint8_t* luma[2];
    luma[0] = base + a.luma + uint32_t(pair) * 2 * a.luma_pitch;
    luma[1] = luma[0] + luma_pitch;
    const uint8_t* chroma1 = base + a.chroma1 + uint32_t(pair) * a.chroma1_pitch;
    const uint8_t* chroma2 = base + a.chroma2 + uint32_t(pair) * a.chroma2_pitch;
    uint32_t* host_rows[2] = {nullptr, nullptr};
    uint8_t* output[2];
    output[0] = base + a.output + uint32_t(pair) * 2 * output_pitch;
    output[1] = output[0] + output_pitch;
    if (out) {
      host_rows[0] = out->data() + size_t(pair) * 2 * columns;
      host_rows[1] = host_rows[0] + columns;
    }
    for (uint32_t k = 0; k < columns / 2; ++k) {
      int32_t c1 = chroma1[k];
      int32_t c2 = chroma2[k];
      int32_t term_a = int32_t(uint32_t(t_c1_a[c1]) + uint32_t(t_c2_a[c2]));
      int32_t term_b = t_c1_b[c1];
      int32_t term_c = t_c2_c[c2];
      for (uint32_t row = 0; row < 2; ++row) {
        for (uint32_t i = 0; i < 2; ++i) {
          uint32_t x = k * 2 + i;
          int32_t yy = t_y[luma[row][x]];
          uint32_t first = clamped(int32_t(uint32_t(yy) + uint32_t(term_b)) >> 20);
          uint32_t second = clamped(int32_t(uint32_t(yy) - uint32_t(term_a)) >> 20);
          uint32_t third = clamped(int32_t(uint32_t(yy) + uint32_t(term_c)) >> 20);
          uint32_t packed = 0xFFFF0000u | ((first << 8) & 0x0000FF00u);
          packed |= second;
          packed = ((packed << 8) | (packed >> 24)) & 0xFFFFFF00u;
          packed |= third;
          if (out) {
            host_rows[row][x] = packed;
          } else {
            uint32_t swapped = __builtin_bswap32(packed);
            std::memcpy(output[row] + x * 4, &swapped, 4);
          }
        }
      }
    }
  }
}

// Compares the guest output (written by the original) with the native result.
bool OutputMatches(const uint8_t* base, const ConvertArgs& a, const std::vector<uint32_t>& out) {
  int32_t blocks = a.width / 8;
  int32_t row_pairs = a.height / 2;
  uint32_t output_pitch = (a.output_pitch >> 2) << 2;
  size_t i = 0;
  for (int32_t pair = 0; pair < row_pairs; ++pair) {
    for (uint32_t row = 0; row < 2; ++row) {
      uint32_t output = a.output + uint32_t(pair) * 2 * output_pitch + row * output_pitch;
      for (int32_t x = 0; x < blocks * 8; ++x, ++i) {
        if (i >= out.size() || LoadBE32(base, output + uint32_t(x) * 4) != out[i]) {
          return false;
        }
      }
    }
  }
  return i == out.size();
}

}  // namespace

extern "C" REX_FUNC(sub_82E7F918) {
  if (disabled || !REXCVAR_GET(sonrac_native_video_convert)) {
    __imp__sub_82E7F918(ctx, base);
    return;
  }
  ConvertArgs args = ReadArgs(base, ctx.r3.u32, ctx.r4.u32);
  if (args.width <= 0 || args.height <= 0) {
    __imp__sub_82E7F918(ctx, base);
    return;
  }
  ++calls;
  auto call_start = std::chrono::steady_clock::now();
  struct ReportOnExit {
    const ConvertArgs& args;
    std::chrono::steady_clock::time_point start;
    ~ReportOnExit() {
      Report(args.width, args.height, std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() -
                                                             start)
                       .count());
    }
  } report_on_exit{args, call_start};
  bool verify = verified_calls < kVerifyCalls || (calls % kReverifyInterval) == 0;
  if (verify) {
    // Native result first (the original may write over its own inputs).
    std::vector<uint32_t> native;
    ConvertNative(base, args, &native);
    __imp__sub_82E7F918(ctx, base);
    if (!OutputMatches(base, args, native)) {
      disabled = true;
      REXLOG_WARN(
          "[NativeVideo] DIFERENCIA: native CRI frame conversion differs from the original "
          "({}x{}) - using the original from now on",
          args.width, args.height);
      return;
    }
    if (++verified_calls == kVerifyCalls) {
      REXLOG_INFO("[NativeVideo] native CRI frame conversion verified on {} frames, now in use",
                  kVerifyCalls);
    }
    return;
  }
  // One invalidation for the whole output instead of a write-watch fault on
  // each of its pages (~900 per 1280x720 frame): the textures made from it are
  // invalidated and the pages unprotected, then written freely.
  uint32_t output_length =
      uint32_t(args.height / 2 * 2 - 1) * ((args.output_pitch >> 2) << 2) +
      uint32_t(args.width / 8 * 8) * 4;
  if (auto* memory = rex::system::kernel_memory()) {
    memory->TriggerPhysicalMemoryCallbacks(rex::thread::global_critical_region::AcquireDirect(),
                                           args.output, output_length, true, false);
  }
  ConvertNative(base, args, nullptr);
}
