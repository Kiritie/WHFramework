#include "Voxel/Rendering/VoxelTerrainStagePlanner.h"
#include "Voxel/Rendering/VoxelCoverageIndex.h"
#include "Voxel/Rendering/VoxelCoveragePartition.h"

namespace
{
uint8 ResolveWorkBoundedMaximumLevel(const int32 InSampleStepCells, const int32 InMaximumSamplesPerAxis, const uint8 InMaximumLevel)
{
	check(InSampleStepCells > 0);
	check(InMaximumSamplesPerAxis > 0);

	uint8 Result = 0;
	while (Result < InMaximumLevel)
	{
		const int64 NextSide = int64(16) << (Result + 1);
		if (NextSide / InSampleStepCells > InMaximumSamplesPerAxis)
		{
			break;
		}
		++Result;
	}
	return Result;
}

uint8 ResolveLocalPredecessorPreference(
	const FVoxelTerrainSpatialKey& InKey, const int32 InWorldMinZ, const int32 InWorldMaxZ, const FVoxelCoverageIndex* InPredecessorCoverage)
{
	if (!InPredecessorCoverage)
	{
		return 0;
	}

	FIntVector Min;
	FIntVector Max;
	if (!InKey.TryBounds(InWorldMinZ, InWorldMaxZ, Min, Max))
	{
		return 0;
	}

	FIntVector QueryMin = Min - FIntVector(1);
	FIntVector QueryMax = Max + FIntVector(1);
	QueryMin.Z = FMath::Max(QueryMin.Z, InWorldMinZ);
	QueryMax.Z = FMath::Min(QueryMax.Z, InWorldMaxZ);

	FVoxelCoverageLocalSnapshot Local;
	InPredecessorCoverage->Capture(QueryMin, QueryMax, Local);

	uint8 Preferred = 0;
	for (const auto& Record : Local.Records)
	{
		const FIntVector& OtherMin = Record->Min;
		const FIntVector& OtherMax = Record->Max;

		const bool bOverlapX = OtherMax.X > Min.X && OtherMin.X < Max.X;
		const bool bOverlapY = OtherMax.Y > Min.Y && OtherMin.Y < Max.Y;
		const bool bOverlapZ = InKey.IsColumn() || (OtherMax.Z > Min.Z && OtherMin.Z < Max.Z);

		const bool bTouchesX = (OtherMax.X == Min.X || OtherMin.X == Max.X) && bOverlapY && bOverlapZ;
		const bool bTouchesY = (OtherMax.Y == Min.Y || OtherMin.Y == Max.Y) && bOverlapX && bOverlapZ;
		const bool bTouchesZ = !InKey.IsColumn() && (OtherMax.Z == Min.Z || OtherMin.Z == Max.Z) && bOverlapX && bOverlapY;

		if (bTouchesX || bTouchesY || bTouchesZ)
		{
			Preferred = FMath::Max(Preferred, Record->Geometry.Key.Level);
		}
	}

	return Preferred;
}
}

