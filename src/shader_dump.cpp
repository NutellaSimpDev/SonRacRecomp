// Native renderer, shaders: dump every loaded Xbox 360 shader container
// (flags 0x102A11xx) with its microcode, to build the offline shader library
// (XenosRecomp needs the container tables, which the microcode alone lacks).
//
// In the packs the containers have their physical part zeroed; the loader puts
// the microcode elsewhere and links it. Once loaded, both are in the 512 MB of
// physical memory. --sonrac_shader_dump_seconds=N scans it once after N
// seconds and writes <user>/shader_dump.bin:
//   per container: u32 guest physical address, u32 virtualSize, u32 physicalSize,
//   u32 Shader::physicalOffset as found in memory, the virtual part, then
//   physicalSize bytes read at physicalOffset (zeros if it is not an address).

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>

#if defined(__linux__)
#include <sys/uio.h>
#include <unistd.h>
#endif

#include <vector>

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/system/kernel_state.h>
#include <rex/system/xmemory.h>
#include <mutex>
#include <unordered_set>

#include "generated/sonicracingtransformed_init.h"

REXCVAR_DEFINE_INT32(sonrac_shader_dump_seconds, 0, "SonRac/Native",
                     "Dump the loaded shader containers and their microcode after this many "
                     "seconds (0 = off, negative = when <files>/shaderdump_now exists)");

namespace {
std::atomic<bool> g_dumped{false};
const auto g_start = std::chrono::steady_clock::now();

uint32_t Be32(const uint8_t* p) {
  return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3];
}

// Copies [src, src + size) without faulting on pages that are not readable
// (watched by the emulation or never committed): process_vm_readv on our own
// process returns an error for them instead. Unreadable 4 KB pages become zeros.
void SafeRead(const uint8_t* src, size_t size, uint8_t* dst) {
#if !defined(__linux__)
  std::memcpy(dst, src, size);  // PC builds: no watched pages to step around
#else
  constexpr size_t kPage = 4096;
  for (size_t off = 0; off < size; off += kPage) {
    const size_t n = std::min(kPage, size - off);
    iovec local{dst + off, n};
    iovec remote{const_cast<uint8_t*>(src + off), n};
    if (process_vm_readv(getpid(), &local, 1, &remote, 1, 0) != ssize_t(n)) {
      std::memset(dst + off, 0, n);
    }
  }
#endif
}

// The D3D shader objects bound at each IM_LOAD (sub_82CE27D8: device in r3,
// pixel shader at +13068, vertex shader at +13072), to mark the used ones.
std::mutex g_used_mutex;
std::unordered_set<uint32_t> g_used;

// The D3D shader objects keep the whole container: a pixel shader (type 7)
// at +40 with its microcode at *(+24), a vertex shader (type 6) at +872 with
// its microcode at *(+32); the container's Shader::physicalOffset is added to it.
// Scans the guest heaps and the physical memory for them and writes
// <user>/shader_lib.bin, per object: u32 address, u32 stage (0 pixel, 1 vertex),
// u32 used at an IM_LOAD, u32 microcode address, u32 virtualSize,
// u32 physicalSize, the container, the microcode.
void DumpObjects(uint8_t* base) {
  FILE* f = std::fopen("/storage/emulated/0/Android/data/com.sonrac.game/files/user/shader_lib.bin",
                       "wb");
  if (!f) return;
  std::unordered_set<uint32_t> used;
  {
    std::lock_guard<std::mutex> lock(g_used_mutex);
    used = g_used;
  }
  constexpr uint32_t kChunk = 1u << 20;
  std::vector<uint8_t> chunk(kChunk + 1024), micro(0x20000);
  uint32_t found[2] = {};
  for (uint64_t start = 0x40000000u; start < 0xC0000000u; start += kChunk) {
    SafeRead(base + start, kChunk + 1024, chunk.data());
    for (uint32_t i = 0; i < kChunk; i += 4) {
      const uint8_t* o = chunk.data() + i;
      const uint32_t type = Be32(o);
      if ((type != 6 && type != 7) || Be32(o + 4) != 1) continue;
      const uint32_t stage = type == 6;
      const uint32_t container_offset = stage ? 872 : 40;
      if (i + container_offset + 36 > kChunk + 1024) continue;
      const uint8_t* c = o + container_offset;
      if (c[0] != 0x10 || c[1] != 0x2A || c[2] != 0x11) continue;
      const uint32_t vsize = Be32(c + 4), psize = Be32(c + 8), shader_offset = Be32(c + 24);
      if (!vsize || vsize > 0x8000 || psize > micro.size() || shader_offset + 4 > vsize) continue;
      const uint32_t address = uint32_t(start) + i;
      std::vector<uint8_t> container(vsize);
      SafeRead(base + address + container_offset, vsize, container.data());
      // The physical part from its start: the shader's literal constants (the definition table's values)
      // are there, before the microcode at Shader::physicalOffset, and the D3D loads them from it with
      // LOAD_ALU_CONSTANT. Stage bit 8 marks this layout for tools/native/shader_lib_bins.py.
      const uint32_t fisica = Be32(o + (stage ? 32 : 24));
      const uint32_t desplazamiento = Be32(container.data() + shader_offset);
      const uint32_t total = desplazamiento + psize;
      if (desplazamiento > 0x10000 || total > micro.size()) continue;
      SafeRead(base + fisica, total, micro.data());
      const uint32_t header[6] = {address, stage | 0x100u, uint32_t(used.count(address)), fisica, vsize,
                                  total};
      std::fwrite(header, sizeof(header), 1, f);
      std::fwrite(container.data(), 1, vsize, f);
      std::fwrite(micro.data(), 1, total, f);
      ++found[stage];
    }
  }
  std::fclose(f);
  REXLOG_INFO("[ShaderDump] shader objects: {} vertex, {} pixel, {} used", found[0], found[1],
              used.size());
}

