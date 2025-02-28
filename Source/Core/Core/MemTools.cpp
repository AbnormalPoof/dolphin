// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/MemTools.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "Common/Assert.h"
#include "Common/CommonFuncs.h"
#include "Common/CommonTypes.h"
#include "Common/MsgHandler.h"
#include "Common/Thread.h"

#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

#if TARGET_OS_TV
// On tvOS, these functions are marked as unavailable. We can get around this
// by defining their names as something else. After the mach headers are included,
// we undefine them and redefine them as function prototypes.
#define mach_msg_overwrite unavailable_mach_msg_overwrite 
#define thread_set_exception_ports unavailable_thread_set_exception_ports
#endif

#include "Core/MachineContext.h"
#include "Core/PowerPC/JitInterface.h"

#if TARGET_OS_TV
#undef mach_msg_overwrite
#undef thread_set_exception_ports

extern "C"
{
// mach/message.h
mach_msg_return_t mach_msg_overwrite(
	mach_msg_header_t *msg,
	mach_msg_option_t option,
	mach_msg_size_t send_size,
	mach_msg_size_t rcv_size,
	mach_port_name_t rcv_name,
	mach_msg_timeout_t timeout,
	mach_port_name_t notify,
	mach_msg_header_t *rcv_msg,
	mach_msg_size_t rcv_limit);

// mach/thread_act.h 
kern_return_t thread_set_exception_ports
(
	thread_act_t thread,
	exception_mask_t exception_mask,
	mach_port_t new_port,
	exception_behavior_t behavior,
	thread_state_flavor_t new_flavor
);
}
#endif

#if defined(__FreeBSD__) || defined(__NetBSD__)
#include <signal.h>
#endif
#ifndef _WIN32
#include <unistd.h>  // Needed for _POSIX_VERSION
#endif

#if defined(__APPLE__)
#ifdef _M_X86_64
#define THREAD_STATE64_COUNT x86_THREAD_STATE64_COUNT
#define THREAD_STATE64 x86_THREAD_STATE64
#define thread_state64_t x86_thread_state64_t
#elif defined(_M_ARM_64)
#define THREAD_STATE64_COUNT ARM_THREAD_STATE64_COUNT
#define THREAD_STATE64 ARM_THREAD_STATE64
#define thread_state64_t arm_thread_state64_t
#else
#error Unsupported architecture
#endif
#endif

