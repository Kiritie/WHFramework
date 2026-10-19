#pragma once

#include "SaveGame/SaveGameModuleTypes.h"
#include "SaveGame/SaveGameGenerationLease.h"

// 一次分阶段恢复持有的不可变文件代及其已应用阶段。
struct WHFRAMEWORK_API FSaveGameLoadSnapshot
{
	FPendingSaveLoadContext Context;
	TSharedPtr<const FSaveGameGenerationReadLease, ESPMode::ThreadSafe> ReadLease;
	EPhase AppliedPhases = EPhase::None;

	bool IsValid() const
	{
		return Context.IsValid() && ReadLease.IsValid();
	}

	bool CanApply(EPhase InPhase) const
	{
		const uint8 Requested = static_cast<uint8>(InPhase);
		const uint8 Applied = static_cast<uint8>(AppliedPhases);
		const uint8 Primary = static_cast<uint8>(EPhase::Primary);
		return IsValid() && Requested != 0 && (Requested & ~static_cast<uint8>(EPhase::All)) == 0 &&
			(Requested & Applied) == 0 && ((Requested & Primary) != 0 || (Applied & Primary) != 0);
	}
};
