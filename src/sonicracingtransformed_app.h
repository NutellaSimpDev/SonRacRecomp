
// sonicracingtransformed - ReXGlue Recompiled Project
//
// This file is yours to edit. 'rexglue migrate' will NOT overwrite it.
// Customize your app by overriding virtual hooks from rex::ReXApp.

#pragma once

#include <rex/rex_app.h>
#include <rex/ui/keybinds.h>
#include <rex/system/gpu_plugin.h>

#include <cstdio>
#include <cstdlib>
#include <mutex>

#include <rex/logging/api.h>
#include <spdlog/sinks/base_sink.h>

// The old PC configuration dialog (F1) reads cvars that live in the graphics
// plugin, which the executable does not link against with the 0.9.0 SDK. Only
// built on request; Android is configured from its launcher.
#if defined(SONRAC_CONFIG_DIALOG)
#include "sonic_config_dialog.h"
#endif
#include "android_crash_logger.h"

#if defined(_WIN32)
#include <windows.h>
#include <cstdlib>
#include <csignal>
#include <exception>
// PC: the STL's hardening checks (an index out of range, for instance) end in a fail-fast that leaves no
// trace. Before it, the CRT calls the invalid parameter handler: this one writes the return addresses and
// the executable's base to sonrac_fallo.txt next to the executable (llvm-symbolizer with the .pdb resolves
// them), and std::terminate does the same.
inline void SonracVolcarPila(const char* que) {
  void* marcos[64];
  const USHORT n = RtlCaptureStackBackTrace(1, 64, marcos, nullptr);
  char ruta[MAX_PATH];
  GetModuleFileNameA(nullptr, ruta, MAX_PATH);
  std::string archivo(ruta);
  archivo = archivo.substr(0, archivo.find_last_of('\\') + 1) + "sonrac_fallo.txt";
  if (FILE* f = std::fopen(archivo.c_str(), "w")) {
    std::fprintf(f, "%s\nbase %p\n", que, (void*)GetModuleHandleA(nullptr));
    for (USHORT i = 0; i < n; ++i) {
      HMODULE modulo = nullptr;
      GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                         static_cast<LPCSTR>(marcos[i]), &modulo);
      char nombre[MAX_PATH] = "?";
      if (modulo) GetModuleFileNameA(modulo, nombre, MAX_PATH);
      std::fprintf(f, "%p %s+0x%llx\n", marcos[i], nombre,
                   (unsigned long long)((char*)marcos[i] - (char*)modulo));
    }
    std::fclose(f);
  }
}
inline void SonracInstalarVolcados() {
  _set_invalid_parameter_handler([](const wchar_t*, const wchar_t*, const wchar_t*, unsigned, uintptr_t) {
    SonracVolcarPila("invalid parameter (STL hardening)");
  });
  std::signal(SIGABRT, [](int) { SonracVolcarPila("SIGABRT (abort)"); });
  std::set_terminate([] {
    SonracVolcarPila("std::terminate");
    std::abort();
  });
}
#endif

#if !defined(__ANDROID__)
// PC: the SDK's file log does not come out in the Windows build; this writes every line to
// sonrac_pc.log next to the executable and flushes it, so a fail-fast still leaves the last lines.
class SonracLogPc : public spdlog::sinks::base_sink<std::mutex> {
 public:
  explicit SonracLogPc(const std::filesystem::path& ruta) : f_(std::fopen(ruta.string().c_str(), "w")) {}
  ~SonracLogPc() override {
    if (f_) std::fclose(f_);
  }

 protected:
  void sink_it_(const spdlog::details::log_msg& msg) override {
    if (!f_) return;
    spdlog::memory_buf_t texto;
    formatter_->format(msg, texto);
    std::fwrite(texto.data(), 1, texto.size(), f_);
    std::fflush(f_);
  }
  void flush_() override {
    if (f_) std::fflush(f_);
  }

