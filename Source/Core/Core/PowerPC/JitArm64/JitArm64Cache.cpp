// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/PowerPC/JitArm64/JitArm64Cache.h"
#include "Common/CommonTypes.h"
#include "Core/PowerPC/JitArm64/Jit.h"
#include "Core/PowerPC/JitCommon/JitBase.h"

using namespace Arm64Gen;

JitArm64BlockCache::JitArm64BlockCache(JitBase& jit) : JitBaseBlockCache{jit}
{
}

void JitArm64BlockCache::WriteLinkBlock(Arm64Gen::ARM64XEmitter& emit,
                                        const JitBlock::LinkData& source, const JitBlock* dest)
{
  if (!dest)
  {
    // Use a fixed amount of instructions, so we can assume to use 3 instructions on patching.
    emit.MOVZ(DISPATCHER_PC, source.exitAddress & 0xFFFF, ShiftAmount::Shift0);
    emit.MOVK(DISPATCHER_PC, source.exitAddress >> 16, ShiftAmount::Shift16);

    if (source.call)
      emit.BL(m_jit.GetAsmRoutines()->dispatcher);
    else
      emit.B(m_jit.GetAsmRoutines()->dispatcher);
    return;
  }

  if (source.call)
  {
    // The "fast" BL must be the third instruction. So just use the former two to inline the
    // downcount check here. It's better to do this near jump before the long jump to the other
    // block.
    FixupBranch fast_link = emit.B(CC_GT);
    emit.BL(dest->checkedEntry);
    emit.SetJumpTarget(fast_link);
    emit.BL(dest->normalEntry);
    return;
  }

  // Are we able to jump directly to the normal entry?
  s64 distance = ((s64)dest->normalEntry - (s64)emit.GetCodePtr()) >> 2;
  if (distance >= -0x40000 && distance <= 0x3FFFF)
  {
    emit.B(CC_GT, dest->normalEntry);
    emit.B(dest->checkedEntry);
    emit.BRK(101);
    return;
  }

  FixupBranch fast_link = emit.B(CC_GT);
  emit.B(dest->checkedEntry);
  emit.SetJumpTarget(fast_link);
  emit.B(dest->normalEntry);
}

void JitArm64BlockCache::WriteLinkBlock(const JitBlock::LinkData& source, const JitBlock* dest)
{
  const Common::ScopedJITPageWriteAndNoExecute enable_jit_page_writes;
  u8* location = source.exitPtrs;

#ifdef _BULLETPROOF_JIT
  JitArm64& arm_jit = static_cast<JitArm64&>(m_jit);
  Arm64Gen::ARM64CodeBlock& code_block = static_cast<Arm64Gen::ARM64CodeBlock&>(arm_jit);
  ARM64XEmitter emit(location, code_block.GetBpDifference());
#else
  ARM64XEmitter emit(location);
#endif

  WriteLinkBlock(emit, source, dest);
  emit.FlushIcache();
}

void JitArm64BlockCache::WriteDestroyBlock(const JitBlock& block)
{
  JitArm64& arm_jit = static_cast<JitArm64&>(m_jit);
  
  // Only clear the entry points as we might still be within this block.
#ifdef _BULLETPROOF_JIT
  Arm64Gen::ARM64CodeBlock& code_block = static_cast<Arm64Gen::ARM64CodeBlock&>(arm_jit);
  ARM64XEmitter emit(block.checkedEntry, code_block.GetBpDifference());
#else
  ARM64XEmitter emit(block.checkedEntry);
#endif

  const Common::ScopedJITPageWriteAndNoExecute enable_jit_page_writes;
  while (emit.GetWritableCodePtr() <= block.normalEntry)
    emit.BRK(0x123);

  emit.FlushIcache();
}
