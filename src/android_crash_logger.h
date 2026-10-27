// SonRac - Android crash logger
//
// Logs where a SIGSEGV/SIGBUS/SIGILL happens (fault address, pc, lr and a
// frame-pointer backtrace with symbol names) before handing the signal to the
// handlers installed before it (the runtime's guest-memory handler, then
// debuggerd). The runtime's own handlers can die silently on some faults, which
// leaves no crash report at all; this one runs on its own signal stack.

#pragma once

#if defined(__ANDROID__)

#include <android/log.h>
#include <dlfcn.h>
#include <signal.h>
#include <stdint.h>
#include <ucontext.h>
#include <unistd.h>

#include <cstdlib>

namespace sonrac {

inline struct sigaction g_crash_previous[NSIG];
inline int g_crash_reports = 0;

inline void LogFrame(int index, uintptr_t pc) {
  Dl_info info{};
  if (dladdr(reinterpret_cast<void*>(pc), &info) && info.dli_fname) {
    const char* lib = info.dli_fname;
    for (const char* p = info.dli_fname; *p; ++p) {
      if (*p == '/') lib = p + 1;
    }
    uintptr_t base = reinterpret_cast<uintptr_t>(info.dli_fbase);
    if (info.dli_sname) {
      __android_log_print(ANDROID_LOG_FATAL, "SonRacCrash", "  #%02d %s+0x%zx (%s+%zu)", index, lib,
                          static_cast<size_t>(pc - base), info.dli_sname,
                          static_cast<size_t>(pc - reinterpret_cast<uintptr_t>(info.dli_saddr)));
    } else {
      __android_log_print(ANDROID_LOG_FATAL, "SonRacCrash", "  #%02d %s+0x%zx", index, lib,
                          static_cast<size_t>(pc - base));
    }
  } else {
    __android_log_print(ANDROID_LOG_FATAL, "SonRacCrash", "  #%02d 0x%zx", index,
                        static_cast<size_t>(pc));
  }
}

inline void CrashHandler(int sig, siginfo_t* info, void* context) {
  // SEGV_ACCERR on guest memory is the runtime's write watch (texture and
  // buffer invalidation): routine, frequent, handled by the next handler.
  const bool routine = sig == SIGSEGV && info && info->si_code == SEGV_ACCERR;
  if (!routine && g_crash_reports < 8) {
    ++g_crash_reports;
    auto* uc = static_cast<ucontext_t*>(context);
    uintptr_t pc = uc->uc_mcontext.pc;
    uintptr_t lr = uc->uc_mcontext.regs[30];
    uintptr_t fp = uc->uc_mcontext.regs[29];
    uintptr_t sp = uc->uc_mcontext.sp;
    __android_log_print(ANDROID_LOG_FATAL, "SonRacCrash",
                        "signal %d code %d fault addr %p pc 0x%zx lr 0x%zx sp 0x%zx tid %d", sig,
                        info ? info->si_code : 0, info ? info->si_addr : nullptr,
                        static_cast<size_t>(pc), static_cast<size_t>(lr),
                        static_cast<size_t>(sp), gettid());
    LogFrame(0, pc);
    LogFrame(1, lr);
    // Frame-pointer chain: [fp] = previous fp, [fp+8] = return address.
    for (int i = 2; i < 40 && fp && (fp & 7) == 0 && fp > sp && fp - sp < (64u << 20); ++i) {
      uintptr_t next = reinterpret_cast<uintptr_t*>(fp)[0];
      uintptr_t ret = reinterpret_cast<uintptr_t*>(fp)[1];
      if (!ret) break;
      LogFrame(i, ret);
      if (next <= fp) break;
      fp = next;
    }
  }
  const struct sigaction& previous = g_crash_previous[sig];
  if ((previous.sa_flags & SA_SIGINFO) && previous.sa_sigaction) {
    previous.sa_sigaction(sig, info, context);
    return;
  }
  if (previous.sa_handler != SIG_DFL && previous.sa_handler != SIG_IGN && previous.sa_handler) {
    previous.sa_handler(sig);
    return;
  }
  signal(sig, SIG_DFL);
  raise(sig);
}

// Call after the runtime has installed its handlers, so this one runs first.
inline void InstallCrashLogger() {
  // A large alternate stack for this thread; other threads use bionic's.
  static char alt_stack[256 * 1024];
  stack_t ss{};
  ss.ss_sp = alt_stack;
  ss.ss_size = sizeof(alt_stack);
  sigaltstack(&ss, nullptr);

  struct sigaction sa{};
  sa.sa_sigaction = CrashHandler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_SIGINFO | SA_ONSTACK | SA_NODEFER;
  for (int sig : {SIGSEGV, SIGBUS, SIGILL}) {
    sigaction(sig, &sa, &g_crash_previous[sig]);
  }
  __android_log_print(ANDROID_LOG_INFO, "SonRacCrash", "crash logger installed");
}

}  // namespace sonrac

#endif  // __ANDROID__