namespace EMM
{
#ifdef _WIN32

static PVOID s_veh_handle;

static LONG NTAPI Handler(PEXCEPTION_POINTERS pPtrs)
{
  switch (pPtrs->ExceptionRecord->ExceptionCode)
  {
  case EXCEPTION_ACCESS_VIOLATION:
  {
    ULONG_PTR access_type = pPtrs->ExceptionRecord->ExceptionInformation[0];
    if (access_type == 8)  // Rule out DEP
    {
      return EXCEPTION_CONTINUE_SEARCH;
    }

    // virtual address of the inaccessible data
    uintptr_t fault_address = (uintptr_t)pPtrs->ExceptionRecord->ExceptionInformation[1];
    SContext* ctx = pPtrs->ContextRecord;

    if (JitInterface::HandleFault(fault_address, ctx))
    {
      return EXCEPTION_CONTINUE_EXECUTION;
    }
    else
    {
      // Let's not prevent debugging.
      return EXCEPTION_CONTINUE_SEARCH;
    }
  }

  case EXCEPTION_STACK_OVERFLOW:
    if (JitInterface::HandleStackFault())
      return EXCEPTION_CONTINUE_EXECUTION;
    else
      return EXCEPTION_CONTINUE_SEARCH;

  case EXCEPTION_ILLEGAL_INSTRUCTION:
    // No SSE support? Or simply bad codegen?
    return EXCEPTION_CONTINUE_SEARCH;

  case EXCEPTION_PRIV_INSTRUCTION:
    // okay, dynarec codegen is obviously broken.
    return EXCEPTION_CONTINUE_SEARCH;

  case EXCEPTION_IN_PAGE_ERROR:
    // okay, something went seriously wrong, out of memory?
    return EXCEPTION_CONTINUE_SEARCH;

  case EXCEPTION_BREAKPOINT:
    // might want to do something fun with this one day?
    return EXCEPTION_CONTINUE_SEARCH;

  default:
    return EXCEPTION_CONTINUE_SEARCH;
  }
}

void InstallExceptionHandler()
{
  ASSERT(!s_veh_handle);
  s_veh_handle = AddVectoredExceptionHandler(TRUE, Handler);
  ASSERT(s_veh_handle);
}

void UninstallExceptionHandler()
{
  ULONG status = RemoveVectoredExceptionHandler(s_veh_handle);
  ASSERT(status);
  if (status)
    s_veh_handle = nullptr;
}

#elif defined(__APPLE__) && !defined(USE_SIGACTION_ON_APPLE)

static void CheckKR(const char* name, kern_return_t kr)
{
  if (kr)
  {
    PanicAlertFmt("{} failed: kr={:x}", name, kr);
  }
}

static void ExceptionThread(mach_port_t port)
{
  Common::SetCurrentThreadName("Mach exception thread");
#pragma pack(4)
  struct
  {
    mach_msg_header_t Head;
    NDR_record_t NDR;
    exception_type_t exception;
    mach_msg_type_number_t codeCnt;
    int64_t code[2];
    int flavor;
    mach_msg_type_number_t old_stateCnt;
    natural_t old_state[THREAD_STATE64_COUNT];
    mach_msg_trailer_t trailer;
  } msg_in;

  struct
  {
    mach_msg_header_t Head;
    NDR_record_t NDR;
    kern_return_t RetCode;
    int flavor;
    mach_msg_type_number_t new_stateCnt;
    natural_t new_state[THREAD_STATE64_COUNT];
  } msg_out;
#pragma pack()
  memset(&msg_in, 0xee, sizeof(msg_in));
  memset(&msg_out, 0xee, sizeof(msg_out));
  mach_msg_size_t send_size = 0;
  mach_msg_option_t option = MACH_RCV_MSG;
  while (true)
  {
    // If this isn't the first run, send the reply message.  Then, receive
    // a message: either a mach_exception_raise_state RPC due to
    // thread_set_exception_ports, or MACH_NOTIFY_NO_SENDERS due to
    // mach_port_request_notification.
    CheckKR("mach_msg_overwrite",
            mach_msg_overwrite(&msg_out.Head, option, send_size, sizeof(msg_in), port,
                               MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL, &msg_in.Head, 0));

    if (msg_in.Head.msgh_id == MACH_NOTIFY_NO_SENDERS)
    {
      // the other thread exited
      mach_port_destroy(mach_task_self(), port);
      return;
    }

    if (msg_in.Head.msgh_id != 2406)
    {
      PanicAlertFmt("unknown message received");
      return;
    }

    if (msg_in.flavor != THREAD_STATE64)
    {
      PanicAlertFmt("unknown flavor {} (expected {})", msg_in.flavor, THREAD_STATE64);
      return;
    }

    thread_state64_t* state = (thread_state64_t*)msg_in.old_state;

    bool ok = JitInterface::HandleFault((uintptr_t)msg_in.code[1], state);

    // Set up the reply.
    msg_out.Head.msgh_bits = MACH_MSGH_BITS(MACH_MSGH_BITS_REMOTE(msg_in.Head.msgh_bits), 0);
    msg_out.Head.msgh_remote_port = msg_in.Head.msgh_remote_port;
    msg_out.Head.msgh_local_port = MACH_PORT_NULL;
    msg_out.Head.msgh_id = msg_in.Head.msgh_id + 100;
    msg_out.NDR = msg_in.NDR;
    if (ok)
    {
      msg_out.RetCode = KERN_SUCCESS;
      msg_out.flavor = THREAD_STATE64;
      msg_out.new_stateCnt = THREAD_STATE64_COUNT;
      memcpy(msg_out.new_state, msg_in.old_state, THREAD_STATE64_COUNT * sizeof(natural_t));
    }
    else
    {
      // Pass the exception to the next handler (debugger or crash).
      msg_out.RetCode = KERN_FAILURE;
      msg_out.flavor = 0;
      msg_out.new_stateCnt = 0;
    }
    msg_out.Head.msgh_size =
        offsetof(__typeof__(msg_out), new_state) + msg_out.new_stateCnt * sizeof(natural_t);

    send_size = msg_out.Head.msgh_size;
    option |= MACH_SEND_MSG;
  }
}

void InstallExceptionHandler()
{
  mach_port_t port;
  CheckKR("mach_port_allocate",
          mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &port));
  std::thread exc_thread(ExceptionThread, port);
  exc_thread.detach();
  // Obtain a send right for thread_set_exception_ports to copy...
  CheckKR("mach_port_insert_right",
          mach_port_insert_right(mach_task_self(), port, port, MACH_MSG_TYPE_MAKE_SEND));
  // Mach tries the following exception ports in order: thread, task, host.
  // Debuggers set the task port, so we grab the thread port.
  CheckKR("thread_set_exception_ports",
          thread_set_exception_ports(mach_thread_self(), EXC_MASK_BAD_ACCESS, port,
                                     EXCEPTION_STATE | MACH_EXCEPTION_CODES, THREAD_STATE64));
  // ...and get rid of our copy so that MACH_NOTIFY_NO_SENDERS works.
  CheckKR("mach_port_mod_refs",
          mach_port_mod_refs(mach_task_self(), port, MACH_PORT_RIGHT_SEND, -1));
  mach_port_t previous;
  CheckKR("mach_port_request_notification",
          mach_port_request_notification(mach_task_self(), port, MACH_NOTIFY_NO_SENDERS, 0, port,
                                         MACH_MSG_TYPE_MAKE_SEND_ONCE, &previous));
}

