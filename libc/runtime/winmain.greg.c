/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8 -*-│
│vi: set net ft=c ts=2 sts=2 sw=2 fenc=utf-8                                :vi│
╞══════════════════════════════════════════════════════════════════════════════╡
│ Copyright 2020 Justine Alexandra Roberts Tunney                              │
│                                                                              │
│ Permission to use, copy, modify, and/or distribute this software for         │
│ any purpose with or without fee is hereby granted, provided that the         │
│ above copyright notice and this permission notice appear in all copies.      │
│                                                                              │
│ THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL                │
│ WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED                │
│ WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE             │
│ AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL         │
│ DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR        │
│ PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER               │
│ TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR             │
│ PERFORMANCE OF THIS SOFTWARE.                                                │
╚─────────────────────────────────────────────────────────────────────────────*/
#include "libc/assert.h"
#include "libc/calls/internal.h"
#include "libc/calls/state.internal.h"
#include "libc/calls/syscall_support-nt.internal.h"
#include "libc/dce.h"
#include "libc/intrin/asan.internal.h"
#include "libc/intrin/asancodes.h"
#include "libc/intrin/describeflags.internal.h"
#include "libc/intrin/getenv.internal.h"
#include "libc/intrin/weaken.h"
#include "libc/log/libfatal.internal.h"
#include "libc/macros.internal.h"
#include "libc/nexgen32e/rdtsc.h"
#include "libc/nt/console.h"
#include "libc/nt/enum/accessmask.h"
#include "libc/nt/enum/consolemodeflags.h"
#include "libc/nt/enum/creationdisposition.h"
#include "libc/nt/enum/fileflagandattributes.h"
#include "libc/nt/enum/filemapflags.h"
#include "libc/nt/enum/filesharemode.h"
#include "libc/nt/enum/pageflags.h"
#include "libc/nt/files.h"
#include "libc/nt/ipc.h"
#include "libc/nt/memory.h"
#include "libc/nt/pedef.internal.h"
#include "libc/nt/process.h"
#include "libc/nt/runtime.h"
#include "libc/nt/signals.h"
#include "libc/nt/struct/ntexceptionpointers.h"
#include "libc/nt/struct/teb.h"
#include "libc/nt/synchronization.h"
#include "libc/nt/thread.h"
#include "libc/nt/thunk/msabi.h"
#include "libc/runtime/internal.h"
#include "libc/runtime/memtrack.internal.h"
#include "libc/runtime/runtime.h"
#include "libc/runtime/stack.h"
#include "libc/runtime/winargs.internal.h"
#include "libc/sock/internal.h"
#include "libc/str/str.h"
#include "libc/sysv/consts/prot.h"

#ifdef __x86_64__

// clang-format off
__msabi extern typeof(CreateFileMapping) *const __imp_CreateFileMappingW;
__msabi extern typeof(DuplicateHandle) *const __imp_DuplicateHandle;
__msabi extern typeof(ExitProcess) *const __imp_ExitProcess;
__msabi extern typeof(FreeEnvironmentStrings) *const __imp_FreeEnvironmentStringsW;
__msabi extern typeof(GetConsoleMode) *const __imp_GetConsoleMode;
__msabi extern typeof(GetCurrentProcess) *const __imp_GetCurrentProcess;
__msabi extern typeof(GetCurrentProcessId) *const __imp_GetCurrentProcessId;
__msabi extern typeof(GetEnvironmentStrings) *const __imp_GetEnvironmentStringsW;
__msabi extern typeof(GetFileAttributes) *const __imp_GetFileAttributesW;
__msabi extern typeof(GetStdHandle) *const __imp_GetStdHandle;
__msabi extern typeof(MapViewOfFileEx) *const __imp_MapViewOfFileEx;
__msabi extern typeof(SetConsoleCP) *const __imp_SetConsoleCP;
__msabi extern typeof(SetConsoleMode) *const __imp_SetConsoleMode;
__msabi extern typeof(SetConsoleOutputCP) *const __imp_SetConsoleOutputCP;
__msabi extern typeof(SetStdHandle) *const __imp_SetStdHandle;
__msabi extern typeof(VirtualProtect) *const __imp_VirtualProtect;
// clang-format on

