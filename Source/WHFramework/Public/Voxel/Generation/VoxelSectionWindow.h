#pragma once

#include "CoreMinimal.h"

// Section 局部格坐标的半开窗口；输出按 X、Y、Z 顺序紧密排列。
struct WHFRAMEWORK_API FVoxelSectionWindow
{
	FIntVector Min = FIntVector::ZeroValue;
	FIntVector Max = FIntVector(16);

	bool IsValid() const
	{
		return Min.GetMin() >= 0 && Max.GetMax() <= 16 &&
			Min.X < Max.X && Min.Y < Max.Y && Min.Z < Max.Z;
	}

	bool Contains(const FIntVector& InCell) const
	{
		return InCell.X >= Min.X && InCell.X < Max.X && InCell.Y >= Min.Y && InCell.Y < Max.Y &&
			InCell.Z >= Min.Z && InCell.Z < Max.Z;
	}

	int32 Num() const
	{
		if (!IsValid()) return 0;
		const FIntVector Size = Max - Min;
		return Size.X * Size.Y * Size.Z;
	}

	int32 Index(const FIntVector& InCell) const
	{
		check(IsValid() && Contains(InCell));
		const FIntVector Size = Max - Min;
		const FIntVector Local = InCell - Min;
		return Local.X + Size.X * (Local.Y + Size.Y * Local.Z);
	}
};
