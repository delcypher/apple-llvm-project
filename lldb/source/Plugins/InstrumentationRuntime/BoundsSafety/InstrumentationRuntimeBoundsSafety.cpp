//===-- InstrumentationRuntimeMainThreadChecker.cpp -----------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "InstrumentationRuntimeBoundsSafety.h"

#include "Plugins/Process/Utility/HistoryThread.h"
#include "lldb/Breakpoint/StoppointCallbackContext.h"
#include "lldb/Core/Module.h"
#include "lldb/Core/PluginManager.h"
#include "lldb/Symbol/Block.h"
#include "lldb/Symbol/Symbol.h"
#include "lldb/Symbol/SymbolContext.h"
#include "lldb/Symbol/Variable.h"
#include "lldb/Symbol/VariableList.h"
#include "lldb/Target/InstrumentationRuntimeStopInfo.h"
#include "lldb/Target/RegisterContext.h"
#include "lldb/Target/SectionLoadList.h"
#include "lldb/Target/StopInfo.h"
#include "lldb/Target/Target.h"
#include "lldb/Target/Thread.h"
#include "lldb/Utility/RegisterValue.h"
#include "lldb/Utility/RegularExpression.h"

#include <memory>

using namespace lldb;
using namespace lldb_private;

LLDB_PLUGIN_DEFINE(InstrumentationRuntimeBoundsSafety)

#define BOUNDS_SAFETY_SOFT_TRAP_C "__bounds_safety_soft_trap_c"
#define BOUNDS_SAFETY_SOFT_TRAP_S "__bounds_safety_soft_trap_s"

llvm::ArrayRef<const char *> getBoundsSafetySoftTrapRuntimeFuncs() {
  static const char *Funcs[] = {
      BOUNDS_SAFETY_SOFT_TRAP_C,
      BOUNDS_SAFETY_SOFT_TRAP_S,
  };

  return Funcs;
}

class InstrumentationBoundsSafetyStopInfo : public StopInfo {
public:
  ~InstrumentationBoundsSafetyStopInfo() override = default;

  lldb::StopReason GetStopReason() const override {
    return lldb::eStopReasonInstrumentation;
  }

  std::optional<uint32_t>
  GetSuggestedStackFrameIndex(bool inlined_stack) override {
    return m_value;
  }

  const char *GetDescription() override { return m_description.c_str(); }

  bool DoShouldNotify(Event *event_ptr) override { return true; }

  static lldb::StopInfoSP
  CreateInstrumentationBoundsSafetyStopInfo(Thread &thread) {
    return StopInfoSP(new InstrumentationBoundsSafetyStopInfo(thread));
  }

private:
  std::pair<std::string, std::optional<uint32_t>>
  ComputeStopReasonAndSuggestedStackFrameWithDebugInfo(
      lldb::StackFrameSP parent_sf) {
    // First try to use debug info to understand the reason for trapping. The
    // call stack will look something like this:
    //
    // ```
    // frame #0: `__bounds_safety_soft_trap_s(reason="")
    // frame #1: `__clang_trap_msg$Bounds check failed$<reason>'
    // frame #2: `bad_read(index=10)
    // ```
    // ....
    const auto *TrapReasonFuncName = parent_sf->GetFunctionName();

    // FIXME: Taken from `VerboseTrapFrameRecognizer::RecognizeFrame()`. We
    // should factor out this logic into a common utility
    static auto trap_regex = llvm::Regex(
        llvm::formatv("^{0}\\$(.*)\\$(.*)$", "__clang_trap_msg").str());
    llvm::SmallVector<llvm::StringRef, 3> matches;
    std::string regex_err_msg;
    if (!trap_regex.match(TrapReasonFuncName, &matches, &regex_err_msg))
      return {};
    auto category = matches[1];
    auto message = matches[2];

    // TODO: Maybe clang should be putting the Soft prefix here instead of
    // us adding it here?
    std::string stop_reason = "Soft ";
    stop_reason += category.empty() ? "<empty category>" : category.str();
    if (!message.empty()) {
      stop_reason += ": ";
      stop_reason += message.str();
    }
    // Use computed stop-reason and assume the parent frame below is the
    // the place in the user's code where the call to the soft trap runtime
    // originated.
    return std::make_pair(stop_reason, 2);
  }

