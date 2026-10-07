#include "Voxel/Streaming/VoxelFineSpatial.h"

FVoxelGenerationBounds FVoxelFineShardKey::Bounds() const
{
	return {Min, Min + FIntVector(SideCells)};
}

bool FVoxelFineShardKey::Validate(FString& OutError) const
{
	if (SideCells < 1 || SideCells > 64 || !FMath::IsPowerOfTwo(SideCells))
	{
		OutError = TEXT("Fine Shard 边长必须为 1..64 的二次幂");
		return false;
	}
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		if (Min[Axis] % SideCells != 0 || int64(Min[Axis]) + SideCells > MAX_int32)
		{
			OutError = TEXT("Fine Shard 坐标未对齐或越界");
			return false;
		}
	}
	OutError.Reset();
	return true;
}

uint32 GetTypeHash(const FVoxelFineShardKey& InKey)
{
	return HashCombineFast(GetTypeHash(InKey.Min), GetTypeHash(InKey.SideCells));
}

bool FVoxelFineWindowSettings::Validate(const int32 InCoreRadiusCells, const int32 InGuardRadiusCells, FString& OutError) const
{
	if (PreferredShardSideCells < 32 || PreferredShardSideCells > 64 || !FMath::IsPowerOfTwo(PreferredShardSideCells) ||
		ShiftStepCells < PreferredShardSideCells || ShiftStepCells % PreferredShardSideCells != 0 ||
		VerticalShiftStepCells < PreferredShardSideCells || VerticalShiftStepCells % PreferredShardSideCells != 0 ||
		RadiusCells <= 0 || RadiusCells % PreferredShardSideCells != 0 ||
		VerticalHalfExtentCells <= 0 || VerticalHalfExtentCells % PreferredShardSideCells != 0 ||
		GuardMarginCells < 0 || MaximumShardCount <= 0 || InCoreRadiusCells < 0 ||
		InGuardRadiusCells <= InCoreRadiusCells ||
		RadiusCells <= InGuardRadiusCells + 14 + GuardMarginCells + ShiftStepCells ||
		VerticalHalfExtentCells <= InGuardRadiusCells + 14 + GuardMarginCells + VerticalShiftStepCells ||
		!FMath::IsFinite(MinimumPreparationSeconds) || !FMath::IsFinite(MaximumPreparationSeconds) ||
		MinimumPreparationSeconds <= 0.0 || MaximumPreparationSeconds < MinimumPreparationSeconds)
	{
		OutError = TEXT("Fine Buffer、Guard、步长或准备时间配置不合法");
		return false;
	}
	OutError.Reset();
	return true;
}

int64 FVoxelFineSpatial::FloorDivide(const int64 InValue, const int64 InDivisor)
{
	check(InDivisor > 0);
	return InValue / InDivisor - (InValue % InDivisor < 0 ? 1 : 0);
}

bool FVoxelFineSpatial::TrySnap(const FIntVector& InCell, const int32 InSide, FIntVector& OutCell)
{
	if (InSide <= 0)
	{
		return false;
	}
	FIntVector Candidate;
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		const int64 Coordinate = FloorDivide(InCell[Axis], InSide) * InSide;
		if (Coordinate < MIN_int32 || Coordinate > MAX_int32)
		{
			return false;
		}
		Candidate[Axis] = int32(Coordinate);
	}
	OutCell = Candidate;
	return true;
}

bool FVoxelFineSpatial::Intersects(const FVoxelGenerationBounds& InFirst, const FVoxelGenerationBounds& InSecond)
{
	return InFirst.Min.X < InSecond.Max.X && InFirst.Max.X > InSecond.Min.X &&
		InFirst.Min.Y < InSecond.Max.Y && InFirst.Max.Y > InSecond.Min.Y &&
		InFirst.Min.Z < InSecond.Max.Z && InFirst.Max.Z > InSecond.Min.Z;
}

bool FVoxelFineSpatial::ContainsBounds(const FVoxelGenerationBounds& InOuter, const FVoxelGenerationBounds& InInner)
{
	return InOuter.Min.X <= InInner.Min.X && InOuter.Max.X >= InInner.Max.X &&
		InOuter.Min.Y <= InInner.Min.Y && InOuter.Max.Y >= InInner.Max.Y &&
		InOuter.Min.Z <= InInner.Min.Z && InOuter.Max.Z >= InInner.Max.Z;
}

double FVoxelFineSpatial::DistanceToBounds(const FVector& InCell, const FVoxelGenerationBounds& InBounds)
{
	return FMath::Sqrt(FBox(FVector(InBounds.Min), FVector(InBounds.Max)).ComputeSquaredDistanceToPoint(InCell));
}