void cosmo(int, char **, char **, long (*)[2]) wontreturn;
void __switch_stacks(int, char **, char **, long (*)[2],
                     void (*)(int, char **, char **, long (*)[2]),
                     intptr_t) wontreturn;

static const signed char kNtStdio[3] = {
    (signed char)kNtStdInputHandle,
    (signed char)kNtStdOutputHandle,
    (signed char)kNtStdErrorHandle,
};

forceinline int IsAlpha(int c) {
  return ('A' <= c && c <= 'Z') || ('a' <= c && c <= 'z');
}

// implements all win32 apis on non-windows hosts
__msabi long __oops_win32(void) {
  assert(!"win32 api called on non-windows host");
  return 0;
}

// https://nullprogram.com/blog/2022/02/18/
__msabi static inline char16_t *MyCommandLine(void) {
  void *cmd;
  asm("mov\t%%gs:(0x60),%0\n"
      "mov\t0x20(%0),%0\n"
      "mov\t0x78(%0),%0\n"
      : "=r"(cmd));
  return cmd;
}

// returns true if utf-8 path is a win32-style path that exists
__msabi static textwindows bool32 WinFileExists(const char *path) {
  uint16_t path16[PATH_MAX];
  size_t z = ARRAYLEN(path16);
  size_t n = tprecode8to16(path16, z, path).ax;
  if (n >= z - 1) return false;
  return __imp_GetFileAttributesW(path16) != -1u;
}

// this ensures close(1) won't accidentally close(2) for example
__msabi static textwindows void DeduplicateStdioHandles(void) {
  for (long i = 0; i < 3; ++i) {
    int64_t h1 = __imp_GetStdHandle(kNtStdio[i]);
    for (long j = i + 1; j < 3; ++j) {
      int64_t h2 = __imp_GetStdHandle(kNtStdio[j]);
      if (h1 == h2) {
        int64_t h3, proc = __imp_GetCurrentProcess();
        __imp_DuplicateHandle(proc, h2, proc, &h3, 0, true,
                              kNtDuplicateSameAccess);
        __imp_SetStdHandle(kNtStdio[j], h3);
      }
    }
  }
}