  std::pair<std::string, std::optional<uint32_t>>
  ComputeStopReasonAndSuggestedStackFrameWithoutDebugInfo(ThreadSP thread_sp) {
    auto softtrap_sf = thread_sp->GetStackFrameAtIndex(0);
    if (!softtrap_sf)
      return {};
    llvm::StringRef TrapReasonFuncName = softtrap_sf->GetFunctionName();
    auto rc = thread_sp->GetRegisterContext();
    if (!rc)
      return {};
    // The ABI of BOUNDS_SAFETY_SOFT_TRAP_C and BOUNDS_SAFETY_SOFT_TRAP_S is
    // that they both have information about the trap reason in their first
    // argument such that the type is a pointer type or smaller which should be
    // in a register.
    auto *arg0_info = rc->GetRegisterInfo(
        lldb::RegisterKind::eRegisterKindGeneric, LLDB_REGNUM_GENERIC_ARG1);
    if (!arg0_info)
      return {};
    RegisterValue reg_value;
    if (!rc->ReadRegister(arg0_info, reg_value))
      return {};
    uint64_t reg_value_as_int = reg_value.GetAsUInt64(UINT64_MAX);
    if (reg_value_as_int == UINT64_MAX)
      return {};
    if (TrapReasonFuncName == BOUNDS_SAFETY_SOFT_TRAP_C) {
      // The first argument to the call is an integer representing the trap
      // reason.
      //
      // TODO: Once `bounds_safety_soft_traps.h` defines the codes we can map
      // these to a human readable description. For now just give up because
      // the value is alway 0 which doesn't tell us anything useful.
      return {};
    }
    if (TrapReasonFuncName == BOUNDS_SAFETY_SOFT_TRAP_S) {
      // The first argument to the call is a pointer to a global C string
      // containing the trap reason.
      std::string out_string;
      Status error_status;
      thread_sp->GetProcess()->ReadCStringFromMemory(reg_value_as_int,
                                                     out_string, error_status);
      if (error_status.Fail())
        return {};
      std::string TrapReason = "Soft Bounds check failed: " + out_string;
      // The frame where the call to the soft trap function happened is used
      // as the suggested frame. This should be frame where the bounds check
      // actually failed.
      return {TrapReason, 1};
    }
    // Failed
    return {};
  }

  std::pair<std::string, std::optional<uint32_t>>
  ComputeStopReasonAndSuggestedStackFrame() {

    ThreadSP thread_sp = GetThread();
    if (!thread_sp)
      return {};

    auto parent_sf = thread_sp->GetStackFrameAtIndex(1);
    if (!parent_sf)
      return {};

    if (parent_sf->HasDebugInformation()) {
      // FIXME: Can we emit a note?
      return ComputeStopReasonAndSuggestedStackFrameWithDebugInfo(parent_sf);
    }

    // If the debug info is missing we can still get some information
    // from the parameter in the soft trap runtime call.
#if 0
    auto softtrap_sf = thread_sp->GetStackFrameAtIndex(0);
    if (!softtrap_sf)
      return std::make_pair("", std::nullopt);

    Status error_stat;
    auto var_list =
        softtrap_sf->GetVariableList(/*get_file_globals=*/false, &error_stat);

    if (error_stat.Fail())
      return std::make_pair("", std::nullopt);

    if (!var_list)
      return std::make_pair("", std::nullopt);
    VariableSP firstArg;
    for (size_t idx = 0; idx < var_list->GetSize(); ++idx) {
      auto v = var_list->GetVariableAtIndex(idx);
      if (!v)
        continue;
      if (v->GetScope() != eValueTypeVariableArgument)
        continue;
      firstArg = v;
      break;
    }
    if (!firstArg)
      std::make_pair("", std::nullopt);
    llvm::errs() << "XXX: found: " << firstArg->GetName() << "\n";
    //softtrap_sf->GetValueForVariableExpressionPath(firstArg->GetName() , lldb::DynamicValueType use_dynamic, uint32_t options, lldb::VariableSP &var_sp, Status &error)
    // TODO: How do we get the value of the argument?
#endif

    return ComputeStopReasonAndSuggestedStackFrameWithoutDebugInfo(thread_sp);
  }

  InstrumentationBoundsSafetyStopInfo(Thread &thread) : StopInfo(thread, 0) {
    // No additional data describing the reason for stopping
    m_extended_info = nullptr;
    m_description = "Soft Bounds check failed: Reason Unknown";

    auto [Description, MaybeSuggestedStackIndex] =
        ComputeStopReasonAndSuggestedStackFrame();
    if (Description.length() > 0)
      m_description = Description;
    if (MaybeSuggestedStackIndex)
      m_value = MaybeSuggestedStackIndex.value();
  }
};

InstrumentationRuntimeBoundsSafety::~InstrumentationRuntimeBoundsSafety() {
  Deactivate();
}

lldb::InstrumentationRuntimeSP
InstrumentationRuntimeBoundsSafety::CreateInstance(
    const lldb::ProcessSP &process_sp) {
  return InstrumentationRuntimeSP(
      new InstrumentationRuntimeBoundsSafety(process_sp));
}