bool FVoxelFineSpatial::BuildWindow(const FIntVector& InAnchor, const FVoxelFineWindowSettings& InSettings,
	const int32 InWorldMinZ, const int32 InWorldMaxZ, const uint64 InSerial, FVoxelFineWindow& OutWindow, FString& OutError,
	FVoxelFineSpatialCapacity* InCapacity)
{
	const int32 Side = InSettings.PreferredShardSideCells;
	if (InSerial == 0 || Side < 16 || Side > 64 || !FMath::IsPowerOfTwo(Side) ||
		InSettings.RadiusCells <= 0 || InSettings.RadiusCells % Side != 0 ||
		InSettings.VerticalHalfExtentCells <= 0 || InSettings.VerticalHalfExtentCells % Side != 0 ||
		InWorldMinZ >= InWorldMaxZ || InWorldMinZ % 16 != 0 || InWorldMaxZ % 16 != 0)
	{
		OutError = TEXT("Fine Window 的世界边界或尺寸不合法");
		return false;
	}
	FVoxelFineWindow Candidate;
	Candidate.Serial = InSerial;
	if (!TrySnap(InAnchor, Side, Candidate.Anchor))
	{
		OutError = TEXT("Fine Window Anchor 越界");
		return false;
	}
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		const int64 Radius = Axis == 2 ? InSettings.VerticalHalfExtentCells : InSettings.RadiusCells;
		int64 Minimum = int64(Candidate.Anchor[Axis]) - Radius;
		int64 Maximum = int64(Candidate.Anchor[Axis]) + Radius;
		if (Axis == 2)
		{
			Minimum = FMath::Max<int64>(Minimum, InWorldMinZ);
			Maximum = FMath::Min<int64>(Maximum, InWorldMaxZ);
		}
		if (Minimum < MIN_int32 + Side || Maximum > MAX_int32 - Side || Minimum >= Maximum)
		{
			OutError = TEXT("Fine Window 超过有效世界坐标范围");
			return false;
		}
		Candidate.Bounds.Min[Axis] = int32(Minimum);
		Candidate.Bounds.Max[Axis] = int32(Maximum);
	}
	const int64 FirstZ = FloorDivide(Candidate.Bounds.Min.Z, Side) * Side;
	const int64 LastZ = (FloorDivide(int64(Candidate.Bounds.Max.Z) - 1, Side) + 1) * Side;
	const int64 CountX = (int64(Candidate.Bounds.Max.X) - Candidate.Bounds.Min.X) / Side;
	const int64 CountY = (int64(Candidate.Bounds.Max.Y) - Candidate.Bounds.Min.Y) / Side;
	const int64 CountZ = (LastZ - FirstZ) / Side;
	if (CountX <= 0 || CountY <= 0 || CountZ <= 0 || CountX > InSettings.MaximumShardCount ||
		CountY > InSettings.MaximumShardCount / CountX || CountZ > InSettings.MaximumShardCount / (CountX * CountY))
	{
		OutError = TEXT("Fine Window 超出 Shard 元数据预算；不能静默缩小范围");
		return false;
	}
	const int32 Count = int32(CountX * CountY * CountZ);
	if (InCapacity && !InCapacity->GrantSet<FVoxelFineShardKey>(Count, OutError)) return false;
	Candidate.Shards.Reserve(Count);
	for (int64 WorldZ = FirstZ; WorldZ < LastZ; WorldZ += Side)
	{
		for (int64 WorldY = Candidate.Bounds.Min.Y; WorldY < Candidate.Bounds.Max.Y; WorldY += Side)
		{
			for (int64 WorldX = Candidate.Bounds.Min.X; WorldX < Candidate.Bounds.Max.X; WorldX += Side)
			{
				Candidate.Shards.Add({FIntVector(int32(WorldX), int32(WorldY), int32(WorldZ)), Side});
			}
		}
	}
	OutWindow = MoveTemp(Candidate);
	OutError.Reset();
	return true;
}