// main function of windows init process
// i.e. first process spawned that isn't forked
__msabi static textwindows wontreturn void WinInit(const char16_t *cmdline) {
  __oldstack = (intptr_t)__builtin_frame_address(0);

  // make console into utf-8 ansi/xterm style tty
  if (NtGetPeb()->OSMajorVersion >= 10 &&
      (intptr_t)v_ntsubsystem == kNtImageSubsystemWindowsCui) {
    __imp_SetConsoleCP(kNtCpUtf8);
    __imp_SetConsoleOutputCP(kNtCpUtf8);
    for (int i = 1; i <= 2; ++i) {
      uint32_t m;
      intptr_t h = __imp_GetStdHandle(kNtStdio[i]);
      __imp_GetConsoleMode(h, &m);
      __imp_SetConsoleMode(h, m | kNtEnableVirtualTerminalProcessing);
    }
  }

  // allocate memory for stack and argument block
  _Static_assert(sizeof(struct WinArgs) % FRAMESIZE == 0, "");
  _mmi.p = _mmi.s;
  _mmi.n = ARRAYLEN(_mmi.s);
  uintptr_t stackaddr = GetStaticStackAddr(0);
  size_t stacksize = GetStaticStackSize();
  __imp_MapViewOfFileEx((_mmi.p[0].h = __imp_CreateFileMappingW(
                             -1, &kNtIsInheritable, kNtPageExecuteReadwrite,
                             stacksize >> 32, stacksize, NULL)),
                        kNtFileMapWrite | kNtFileMapExecute, 0, 0, stacksize,
                        (void *)stackaddr);
  int prot = (intptr_t)ape_stack_prot;
  if (~prot & PROT_EXEC) {
    uint32_t old;
    __imp_VirtualProtect((void *)stackaddr, stacksize, kNtPageReadwrite, &old);
  }
  _mmi.p[0].x = stackaddr >> 16;
  _mmi.p[0].y = (stackaddr >> 16) + ((stacksize - 1) >> 16);
  _mmi.p[0].prot = prot;
  _mmi.p[0].flags = 0x00000026;  // stack+anonymous
  _mmi.p[0].size = stacksize;
  _mmi.i = 1;
  struct WinArgs *wa =
      (struct WinArgs *)(stackaddr + (stacksize - sizeof(struct WinArgs)));

  // allocate asan memory if needed
  if (IsAsan()) {
    uintptr_t shadowaddr = 0x7fff8000 + (stackaddr >> 3);
    uintptr_t shadowend = 0x7fff8000 + ((stackaddr + stacksize) >> 3);
    uintptr_t shallocaddr = ROUNDDOWN(shadowaddr, FRAMESIZE);
    uintptr_t shallocend = ROUNDUP(shadowend, FRAMESIZE);
    uintptr_t shallocsize = shallocend - shallocaddr;
    __imp_MapViewOfFileEx(
        (_mmi.p[1].h =
             __imp_CreateFileMappingW(-1, &kNtIsInheritable, kNtPageReadwrite,
                                      shallocsize >> 32, shallocsize, NULL)),
        kNtFileMapWrite, 0, 0, shallocsize, (void *)shallocaddr);
    _mmi.p[1].x = shallocaddr >> 16;
    _mmi.p[1].y = (shallocaddr >> 16) + ((shallocsize - 1) >> 16);
    _mmi.p[1].prot = PROT_READ | PROT_WRITE;
    _mmi.p[1].flags = 0x00000022;  // private+anonymous
    _mmi.p[1].size = shallocsize;
    _mmi.i = 2;
    __asan_poison((void *)stackaddr, GetGuardSize(), kAsanStackOverflow);
  }

  // parse utf-16 command into utf-8 argv array in argument block
  int count = GetDosArgv(cmdline, wa->argblock, ARRAYLEN(wa->argblock),
                         wa->argv, ARRAYLEN(wa->argv));

  // munge argv so dos paths become cosmo paths
  for (int i = 0; wa->argv[i]; ++i) {
    if (wa->argv[i][0] == '\\' &&  //
        wa->argv[i][1] == '\\') {
      // don't munge new technology style paths
      continue;
    }
    if (!WinFileExists(wa->argv[i])) {
      // don't munge if we're not certain it's a file
      continue;
    }
    // use forward slashes
    for (int j = 0; wa->argv[i][j]; ++j) {
      if (wa->argv[i][j] == '\\') {
        wa->argv[i][j] = '/';
      }
    }
    // turn c:/... into /c/...
    if (IsAlpha(wa->argv[i][0]) &&  //
        wa->argv[i][1] == ':' &&    //
        wa->argv[i][2] == '/') {
      wa->argv[i][1] = wa->argv[i][0];
      wa->argv[i][0] = '/';
    }
  }

  // translate utf-16 win32 environment into utf-8 environment variables
  char16_t *env16 = __imp_GetEnvironmentStringsW();
  GetDosEnviron(env16, wa->envblock, ARRAYLEN(wa->envblock) - 8, wa->envp,
                ARRAYLEN(wa->envp) - 1);
  __imp_FreeEnvironmentStringsW(env16);
  __envp = &wa->envp[0];

  // handover control to cosmopolitan runtime
  __switch_stacks(count, wa->argv, wa->envp, wa->auxv, cosmo,
                  stackaddr + (stacksize - sizeof(struct WinArgs)));
}

__msabi textwindows int64_t WinMain(int64_t hInstance, int64_t hPrevInstance,
                                    const char *lpCmdLine, int64_t nCmdShow) {
  const char16_t *cmdline;
  extern char os asm("__hostos");
  os = _HOSTWINDOWS;  // madness https://news.ycombinator.com/item?id=21019722
  kStartTsc = rdtsc();
  __umask = 077;
  __pid = __imp_GetCurrentProcessId();
  cmdline = MyCommandLine();
#ifdef SYSDEBUG
  // sloppy flag-only check for early initialization
  if (__strstr16(cmdline, u"--strace")) ++__strace;
#endif
  if (_weaken(WinSockInit)) {
    _weaken(WinSockInit)();
  }
  DeduplicateStdioHandles();
  if (_weaken(WinMainStdin)) {
    _weaken(WinMainStdin)();
  }
  if (_weaken(WinMainForked)) {
    _weaken(WinMainForked)();
  }
  WinInit(cmdline);
}

#endif /* __x86_64__ */