void Dump() {
  auto* memory = rex::system::kernel_memory();
  if (!memory) return;
  const uint8_t* phys = memory->physical_membase();
  constexpr uint32_t kSize = 512u << 20;
  // A readable copy of the whole physical memory, 1 MB at a time.
  std::vector<uint8_t> image(kSize);
  constexpr size_t kChunk = 1 << 20;
  for (size_t off = 0; off < kSize; off += kChunk) {
    SafeRead(phys + off, kChunk, image.data() + off);
  }
  const uint8_t* mem = image.data();
  FILE* f = std::fopen("/storage/emulated/0/Android/data/com.sonrac.game/files/user/shader_dump.bin",
                       "wb");
  if (!f) {
    REXLOG_ERROR("[ShaderDump] cannot open the output");
    return;
  }
  uint32_t found = 0, linked = 0;
  static uint8_t zeros[0x20000];
  for (uint32_t i = 0; i + 36 <= kSize; i += 4) {
    if (mem[i] != 0x10 || mem[i + 1] != 0x2A || mem[i + 2] != 0x11) continue;
    const uint8_t* c = mem + i;
    const uint32_t vsize = Be32(c + 4), psize = Be32(c + 8);
    const uint32_t shader_offset = Be32(c + 24);
    if (Be32(c + 28) != 0 || Be32(c + 32) != 0 || !vsize || vsize > 0x20000 ||
        psize > 0x20000 || shader_offset + 8 > vsize || i + vsize > kSize) {
      continue;
    }
    const uint32_t physical_offset = Be32(c + shader_offset);
    const uint32_t header[4] = {i, vsize, psize, physical_offset};
    std::fwrite(header, sizeof(header), 1, f);
    std::fwrite(c, 1, vsize, f);
    const uint32_t micro = physical_offset & 0x1FFFFFFF;
    if (physical_offset && micro + psize <= kSize) {
      std::fwrite(mem + micro, 1, psize, f);
      ++linked;
    } else {
      std::fwrite(zeros, 1, psize, f);
    }
    ++found;
  }
  std::fclose(f);
  REXLOG_INFO("[ShaderDump] {} containers, {} with a linked physical address", found, linked);
}
}  // namespace

extern "C" REX_FUNC(sub_82CE27D8) {
  if (REXCVAR_GET(sonrac_shader_dump_seconds) != 0) {
    uint8_t words[8];
    SafeRead(base + ctx.r3.u32 + 13068, 8, words);
    std::lock_guard<std::mutex> lock(g_used_mutex);
    g_used.insert(Be32(words));
    g_used.insert(Be32(words + 4));
  }
  __imp__sub_82CE27D8(ctx, base);
}

void SonracShaderDumpPoll() {
  const int32_t seconds = REXCVAR_GET(sonrac_shader_dump_seconds);
  if (seconds == 0 || g_dumped.load(std::memory_order_relaxed)) return;
  if (seconds < 0) {
    // Negative: dump when <files>/shaderdump_now appears (checked every 30 frames).
    static uint32_t frame = 0;
    if (++frame % 30 ||
        !std::filesystem::exists("/storage/emulated/0/Android/data/com.sonrac.game/files/shaderdump_now")) {
      return;
    }
  } else if (std::chrono::steady_clock::now() - g_start < std::chrono::seconds(seconds)) {
    return;
  }
  if (g_dumped.exchange(true)) return;
  DumpObjects(rex::system::kernel_memory()->virtual_membase());
  Dump();
}