bool FVoxelFineSpatial::GatherCore(const FIntVector& InCenter, const int32 InRadiusCells, const int32 InVerticalHalfExtentCells,
	const int32 InWorldMinZ, const int32 InWorldMaxZ, TSet<FIntVector>& OutSections, FString& OutError,
	FVoxelFineSpatialCapacity* InCapacity)
{
	if (InRadiusCells < 0 || InVerticalHalfExtentCells <= 0 || InWorldMinZ >= InWorldMaxZ ||
		InWorldMinZ % 16 != 0 || InWorldMaxZ % 16 != 0)
	{
		OutError = TEXT("F0 核心区参数不合法");
		return false;
	}
	FIntVector First;
	FIntVector Last;
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		const int64 Radius = Axis == 2 ? InVerticalHalfExtentCells : InRadiusCells;
		int64 Minimum = int64(InCenter[Axis]) - Radius;
		int64 Maximum = int64(InCenter[Axis]) + Radius;
		if (Axis == 2)
		{
			Minimum = FMath::Max<int64>(Minimum, InWorldMinZ);
			Maximum = FMath::Min<int64>(Maximum, InWorldMaxZ - 1);
		}
		if (Minimum < MIN_int32 + 16 || Maximum > MAX_int32 - 16 || Minimum > Maximum)
		{
			OutError = TEXT("F0 核心区坐标越界");
			return false;
		}
		First[Axis] = int32(FloorDivide(Minimum, 16));
		Last[Axis] = int32(FloorDivide(Maximum, 16));
	}
	const int64 CountX = int64(Last.X) - First.X + 1;
	const int64 CountY = int64(Last.Y) - First.Y + 1;
	const int64 CountZ = int64(Last.Z) - First.Z + 1;
	constexpr int64 MaximumSections = 262144;
	if (CountX > MaximumSections || CountY > MaximumSections / CountX || CountZ > MaximumSections / (CountX * CountY))
	{
		OutError = TEXT("F0 核心区超出保护性枚举上限");
		return false;
	}
	TSet<FIntVector> Candidate;
	const int32 Count = int32(CountX * CountY * CountZ);
	if (InCapacity && !InCapacity->GrantSet<FIntVector>(Count, OutError)) return false;
	Candidate.Reserve(Count);
	const double RadiusSquared = FMath::Square(double(InRadiusCells));
	for (int32 SectionZ = First.Z; SectionZ <= Last.Z; ++SectionZ)
	{
		for (int32 SectionY = First.Y; SectionY <= Last.Y; ++SectionY)
		{
			for (int32 SectionX = First.X; SectionX <= Last.X; ++SectionX)
			{
				const FIntVector Key(SectionX, SectionY, SectionZ);
				const FIntVector Minimum = Key * 16;
				const FIntVector Maximum = Minimum + FIntVector(16);
				const double DeltaX = double(InCenter.X) - FMath::Clamp(double(InCenter.X), double(Minimum.X), double(Maximum.X));
				const double DeltaY = double(InCenter.Y) - FMath::Clamp(double(InCenter.Y), double(Minimum.Y), double(Maximum.Y));
				if (DeltaX * DeltaX + DeltaY * DeltaY <= RadiusSquared &&
					int64(Maximum.Z) > int64(InCenter.Z) - InVerticalHalfExtentCells &&
					int64(Minimum.Z) < int64(InCenter.Z) + InVerticalHalfExtentCells)
				{
					Candidate.Add(Key);
				}
			}
		}
	}
	OutSections = MoveTemp(Candidate);
	OutError.Reset();
	return true;
}

bool FVoxelFineSpatial::GatherSweptGuard(const FVector& InStart, const FVector& InEnd, const int32 InRadiusCells,
	const int32 InWorldMinZ, const int32 InWorldMaxZ, const int32 InMaximumSections, TSet<FIntVector>& OutSections, FString& OutError,
	FVoxelFineSpatialCapacity* InCapacity)
{
	if (InStart.ContainsNaN() || InEnd.ContainsNaN() || InRadiusCells < 0 || InWorldMinZ >= InWorldMaxZ || InMaximumSections <= 0)
	{
		OutError = TEXT("Collision Guard 参数不合法");
		return false;
	}
	const FVector Minimum = InStart.ComponentMin(InEnd) - FVector(InRadiusCells);
	const FVector Maximum = InStart.ComponentMax(InEnd) + FVector(InRadiusCells);
	if (Minimum.GetMin() < MIN_int32 + 32.0 || Maximum.GetMax() > MAX_int32 - 32.0)
	{
		OutError = TEXT("Collision Guard 坐标越界");
		return false;
	}
	const FIntVector First(FMath::FloorToInt(Minimum.X / 16.0), FMath::FloorToInt(Minimum.Y / 16.0),
		FMath::Max(FMath::FloorToInt(Minimum.Z / 16.0), int32(FloorDivide(InWorldMinZ, 16))));
	const FIntVector Last(FMath::FloorToInt(Maximum.X / 16.0), FMath::FloorToInt(Maximum.Y / 16.0),
		FMath::Min(FMath::FloorToInt(Maximum.Z / 16.0), int32(FloorDivide(int64(InWorldMaxZ) - 1, 16))));
	const int64 CountX = int64(Last.X) - First.X + 1;
	const int64 CountY = int64(Last.Y) - First.Y + 1;
	const int64 CountZ = int64(Last.Z) - First.Z + 1;
	if (CountX <= 0 || CountY <= 0 || CountZ <= 0 || CountX > InMaximumSections ||
		CountY > InMaximumSections / CountX || CountZ > InMaximumSections / (CountX * CountY))
	{
		OutError = TEXT("Collision Guard 超出可枚举范围；长距离传送必须走预热协议");
		return false;
	}
	TSet<FIntVector> Candidate;
	const int32 Count = int32(CountX * CountY * CountZ);
	if (InCapacity && !InCapacity->GrantSet<FIntVector>(Count, OutError)) return false;
	Candidate.Reserve(Count);
	const double ConservativeRadius = InRadiusCells + FMath::Sqrt(3.0) * 8.0;
	for (int32 SectionZ = First.Z; SectionZ <= Last.Z; ++SectionZ)
	{
		for (int32 SectionY = First.Y; SectionY <= Last.Y; ++SectionY)
		{
			for (int32 SectionX = First.X; SectionX <= Last.X; ++SectionX)
			{
				const FIntVector Key(SectionX, SectionY, SectionZ);
				const FVector Center = FVector(Key * 16 + FIntVector(8));
				if (FMath::PointDistToSegmentSquared(Center, InStart, InEnd) <= FMath::Square(ConservativeRadius))
				{
					Candidate.Add(Key);
				}
			}
		}
	}
	OutSections = MoveTemp(Candidate);
	OutError.Reset();
	return true;
}