 private:
  FILE* f_;
};
#endif

class SonicracingtransformedApp : public rex::ReXApp {
 public:
  using rex::ReXApp::ReXApp;

  static std::unique_ptr<rex::ui::WindowedApp> Create(
      rex::ui::WindowedAppContext& ctx) {
    return std::unique_ptr<SonicracingtransformedApp>(new SonicracingtransformedApp(ctx, "sonicracingtransformed",
        PPCImageConfig));
  }

  void OnCreateDialogs(rex::ui::ImGuiDrawer* drawer) override {
#if defined(SONRAC_CONFIG_DIALOG)
    // Register F1 keybind to open/close the Sonic configuration dialog at any time
    auto exe_dir = rex::filesystem::GetExecutableFolder();
    auto config_path = exe_dir / (std::string(GetName()) + ".toml");

    rex::ui::RegisterBind("bind_sonic_config", "F1", "Abrir Menu de Configuracion Sonic", [this, drawer, config_path]() {
      if (config_dialog_) {
        config_dialog_.reset();
      } else {
        config_dialog_ = std::make_unique<SonicConfigDialog>(drawer, config_path, window());
      }
    });

    // Also open on startup so the player can configure resolution, controllers and graphics immediately
    config_dialog_ = std::make_unique<SonicConfigDialog>(drawer, config_path, window());
#else
    (void)drawer;
#endif
  }

#if !defined(__ANDROID__)
  // Desktop testing: SONRAC_GPU_BACKEND=vulkan|d3d12 picks the xenos backend
  // (the plugin defaults to vulkan).
  void OnPreSetup(rex::RuntimeConfig& config) override {
    rex::cvar::SetFlagByName("mnk_controller_emulation", "true");
    config.gpu_plugin = "xenos";
    const char* backend = std::getenv("SONRAC_GPU_BACKEND");
    config.graphics = rex::system::LoadGpuPlugin("xenos", backend ? backend : "vulkan");
  }
#else
  void OnPreSetup(rex::RuntimeConfig& config) override {
    config.gpu_plugin = "xenos";
    config.graphics = rex::system::LoadGpuPlugin("xenos", "vulkan");
  }
#endif

#if defined(__ANDROID__)
  // The runtime's signal handlers are in place by now; ours goes on top so a
  // crash in the game is logged (tag SonRacCrash) before they see it.
  void OnPreLaunchModule() override { sonrac::InstallCrashLogger(); }
#endif

#if !defined(__ANDROID__)
  void OnPostInitLogging() override {
#if defined(_WIN32)
    SonracInstalarVolcados();
#endif
    // The REXLOG lines go to rexruntime's logging registry, but the executable links its own static copy of
    // rex::AddSink: call the DLL's export instead, so the sink lands where the lines go.
    auto sink = std::make_shared<SonracLogPc>(rex::filesystem::GetExecutableFolder() / "sonrac_pc.log");
#if defined(_WIN32)
    using AddSinkDll = void (*)(spdlog::sink_ptr);
    for (const char* dll : {"rexruntimerd.dll", "rexruntime.dll", "rexruntimed.dll"}) {
      if (HMODULE m = GetModuleHandleA(dll)) {
        if (auto f = reinterpret_cast<AddSinkDll>(
                GetProcAddress(m, "?AddSink@rex@@YAXV?$shared_ptr@Vsink@sinks@spdlog@@@std@@@Z"))) {
          f(sink);
          return;
        }
      }
    }
#endif
    rex::AddSink(sink);
  }
#endif

  void OnShutdown() override {
#if defined(SONRAC_CONFIG_DIALOG)
    rex::ui::UnregisterBind("bind_sonic_config");
    config_dialog_.reset();
#endif
  }

#if defined(SONRAC_CONFIG_DIALOG)
 private:
  std::unique_ptr<SonicConfigDialog> config_dialog_;
#endif
};