bool FVoxelTerrainStagePlanner::Build(const EVoxelTerrainRepresentation InRepresentation, TConstArrayView<FVoxelTerrainStageSource> InSources,
	const uint8 InMaximumLevel, const int32 InMaximumSamplesPerAxis, const int32 InWorldMinZ, const int32 InWorldMaxZ, const double InTargetPixels,
	const uint64 InGeneratorIdentity, const uint64 InMaterialRevision, FVoxelTerrainStagePlan& OutPlan, FString& OutError,
	const FVoxelCoverageIndex* InPredecessorCoverage, const FVoxelTerrainPageCostLimits& InCostLimits)
{
	if (InRepresentation != EVoxelTerrainRepresentation::VoxelProxy && InRepresentation != EVoxelTerrainRepresentation::Surface &&
		InRepresentation != EVoxelTerrainRepresentation::Macro)
	{
		OutError = TEXT("Stage planner only builds coarse representations; Fine uses complete real Section families");
		return false;
	}

	if (InMaximumLevel > VoxelStreamingGeometry::MaximumSpatialLevel ||
		InMaximumSamplesPerAxis < (InRepresentation == EVoxelTerrainRepresentation::VoxelProxy ? 32 : 16) ||
		InMaximumSamplesPerAxis > (InRepresentation == EVoxelTerrainRepresentation::VoxelProxy ? 128 : 256) || !FMath::IsPowerOfTwo(InMaximumSamplesPerAxis) ||
		(InRepresentation == EVoxelTerrainRepresentation::VoxelProxy && InMaximumLevel < 1) || InCostLimits.Triangles == 0 || InCostLimits.MeshBytes == 0 ||
		!FMath::IsFinite(InCostLimits.BuildMilliseconds) || InCostLimits.BuildMilliseconds <= 0.0 || InWorldMinZ >= InWorldMaxZ ||
		!FMath::IsFinite(InTargetPixels) || InTargetPixels <= 0.0)
	{
		OutError = TEXT("Invalid stage policy");
		return false;
	}

	const bool bColumn = InRepresentation != EVoxelTerrainRepresentation::VoxelProxy;
	const int64 Side = int64(16) << InMaximumLevel;
	TSet<FVoxelTerrainSpatialKey> Roots;

	for (const FVoxelTerrainStageSource& Entry : InSources)
	{
		if (Entry.OuterRadiusCells < 0 || Entry.VerticalHalfExtentCells < 0 || !FMath::IsFinite(Entry.Source.VerticalFovDegrees) ||
			Entry.Source.VerticalFovDegrees <= 0.0 || Entry.Source.VerticalFovDegrees >= 180.0 || Entry.Source.ViewportHeightPixels <= 0)
		{
			OutError = TEXT("Invalid source stage range or projection");
			return false;
		}

		if (Entry.OuterRadiusCells == 0 || (!bColumn && Entry.VerticalHalfExtentCells == 0))
		{
			continue;
		}

		const FIntVector Center = Entry.Source.Center;
		const int64 MinX = VoxelStreamingGeometry::FloorDivide(int64(Center.X) - Entry.OuterRadiusCells, Side);
		const int64 MaxX = VoxelStreamingGeometry::FloorDivide(int64(Center.X) + Entry.OuterRadiusCells, Side);
		const int64 MinY = VoxelStreamingGeometry::FloorDivide(int64(Center.Y) - Entry.OuterRadiusCells, Side);
		const int64 MaxY = VoxelStreamingGeometry::FloorDivide(int64(Center.Y) + Entry.OuterRadiusCells, Side);
		const int64 MinZ =
			bColumn ? 0 : VoxelStreamingGeometry::FloorDivide(FMath::Max<int64>(InWorldMinZ, int64(Center.Z) - Entry.VerticalHalfExtentCells), Side);
		const int64 MaxZ =
			bColumn ? 0 : VoxelStreamingGeometry::FloorDivide(FMath::Min<int64>(int64(InWorldMaxZ) - 1, int64(Center.Z) + Entry.VerticalHalfExtentCells), Side);

		const int64 Width = MaxX - MinX + 1;
		const int64 Height = MaxY - MinY + 1;
		const int64 Depth = MaxZ >= MinZ ? MaxZ - MinZ + 1 : 0;
		constexpr int64 EmergencyNodes = 1 << 20;
		if (Width > EmergencyNodes || Height > EmergencyNodes || Depth > EmergencyNodes || Width * Height > EmergencyNodes ||
			(Depth > 0 && Width * Height > EmergencyNodes / Depth))
		{
			OutError = TEXT("Stage root enumeration exceeds the emergency guard; no reduced-quality plan is substituted");
			return false;
		}

		for (int64 Z = MinZ; Z <= MaxZ; ++Z)
		{
			for (int64 Y = MinY; Y <= MaxY; ++Y)
			{
				for (int64 X = MinX; X <= MaxX; ++X)
				{
					Roots.Add({InRepresentation, FIntVector(static_cast<int32>(X), static_cast<int32>(Y), static_cast<int32>(Z)), InMaximumLevel});

					if (Roots.Num() > (1 << 20))
					{
						OutError = TEXT("Combined stage roots exceed the emergency node guard");
						return false;
					}
				}
			}
		}
	}

	FVoxelTerrainStagePlan Candidate;
	int32 Visited = 0;
	for (const FVoxelTerrainSpatialKey& Root : Roots)
	{
		if (!Visit(Root, InSources, InMaximumLevel, InMaximumSamplesPerAxis, InWorldMinZ, InWorldMaxZ, InTargetPixels, InGeneratorIdentity, InMaterialRevision,
				Candidate, Visited, OutError, InPredecessorCoverage, InCostLimits))
		{
			return false;
		}
	}

	// 保留原有同 Representation 邻居物理平衡，但递归 Visit 新增样本上限参数。
	TArray<FVoxelTerrainSpatialKey> BalanceQueue;
	Candidate.Pages.GenerateKeyArray(BalanceQueue);
	for (int32 Next = 0; Next < BalanceQueue.Num(); ++Next)
	{
		const FVoxelTerrainSpatialKey Key = BalanceQueue[Next];
		if (!Candidate.Pages.Contains(Key))
		{
			continue;
		}

		FIntVector Min;
		FIntVector Max;
		if (!Key.TryBounds(InWorldMinZ, InWorldMaxZ, Min, Max))
		{
			return false;
		}

		for (int32 Axis = 0; Axis < (bColumn ? 2 : 3); ++Axis)
		{
			for (const int32 Sign : {-1, 1})
			{
				FIntVector Point = Min + (Max - Min) / 2;
				Point[Axis] = Sign < 0 ? Min[Axis] - 1 : Max[Axis];

				for (uint8 Level = Key.Level + 2; Level <= InMaximumLevel; ++Level)
				{
					const int64 NeighborSide = int64(16) << Level;
					const FVoxelTerrainSpatialKey Neighbor{InRepresentation,
						FIntVector(int32(VoxelStreamingGeometry::FloorDivide(Point.X, NeighborSide)),
							int32(VoxelStreamingGeometry::FloorDivide(Point.Y, NeighborSide)),
							bColumn ? 0 : int32(VoxelStreamingGeometry::FloorDivide(Point.Z, NeighborSide))),
						Level};

					if (!Candidate.Pages.Contains(Neighbor))
					{
						continue;
					}

					Candidate.Pages.Remove(Neighbor);
					Candidate.Costs.Remove(Neighbor);
					Candidate.Priorities.Remove(Neighbor);
					Candidate.ResolutionFloorPages.Remove(Neighbor);

					for (int32 Child = 0; Child < (bColumn ? 4 : 8); ++Child)
					{
						FVoxelTerrainSpatialKey ChildKey = Neighbor;
						ChildKey.Level = Level - 1;
						ChildKey.Coordinate = Neighbor.Coordinate * 2 + FIntVector(Child & 1, (Child >> 1) & 1, bColumn ? 0 : ((Child >> 2) & 1));

						if (!Visit(ChildKey, InSources, InMaximumLevel, InMaximumSamplesPerAxis, InWorldMinZ, InWorldMaxZ, InTargetPixels, InGeneratorIdentity,
								InMaterialRevision, Candidate, Visited, OutError, InPredecessorCoverage, InCostLimits))
						{
							return false;
						}
						BalanceQueue.Add(ChildKey);
					}

					BalanceQueue.Add(Key);
					break;
				}
			}
		}

		if (BalanceQueue.Num() > (1 << 22))
		{
			OutError = TEXT("Physical balance exceeds the bounded traversal guard");
			return false;
		}
	}

	OutPlan = MoveTemp(Candidate);
	OutError.Reset();
	return true;
}