bool FVoxelFineSpatial::GatherAffectedShards(const FVoxelGenerationBounds& InChangedBounds, const int32 InSideCells,
	const int32 InHaloCells, TSet<FVoxelFineShardKey>& OutKeys, FString& OutError)
{
	if (!InChangedBounds.IsValid() || InSideCells < 1 || InSideCells > 64 || !FMath::IsPowerOfTwo(InSideCells) || InHaloCells < 0)
	{
		OutError = TEXT("Fine 编辑失效范围参数不合法");
		return false;
	}
	FIntVector First;
	FIntVector Last;
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		const int64 Minimum = int64(InChangedBounds.Min[Axis]) - InHaloCells;
		const int64 Maximum = int64(InChangedBounds.Max[Axis]) + InHaloCells - 1;
		if (Minimum < MIN_int32 + InSideCells || Maximum > MAX_int32 - InSideCells)
		{
			OutError = TEXT("Fine 编辑失效范围坐标越界");
			return false;
		}
		First[Axis] = int32(FloorDivide(Minimum, InSideCells));
		Last[Axis] = int32(FloorDivide(Maximum, InSideCells));
	}
	const int64 CountX = int64(Last.X) - First.X + 1;
	const int64 CountY = int64(Last.Y) - First.Y + 1;
	const int64 CountZ = int64(Last.Z) - First.Z + 1;
	constexpr int64 MaximumKeys = 262144;
	if (CountX > MaximumKeys || CountY > MaximumKeys / CountX || CountZ > MaximumKeys / (CountX * CountY))
	{
		OutError = TEXT("大范围编辑必须按区段增量处理");
		return false;
	}
	TSet<FVoxelFineShardKey> Candidate;
	for (int32 ShardZ = First.Z; ShardZ <= Last.Z; ++ShardZ)
	{
		for (int32 ShardY = First.Y; ShardY <= Last.Y; ++ShardY)
		{
			for (int32 ShardX = First.X; ShardX <= Last.X; ++ShardX)
			{
				Candidate.Add({FIntVector(ShardX, ShardY, ShardZ) * InSideCells, InSideCells});
			}
		}
	}
	OutKeys = MoveTemp(Candidate);
	OutError.Reset();
	return true;
}

bool FVoxelFineSpatial::SplitShard(const FVoxelFineShardKey& InParent, TArray<FVoxelFineShardKey>& OutChildren, FString& OutError)
{
	if (!InParent.Validate(OutError) || InParent.SideCells <= 1)
	{
		OutError = TEXT("最小采样单元不能继续拆分；必须报告模型复杂度失败");
		return false;
	}
	const int32 ChildSide = InParent.SideCells / 2;
	TArray<FVoxelFineShardKey> Candidate;
	Candidate.Reserve(8);
	for (int32 Child = 0; Child < 8; ++Child)
	{
		const FIntVector Offset(Child & 1, (Child >> 1) & 1, (Child >> 2) & 1);
		Candidate.Add({InParent.Min + Offset * ChildSide, ChildSide});
	}
	OutChildren = MoveTemp(Candidate);
	OutError.Reset();
	return true;
}