void UninstallExceptionHandler()
{
}

#elif defined(_POSIX_VERSION) && !defined(_M_GENERIC)

static struct sigaction old_sa_segv;
static struct sigaction old_sa_bus;

static void sigsegv_handler(int sig, siginfo_t* info, void* raw_context)
{
  if (sig != SIGSEGV && sig != SIGBUS)
  {
    // We are not interested in other signals - handle it as usual.
    return;
  }
  ucontext_t* context = (ucontext_t*)raw_context;
  int sicode = info->si_code;
  if (sicode != SEGV_MAPERR && sicode != SEGV_ACCERR)
  {
    // Huh? Return.
    return;
  }
  uintptr_t bad_address = (uintptr_t)info->si_addr;

// Get all the information we can out of the context.
#ifdef __OpenBSD__
  ucontext_t* ctx = context;
#else
  mcontext_t* ctx = &context->uc_mcontext;
#endif
  // assume it's not a write
  if (!JitInterface::HandleFault(bad_address,
#ifdef __APPLE__
                                 *ctx
#else
                                 ctx
#endif
                                 ))
  {
    // retry and crash
    // According to the sigaction man page, if sa_flags "SA_SIGINFO" is set to the sigaction
    // function pointer, otherwise sa_handler contains one of:
    // SIG_DEF: The 'default' action is performed
    // SIG_IGN: The signal is ignored
    // Any other value is a function pointer to a signal handler

    struct sigaction* old_sa;
    if (sig == SIGSEGV)
    {
      old_sa = &old_sa_segv;
    }
    else
    {
      old_sa = &old_sa_bus;
    }

    if (old_sa->sa_flags & SA_SIGINFO)
    {
      old_sa->sa_sigaction(sig, info, raw_context);
      return;
    }
    if (old_sa->sa_handler == SIG_DFL)
    {
      signal(sig, SIG_DFL);
      return;
    }
    if (old_sa->sa_handler == SIG_IGN)
    {
      // Ignore signal
      return;
    }
    old_sa->sa_handler(sig);
  }
}

void InstallExceptionHandler()
{
  stack_t signal_stack;
#ifdef __FreeBSD__
  signal_stack.ss_sp = (char*)malloc(SIGSTKSZ);
#else
  signal_stack.ss_sp = malloc(SIGSTKSZ);
#endif
  signal_stack.ss_size = SIGSTKSZ;
  signal_stack.ss_flags = 0;
  if (sigaltstack(&signal_stack, nullptr))
    PanicAlert("sigaltstack failed");
  struct sigaction sa;
  sa.sa_handler = nullptr;
  sa.sa_sigaction = &sigsegv_handler;
  sa.sa_flags = SA_SIGINFO;
  sigemptyset(&sa.sa_mask);
  sigaction(SIGSEGV, &sa, &old_sa_segv);
#ifdef __APPLE__
  sigaction(SIGBUS, &sa, &old_sa_bus);
#endif
}

void UninstallExceptionHandler()
{
  stack_t signal_stack, old_stack;
  signal_stack.ss_flags = SS_DISABLE;
  if (!sigaltstack(&signal_stack, &old_stack) && !(old_stack.ss_flags & SS_DISABLE))
  {
    free(old_stack.ss_sp);
  }
  sigaction(SIGSEGV, &old_sa_segv, nullptr);
#ifdef __APPLE__
  sigaction(SIGBUS, &old_sa_bus, nullptr);
#endif
}
#else  // _M_GENERIC or unsupported platform

void InstallExceptionHandler()
{
}
void UninstallExceptionHandler()
{
}

#endif

}  // namespace EMM
