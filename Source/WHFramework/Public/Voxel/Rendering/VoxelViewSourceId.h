#pragma once

#include "CoreMinimal.h"

// 发布源的稳定身份，不持有 UObject；几何、可见性和退休由发布器负责。
struct WHFRAMEWORK_API FVoxelViewSourceId
{
	FGuid Value;

	static FVoxelViewSourceId NewId() { return {FGuid::NewGuid()}; }
	bool IsValid() const { return Value.IsValid(); }
	explicit operator bool() const { return IsValid(); }
	bool operator==(const FVoxelViewSourceId& InOther) const { return Value == InOther.Value; }
	friend uint32 GetTypeHash(const FVoxelViewSourceId& InId) { return GetTypeHash(InId.Value); }
};