void InstrumentationRuntimeBoundsSafety::Initialize() {
  PluginManager::RegisterPlugin(GetPluginNameStatic(),
                                "BoundsSafety instrumentation runtime plugin.",
                                CreateInstance, GetTypeStatic);
}

void InstrumentationRuntimeBoundsSafety::Terminate() {
  PluginManager::UnregisterPlugin(CreateInstance);
}

lldb::InstrumentationRuntimeType
InstrumentationRuntimeBoundsSafety::GetTypeStatic() {
  return lldb::eInstrumentationRuntimeTypeBoundsSafety;
}

const RegularExpression &
InstrumentationRuntimeBoundsSafety::GetPatternForRuntimeLibrary() {
  static RegularExpression regex;
  return regex;
}

bool InstrumentationRuntimeBoundsSafety::CheckIfRuntimeIsValid(
    const lldb::ModuleSP module_sp) {

  for (const auto *const SoftTrapFunc : getBoundsSafetySoftTrapRuntimeFuncs()) {
    ConstString test_sym(SoftTrapFunc);

    if (module_sp->FindFirstSymbolWithNameAndType(test_sym,
                                                  lldb::eSymbolTypeAny))
      return true;
  }
  return false;
}

bool InstrumentationRuntimeBoundsSafety::NotifyBreakpointHit(
    void *baton, StoppointCallbackContext *context, user_id_t break_id,
    user_id_t break_loc_id) {
  assert(baton && "null baton");
  if (!baton)
    return false; ///< false => resume execution.

  InstrumentationRuntimeBoundsSafety *const instance =
      static_cast<InstrumentationRuntimeBoundsSafety *>(baton);

  ProcessSP process_sp = instance->GetProcessSP();
  ThreadSP thread_sp = context->exe_ctx_ref.GetThreadSP();
  if (!process_sp || !thread_sp ||
      process_sp != context->exe_ctx_ref.GetProcessSP())
    return false;

  if (process_sp->GetModIDRef().IsLastResumeForUserExpression())
    return false;

  thread_sp->SetStopInfo(
      InstrumentationBoundsSafetyStopInfo::
          CreateInstrumentationBoundsSafetyStopInfo(*thread_sp));
  return true;
}

void InstrumentationRuntimeBoundsSafety::Activate() {
  if (IsActive())
    return;

  ProcessSP process_sp = GetProcessSP();
  if (!process_sp)
    return;

  ModuleSP runtime_module_sp = GetRuntimeModuleSP();

  for (const auto *const SoftTrapFunc : getBoundsSafetySoftTrapRuntimeFuncs()) {
    ConstString symbol_name(SoftTrapFunc);
    const Symbol *symbol = runtime_module_sp->FindFirstSymbolWithNameAndType(
        symbol_name, eSymbolTypeCode);

    if (symbol == nullptr)
      continue;

    Target &target = process_sp->GetTarget();
    if (!symbol->ValueIsAddress() || !symbol->GetAddressRef().IsValid())
      continue;

    addr_t symbol_address =
        symbol->GetAddressRef().GetOpcodeLoadAddress(&target);

    if (symbol_address == LLDB_INVALID_ADDRESS)
      continue;

    // FIXME: How would the user remove these breakpoints?
    // If the user can remove them would the breakpoint ID become invalid?
    // user controls prologue skipping here not me.
    Breakpoint *breakpoint =
        process_sp->GetTarget()
            .CreateBreakpoint(symbol_address, /*internal=*/true,
                              /*hardware=*/false)
            .get();
    // sync is early
    // async is later... user callbacks.
    breakpoint->SetCallback(
        InstrumentationRuntimeBoundsSafety::NotifyBreakpointHit, this,
        /*sync=*/false);
    breakpoint->SetBreakpointKind("bounds-safety-soft-trap");

    if (GetBreakpointID() == LLDB_INVALID_BREAK_ID)
      SetBreakpointID(breakpoint->GetID());
    else
      m_breakpoint_id_call_with_str = breakpoint->GetID();

    SetActive(true);
  }
}

void InstrumentationRuntimeBoundsSafety::Deactivate() {
  SetActive(false);
  llvm::SmallVector<lldb::user_id_t, 2> BreakPoints = {
      GetBreakpointID(), m_breakpoint_id_call_with_str};

  for (const auto BID : BreakPoints) {
    if (BID == LLDB_INVALID_BREAK_ID)
      continue;
    if (ProcessSP process_sp = GetProcessSP())
      process_sp->GetTarget().RemoveBreakpointByID(BID);
  }

  SetBreakpointID(LLDB_INVALID_BREAK_ID);
  m_breakpoint_id_call_with_str = LLDB_INVALID_BREAK_ID;
}