bool FVoxelTerrainStagePlanner::Visit(const FVoxelTerrainSpatialKey& InKey, TConstArrayView<FVoxelTerrainStageSource> InSources, const uint8 InMaximumLevel,
	const int32 InMaximumSamplesPerAxis, const int32 InWorldMinZ, const int32 InWorldMaxZ, const double InTargetPixels, const uint64 InGeneratorIdentity,
	const uint64 InMaterialRevision, FVoxelTerrainStagePlan& InOutPlan, int32& InOutVisited, FString& OutError,
	const FVoxelCoverageIndex* InPredecessorCoverage, const FVoxelTerrainPageCostLimits& InCostLimits)
{
	if (++InOutVisited > (1 << 20))
	{
		OutError = TEXT("Stage traversal exceeds the emergency node guard");
		return false;
	}

	FIntVector Min;
	FIntVector Max;
	if (!InKey.TryBounds(InWorldMinZ, InWorldMaxZ, Min, Max))
	{
		OutError = TEXT("Stage node overflows the supported stable coordinate grid");
		return false;
	}

	if (!InKey.IsColumn() && (Max.Z <= InWorldMinZ || Min.Z >= InWorldMaxZ))
	{
		return true;
	}

	FVoxelCoverageRegion Required{Min, Max};
	Required.Min.Z = FMath::Max(Required.Min.Z, InWorldMinZ);
	Required.Max.Z = FMath::Min(Required.Max.Z, InWorldMaxZ);

	TArray<FVoxelCoverageRegion> Contributions;
	Contributions.Add(Required);

	if (InPredecessorCoverage)
	{
		FVoxelCoverageLocalSnapshot Local;
		InPredecessorCoverage->Capture(Required.Min, Required.Max, Local);

		for (const auto& Record : Local.Records)
		{
			TArray<FVoxelCoverageRegion> Next;
			for (const FVoxelCoverageRegion& Piece : Contributions)
			{
				FVoxelCoverageEvaluationTask::Subtract(Piece, {Record->Min, Record->Max}, Next);
			}
			Contributions = MoveTemp(Next);

			if (Contributions.IsEmpty())
			{
				return true;
			}
			if (Contributions.Num() > 65536)
			{
				OutError = TEXT("Contribution partition exceeds the bounded region limit");
				return false;
			}
		}
	}

	bool bRelevant = false;
	bool bResolutionFloor = false;
	int32 Sampling = static_cast<int32>(InKey.SideCells());
	int32 Priority = MAX_int32;

	for (const FVoxelTerrainStageSource& Entry : InSources)
	{
		if (Entry.OuterRadiusCells == 0)
		{
			continue;
		}

		const FVoxelStreamingSource& Source = Entry.Source;
		double Distance = TNumericLimits<double>::Max();

		for (const FVoxelCoverageRegion& Piece : Contributions)
		{
			const double NearX = FMath::Clamp(double(Source.Center.X), double(Piece.Min.X), double(Piece.Max.X));
			const double NearY = FMath::Clamp(double(Source.Center.Y), double(Piece.Min.Y), double(Piece.Max.Y));
			const double NearZ = FMath::Clamp(double(Source.Center.Z), double(Piece.Min.Z), double(Piece.Max.Z));

			const double Horizontal = FVector2D::Distance(FVector2D(Source.Center.X, Source.Center.Y), FVector2D(NearX, NearY));

			if (Horizontal > Entry.OuterRadiusCells ||
				(!InKey.IsColumn() && (Entry.VerticalHalfExtentCells == 0 || FMath::Abs(NearZ - Source.Center.Z) > Entry.VerticalHalfExtentCells)))
			{
				continue;
			}

			const double FarX = FMath::Max(FMath::Abs(double(Piece.Min.X) - Source.Center.X), FMath::Abs(double(Piece.Max.X) - Source.Center.X));
			const double FarY = FMath::Max(FMath::Abs(double(Piece.Min.Y) - Source.Center.Y), FMath::Abs(double(Piece.Max.Y) - Source.Center.Y));

			if (Entry.InnerRadiusCells > 0 && FarX * FarX + FarY * FarY <= FMath::Square(double(Entry.InnerRadiusCells)))
			{
				continue;
			}

			Distance = FMath::Min(Distance,
				FMath::Max(double(Entry.InnerRadiusCells), FVector(NearX - Source.Center.X, NearY - Source.Center.Y, NearZ - Source.Center.Z).Size()));
		}

		if (Distance == TNumericLimits<double>::Max())
		{
			continue;
		}

		bRelevant = true;
		Priority = FMath::Min(Priority, Source.GetSchedulingPriority());

		int32 Step = 1;
		bool bFloor = false;
		if (!VoxelStreamingGeometry::ResolveSampleStep(Distance, Source.VerticalFovDegrees, Source.ViewportHeightPixels, InTargetPixels,
				static_cast<int32>(InKey.SideCells()), Step, bFloor, OutError))
		{
			return false;
		}

		// 多 Source 相交时使用最严格（最细）的采样要求。
		Sampling = FMath::Min(Sampling, Step);
		bResolutionFloor |= bFloor;
	}

	if (!bRelevant)
	{
		return true;
	}

	const uint8 MinimumLevel = InKey.IsColumn() ? 0 : 1;
	const int32 PreferredSamples = InKey.IsColumn() ? 64 : 32;
	const uint8 ScreenDesired = VoxelStreamingGeometry::ResolveCoverageLevel(Sampling, PreferredSamples, {MinimumLevel, InMaximumLevel});

	const uint8 WorkMaximum = ResolveWorkBoundedMaximumLevel(Sampling, InMaximumSamplesPerAxis, InMaximumLevel);

	const uint8 LocalPredecessorPreference = ResolveLocalPredecessorPreference(InKey, InWorldMinZ, InWorldMaxZ, InPredecessorCoverage);

	// Predecessor 只提供局部空间连续性偏好；绝不突破工作量硬上限。
	const uint8 PreferredWithPredecessor = FMath::Max(ScreenDesired, FMath::Min(LocalPredecessorPreference, WorkMaximum));
	const uint8 Desired = FMath::Min(PreferredWithPredecessor, WorkMaximum);

	const FVoxelTerrainPageCost Cost = FVoxelTerrainPageCost::Estimate(InKey.SideCells(), Sampling, InKey.IsColumn());
	const bool bCostExceeded = !InCostLimits.Accepts(Cost);
	if (InKey.Level > Desired || (bCostExceeded && InKey.Level > MinimumLevel))
	{
		for (int32 Child = 0; Child < (InKey.IsColumn() ? 4 : 8); ++Child)
		{
			FVoxelTerrainSpatialKey ChildKey = InKey;
			ChildKey.Level = InKey.Level - 1;
			ChildKey.Coordinate = InKey.Coordinate * 2 + FIntVector(Child & 1, (Child >> 1) & 1, InKey.IsColumn() ? 0 : ((Child >> 2) & 1));

			if (!Visit(ChildKey, InSources, InMaximumLevel, InMaximumSamplesPerAxis, InWorldMinZ, InWorldMaxZ, InTargetPixels, InGeneratorIdentity,
					InMaterialRevision, InOutPlan, InOutVisited, OutError, InPredecessorCoverage, InCostLimits))
			{
				return false;
			}
		}
		return true;
	}

	if (bCostExceeded || InKey.Level < MinimumLevel)
	{
		OutError =
			FString::Printf(TEXT("Terrain minimum page blocked by cost policy: representation=%d level=%u samples=%llu triangles=%llu bytes=%llu buildMs=%.3f"),
				int32(InKey.Representation), InKey.Level, Cost.Samples, Cost.Triangles, Cost.MeshBytes, Cost.BuildMilliseconds);
		return false;
	}
	const int64 SamplesPerAxis = InKey.SideCells() / Sampling;
	if (SamplesPerAxis > InMaximumSamplesPerAxis)
	{
		OutError = FString::Printf(TEXT("Terrain page violates the work bound: representation=%d level=%u side=%lld step=%d samples=%lld max=%d"),
			static_cast<int32>(InKey.Representation), InKey.Level, InKey.SideCells(), Sampling, SamplesPerAxis, InMaximumSamplesPerAxis);
		return false;
	}

	FVoxelTerrainGeometry Geometry{InKey, Sampling, InWorldMinZ, InWorldMaxZ, InGeneratorIdentity, InMaterialRevision};

	if (!Geometry.Validate(OutError))
	{
		return false;
	}

	InOutPlan.Pages.Add(InKey, Geometry);
	InOutPlan.Costs.Add(InKey, Cost);
	InOutPlan.Priorities.Add(InKey, Priority);
	if (bResolutionFloor)
	{
		InOutPlan.ResolutionFloorPages.Add(InKey);
	}
	return true;
}

FVoxelTerrainPageCost FVoxelTerrainPageCost::Estimate(const int64 InSideCells, const int32 InStepCells, const bool bColumn)
{
	const uint64 Side = InSideCells / FMath::Max(1, InStepCells);
	FVoxelTerrainPageCost Result;
	if (Side > 65536 || (!bColumn && Side > 65536 / FMath::Max<uint64>(1, Side)))
	{
		Result.Samples = Result.Triangles = Result.MeshBytes = MAX_uint64;
		Result.BuildMilliseconds = MAX_dbl;
		return Result;
	}
	Result.Samples = Side * Side * (bColumn ? 1 : Side);
	Result.Triangles = Result.Samples * (bColumn ? 4 : 12);
	Result.MeshBytes = Result.Triangles * (3 * 80 + 3 * sizeof(uint32));
	Result.BuildMilliseconds = double(Result.Samples) * (bColumn ? 0.0001 : 0.0002);
	return Result;
}

bool FVoxelTerrainPageCostLimits::Accepts(const FVoxelTerrainPageCost& InCost) const
{
	return InCost.Triangles <= Triangles && InCost.MeshBytes <= MeshBytes && InCost.BuildMilliseconds <= BuildMilliseconds;
}
