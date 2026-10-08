#include "Voxel/Generation/Caves/VoxelCaveGenerator.h"

#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Misc/ScopeExit.h"
#include "Voxel/Generation/VoxelGenerationAllocation.h"
#include "Voxel/Generation/VoxelGenerationMath.h"

namespace
{
	uint32 MakeStreamSeed(
		const uint64 InSeed)
	{
		return static_cast<uint32>(
			InSeed ^
			(InSeed >> 32));
	}
}

namespace
{
	using FCaveSectionIndex = TMap<FIntVector, TArray<int32>>;

	template<typename ElementType>
	bool RetainCaveAppend(TArray<ElementType>& InArray, const int32 InCount, FString& OutError)
	{
		const int64 Capacity = int64(InArray.Num()) + InCount;
		if (Capacity > MAX_int32)
		{
			if (FVoxelGenerationAllocationScope* Scope = FVoxelGenerationAllocationScope::Get())
			{
				return Scope->Fail(TEXT("Oversized: cave output exceeds native slot domain"), OutError);
			}
			OutError = TEXT("Oversized: cave output exceeds native slot domain");
			return false;
		}
		return VoxelGenerationAllocation::RetainArray(InArray, int32(Capacity), OutError);
	}

	void FreeSectionIndex(FCaveSectionIndex& InIndex)
	{
		for (auto& Pair : InIndex)
		{
			VoxelGenerationAllocation::FreeRetainedArray(Pair.Value);
		}
		VoxelGenerationAllocation::FreeRetainedMap(InIndex);
	}

	template<typename VisitorType>
	bool VisitCaveSections(
		const FVoxelGenerationBounds& InBounds,
		VisitorType&& InVisitor,
		FString& OutError,
		const TAtomic<bool>* InCancel)
	{
		if (!InBounds.IsValid())
		{
			return true;
		}
		const FIntVector MinSection(
			VoxelGeneration::FloorDivide(InBounds.Min.X, 16),
			VoxelGeneration::FloorDivide(InBounds.Min.Y, 16),
			VoxelGeneration::FloorDivide(InBounds.Min.Z, 16));
		const FIntVector MaxSection(
			VoxelGeneration::FloorDivide(InBounds.Max.X - 1, 16),
			VoxelGeneration::FloorDivide(InBounds.Max.Y - 1, 16),
			VoxelGeneration::FloorDivide(InBounds.Max.Z - 1, 16));
		uint64 Count = 1;
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			const uint64 Side = uint64(int64(MaxSection[Axis]) - MinSection[Axis] + 1);
			if (Side > uint64(MAX_int32) / Count)
			{
				OutError = TEXT("Oversized: cave section index exceeds native slot domain");
				return false;
			}
			Count *= Side;
		}
		for (int32 Z = MinSection.Z; Z <= MaxSection.Z; ++Z)
		{
			for (int32 Y = MinSection.Y; Y <= MaxSection.Y; ++Y)
			{
				for (int32 X = MinSection.X; X <= MaxSection.X; ++X)
				{
					if (InCancel && InCancel->Load())
					{
						OutError = TEXT("Canceled");
						return false;
					}
					if (!InVisitor(FIntVector(X, Y, Z)))
					{
						return false;
					}
				}
			}
		}
		return true;
	}

	bool CaveSegmentBounds(const FVoxelCaveSegment& InSegment, FVoxelGenerationBounds& OutBounds)
	{
		return InSegment.IsValid() && VoxelGeneration::ClipWorldBounds(
			FInt64Vector(
				FMath::Min<int64>(InSegment.Start.X, InSegment.End.X) - InSegment.Radius,
				FMath::Min<int64>(InSegment.Start.Y, InSegment.End.Y) - InSegment.Radius,
				FMath::Min<int64>(InSegment.Start.Z, InSegment.End.Z) - InSegment.Radius),
			FInt64Vector(
				FMath::Max<int64>(InSegment.Start.X, InSegment.End.X) + InSegment.Radius + 1,
				FMath::Max<int64>(InSegment.Start.Y, InSegment.End.Y) + InSegment.Radius + 1,
				FMath::Max<int64>(InSegment.Start.Z, InSegment.End.Z) + InSegment.Radius + 1),
			OutBounds);
	}
}

bool FVoxelCavePlan::Finalize(FString& OutError, const TAtomic<bool>* InCancel)
{
	if (InCancel && InCancel->Load())
	{
		OutError = TEXT("Canceled");
		return false;
	}
	// Count 只使用原 consumer 的 Working；新旧正文索引在提交前同时持租。
	FVoxelGenerationWorkspace Workspace;
	TMap<FIntVector, int32> SegmentCounts;
	TMap<FIntVector, int32> VolumeCounts;
	FCaveSectionIndex NewSegments;
	FCaveSectionIndex NewVolumes;
	ON_SCOPE_EXIT
	{
		FreeSectionIndex(NewSegments);
		FreeSectionIndex(NewVolumes);
	};
	if (!Workspace.Check(OutError))
	{
		return false;
	}
	const auto CountSection = [&Workspace, &OutError](
		TMap<FIntVector, int32>& InCounts, const FIntVector& InSection)
	{
		if (int32* Count = InCounts.Find(InSection))
		{
			if (*Count == MAX_int32)
			{
				return Workspace.Fail(TEXT("Oversized: cave section references exceed native slot domain"), OutError);
			}
			++*Count;
			return true;
		}
		if (InCounts.Num() == MAX_int32 ||
			!Workspace.ReserveMap(InCounts, InCounts.Num() + 1, OutError))
		{
			if (InCounts.Num() == MAX_int32)
			{
				Workspace.Fail(TEXT("Oversized: cave section count exceeds native slot domain"), OutError);
			}
			return false;
		}
		InCounts.Add(InSection, 1);
		return true;
	};
	FBox NewInfluence(ForceInit);
	for (const FVoxelCaveSegment& Segment : Segments)
	{
		FVoxelGenerationBounds Bounds;
		if (!CaveSegmentBounds(Segment, Bounds))
		{
			continue;
		}
		NewInfluence += FVector(Bounds.Min);
		NewInfluence += FVector(Bounds.Max);
		if (!VisitCaveSections(Bounds, [&](const FIntVector& Section)
			{ return CountSection(SegmentCounts, Section); }, OutError, InCancel))
		{
			return false;
		}
	}
	for (const FVoxelGenerationBounds& Volume : ClearVolumes)
	{
		FVoxelGenerationBounds Bounds;
		if (!VoxelGeneration::ClipWorldBounds(FInt64Vector(Volume.Min), FInt64Vector(Volume.Max), Bounds))
		{
			continue;
		}
		NewInfluence += FVector(Bounds.Min);
		NewInfluence += FVector(Bounds.Max);
		if (!VisitCaveSections(Bounds, [&](const FIntVector& Section)
			{ return CountSection(VolumeCounts, Section); }, OutError, InCancel))
		{
			return false;
		}
	}
	const auto ReserveIndex = [&OutError, InCancel](const TMap<FIntVector, int32>& InCounts, FCaveSectionIndex& OutIndex)
	{
		if (!VoxelGenerationAllocation::RetainMap(OutIndex, InCounts.Num(), OutError))
		{
			return false;
		}
		for (const auto& Pair : InCounts)
		{
			if (InCancel && InCancel->Load())
			{
				OutError = TEXT("Canceled");
				return false;
			}
			TArray<int32>& Indices = OutIndex.Add(Pair.Key);
			if (!VoxelGenerationAllocation::RetainArray(Indices, Pair.Value, OutError))
			{
				return false;
			}
		}
		return true;
	};
	if (!ReserveIndex(SegmentCounts, NewSegments) || !ReserveIndex(VolumeCounts, NewVolumes))
	{
		return false;
	}
	Workspace.FreeMap(SegmentCounts);
	Workspace.FreeMap(VolumeCounts);
	for (int32 Index = 0; Index < Segments.Num(); ++Index)
	{
		FVoxelGenerationBounds Bounds;
		if (CaveSegmentBounds(Segments[Index], Bounds) &&
			!VisitCaveSections(Bounds, [&](const FIntVector& Section)
				{ NewSegments.FindChecked(Section).Add(Index); return true; }, OutError, InCancel))
		{
			return false;
		}
	}
	for (int32 Index = 0; Index < ClearVolumes.Num(); ++Index)
	{
		FVoxelGenerationBounds Bounds;
		if (VoxelGeneration::ClipWorldBounds(FInt64Vector(ClearVolumes[Index].Min), FInt64Vector(ClearVolumes[Index].Max), Bounds) &&
			!VisitCaveSections(Bounds, [&](const FIntVector& Section)
				{ NewVolumes.FindChecked(Section).Add(Index); return true; }, OutError, InCancel))
		{
			return false;
		}
	}
	if (InCancel && InCancel->Load())
	{
		OutError = TEXT("Canceled");
		return false;
	}
	FreeSectionIndex(SegmentIndicesBySection);
	FreeSectionIndex(ClearVolumeIndicesBySection);
	SegmentIndicesBySection = MoveTemp(NewSegments);
	ClearVolumeIndicesBySection = MoveTemp(NewVolumes);
	InfluenceBounds = NewInfluence;
	OutError.Reset();
	return true;
}

#if WITH_DEV_AUTOMATION_TESTS
void FVoxelCavePlan::Finalize()
{
	FString Error;
	const bool bFinalized = Finalize(Error, nullptr);
	checkf(bFinalized, TEXT("%s"), *Error);
}
#endif

void FVoxelCavePlan::ReleaseBacking()
{
	VoxelGenerationAllocation::FreeRetainedArray(Segments);
	VoxelGenerationAllocation::FreeRetainedArray(Entrances);
	VoxelGenerationAllocation::FreeRetainedArray(ClearVolumes);
	FreeSectionIndex(SegmentIndicesBySection);
	FreeSectionIndex(ClearVolumeIndicesBySection);
	InfluenceBounds.Init();
}

bool FVoxelCavePlan::Carves(
	const FIntVector& InCell) const
{
	if (!VoxelGeneration::IsWorldCell(InCell.X) || !VoxelGeneration::IsWorldCell(InCell.Y) ||
		!VoxelGeneration::IsWorldCell(InCell.Z))
	{
		return false;
	}
	const FIntVector Section(
		VoxelGeneration::FloorDivide(
			InCell.X,
			16),
		VoxelGeneration::FloorDivide(
			InCell.Y,
			16),
		VoxelGeneration::FloorDivide(
			InCell.Z,
			16));
	if (const TArray<int32>* Volumes = ClearVolumeIndicesBySection.Find(Section))
	{
		for (const int32 Index : *Volumes)
		{
			if (ClearVolumes.IsValidIndex(Index) && ClearVolumes[Index].Contains(InCell))
			{
				return true;
			}
		}
	}

	const TArray<int32>* SegmentIndices =
		SegmentIndicesBySection.Find(
			Section);

	if (!SegmentIndices)
	{
		return false;
	}

	for (const int32 SegmentIndex :
		*SegmentIndices)
	{
		if (!Segments.IsValidIndex(
			SegmentIndex))
		{
			continue;
		}

		const FVoxelCaveSegment& Segment =
			Segments[SegmentIndex];

		if (VoxelGeneration::IsInsideCapsule(
			InCell,
			Segment.Start,
			Segment.End,
			Segment.Radius))
		{
			return true;
		}
	}

	return false;
}

bool FVoxelCavePlan::ProtectsFloor(
	const FIntVector& InCell) const
{
	(void)InCell;
	return false;
}

uint64 FVoxelCavePlan::GetAllocatedBytes() const
{
	uint64 Bytes =
		VoxelGenerationAllocation::AllocatedBytes(Segments) +
		VoxelGenerationAllocation::AllocatedBytes(Entrances) +
		VoxelGenerationAllocation::AllocatedBytes(ClearVolumes) +
		VoxelGenerationAllocation::AllocatedBytes(SegmentIndicesBySection) +
		VoxelGenerationAllocation::AllocatedBytes(ClearVolumeIndicesBySection);

	for (const TPair<
		FIntVector,
		TArray<int32>>& Pair :
		SegmentIndicesBySection)
	{
		Bytes +=
			VoxelGenerationAllocation::AllocatedBytes(Pair.Value);
	}
	for (const auto& Pair : ClearVolumeIndicesBySection)
	{
		Bytes += VoxelGenerationAllocation::AllocatedBytes(Pair.Value);
	}

	return Bytes;
}

FVoxelCaveGenerator::FVoxelCaveGenerator(
	TSharedRef<const FVoxelGenerationRecipe, ESPMode::ThreadSafe> InRecipe)
	: Recipe(InRecipe)
{
}

bool FVoxelCaveGenerator::BuildPlan(
	const FVoxelGenerationBounds& InBounds,
	FVoxelCaveColumnSampler InColumnSampler,
	FVoxelCavePlan& OutPlan,
	FString& OutError,
	const TAtomic<bool>* InCancel) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_CavePlan);

	if (!InBounds.IsValid())
	{
		OutError = TEXT("Voxel cave owner tile bounds are invalid");

		return false;
	}

	const int32 Spacing =
		FMath::Max(
			64,
			Recipe->Settings.
				CaveSpacing);

	const int64 MinAnchorX =
		VoxelGeneration::FloorDivideWide(
			InBounds.Min.X,
			Spacing) - 1;

	const int64 MaxAnchorX =
		VoxelGeneration::FloorDivideWide(
			InBounds.Max.X - 1,
			Spacing);

	const int64 MinAnchorY =
		VoxelGeneration::FloorDivideWide(
			InBounds.Min.Y,
			Spacing) - 1;

	const int64 MaxAnchorY =
		VoxelGeneration::FloorDivideWide(
			InBounds.Max.Y - 1,
			Spacing);

	FVoxelCavePlan Plan;
	ON_SCOPE_EXIT
	{
		Plan.ReleaseBacking();
	};
	FVoxelGenerationWorkspace Validation;
	if (!Validation.Check(OutError))
	{
		return false;
	}

	for (int64 AnchorY = MinAnchorY;
		AnchorY <= MaxAnchorY;
		++AnchorY)
	{
		for (int64 AnchorX = MinAnchorX;
			AnchorX <= MaxAnchorX;
			++AnchorX)
		{
			if (InCancel &&
				InCancel->Load())
			{
				OutError =
					TEXT("Canceled");

				return false;
			}

			FString BuildError;

			const bool bBuilt =
				TryBuildSystem(
					FIntPoint(
						int32(AnchorX),
						int32(AnchorY)),
					InBounds,
					InColumnSampler,
					Plan.Segments,
					Plan.Entrances,
					Plan.ClearVolumes,
					BuildError,
					InCancel);

			if (!bBuilt &&
				!BuildError.IsEmpty())
			{
				OutError =
					MoveTemp(
						BuildError);

				return false;
			}
		}
	}

	if (!Plan.Finalize(OutError, InCancel))
	{
		return false;
	}

	OutPlan.ReleaseBacking();
	OutPlan =
		MoveTemp(Plan);

	OutError.Reset();
	return true;
}

bool FVoxelCaveGenerator::TryBuildSystem(
	const FIntPoint& InAnchorGrid,
	const FVoxelGenerationBounds& InOwnerBounds,
	FVoxelCaveColumnSampler InColumnSampler,
	TArray<FVoxelCaveSegment>& OutSegments,
	TArray<FVoxelCaveEntrance>& OutEntrances,
	TArray<FVoxelGenerationBounds>& OutClearVolumes,
	FString& OutError,
	const TAtomic<bool>* InCancel) const
{
	if (InCancel && InCancel->Load())
	{
		OutError = TEXT("Canceled");
		return false;
	}
	const int32 Spacing =
		FMath::Max(
			64,
			Recipe->Settings.
				CaveSpacing);

	const uint64 Seed =
		VoxelGeneration::MakeSeed(
			Recipe->Settings.Seed,
			FIntVector(
				InAnchorGrid.X,
				InAnchorGrid.Y,
				0),
			0x4341564553595354ull);

	if (VoxelGeneration::RandomRange(
		Seed,
		0,
		999) >=
		Recipe->Settings.CaveSystemChancePermille)
	{
		OutError.Reset();
		return false;
	}

	FRandomStream Stream(
		MakeStreamSeed(
			Seed));

	const int64 StartXWide =
		int64(InAnchorGrid.X) *
			Spacing +
		Stream.RandRange(
			0,
			Spacing - 1);

	const int64 StartYWide =
		int64(InAnchorGrid.Y) *
			Spacing +
		Stream.RandRange(
			0,
			Spacing - 1);

	FIntPoint StartXY;
	if (!VoxelGeneration::TryWorldPoint(FInt64Vector2(StartXWide, StartYWide), StartXY))
	{
		OutError.Reset();
		return false;
	}
	const int32 StartX = StartXY.X;
	const int32 StartY = StartXY.Y;
	if (StartX < InOwnerBounds.Min.X ||
		StartY < InOwnerBounds.Min.Y ||
		StartX >= InOwnerBounds.Max.X ||
		StartY >= InOwnerBounds.Max.Y)
	{
		OutError.Reset();
		return false;
	}

	FVoxelColumnSample StartColumn;

	if (!InColumnSampler(
		FIntVector(
			StartX,
			StartY,
			0),
		StartColumn))
	{
		OutError.Reset();
		return false;
	}

	if (int64(StartColumn.SurfaceZ) <=
		int64(Recipe->Settings.SeaLevel) +
			1)
	{
		OutError.Reset();
		return false;
	}

	const bool bWantsEntrance =
		!StartColumn.bOcean &&
		!StartColumn.bLake &&
		!StartColumn.bRiver &&
		!StartColumn.bCoast &&
		StartColumn.SlopePermille <= 450 &&
		Stream.RandRange(
			0,
			999) <
		Recipe->Settings.CaveEntranceChancePermille;

	const int64 MinimumWorldZ =
		int64(Recipe->Settings.MinZ) +
		3;

	const int32 MinimumSurfaceDepth =
		FMath::Max(
			4,
			FMath::Min(
				Recipe->Settings.
					CaveMinDepth,
				12));

	const int64 UndergroundMaximum =
		FMath::Max(
			MinimumWorldZ + 4,
			int64(StartColumn.SurfaceZ) -
				MinimumSurfaceDepth -
				4);

	FIntVector Current = FIntVector::ZeroValue;
	double Yaw = 0.0;
	bool bHasEntrance = false;
	if (bWantsEntrance)
	{
		FString EntranceError;
		FVoxelCaveEntrance Entrance;
		bHasEntrance = BuildEntranceCorridor(
			Stream,
			FIntPoint(StartX, StartY),
			StartColumn,
			InColumnSampler,
			OutSegments,
			OutClearVolumes,
			Entrance,
			Current,
			Yaw,
			EntranceError,
			InCancel);
		if (!bHasEntrance && !EntranceError.IsEmpty())
		{
			OutError = MoveTemp(EntranceError);
			return false;
		}
		if (bHasEntrance)
		{
			Entrance.Id = VoxelGeneration::MakeStableId(Recipe->Settings.Seed,
				FIntVector(InAnchorGrid.X, InAnchorGrid.Y, 0),
				0x43415645454E5452ull);
			if (!RetainCaveAppend(OutEntrances, 1, OutError))
			{
				return false;
			}
			OutEntrances.Add(Entrance);
		}
	}
	if (!bHasEntrance)
	{
		const int64 UndergroundMinimum = MinimumWorldZ + 3;
		if (!VoxelGeneration::IsWorldCell(UndergroundMinimum) ||
			!VoxelGeneration::IsWorldCell(UndergroundMaximum) || UndergroundMinimum > UndergroundMaximum)
		{
			OutError.Reset();
			return false;
		}
		Current = FIntVector(
			StartX,
			StartY,
			0);
		const int64 UndergroundRange = UndergroundMaximum - UndergroundMinimum + 1;
		Current.Z = UndergroundRange <= MAX_int32
			? Stream.RandRange(int32(UndergroundMinimum), int32(UndergroundMaximum))
			: int32(UndergroundMinimum + FMath::TruncToInt64(double(Stream.GetFraction()) * UndergroundRange));
		Yaw = Stream.FRandRange(-PI, PI);
	}

	double Pitch =
		bHasEntrance
			? Stream.FRandRange(
				-0.75f,
				-0.45f)
			: Stream.FRandRange(
				-0.2f,
				0.2f);

	const int32 SegmentCount =
		Stream.RandRange(
			MinimumSegments,
			MaximumSegments);

	for (int32 SegmentIndex = 0;
		SegmentIndex < SegmentCount;
		++SegmentIndex)
	{
		if (InCancel && InCancel->Load())
		{
			OutError = TEXT("Canceled");
			return false;
		}
		const int32 Length =
			Stream.RandRange(
				MinimumSegmentLength,
				MaximumSegmentLength);

		const FVector Direction(
			FMath::Cos(Yaw) *
				FMath::Cos(Pitch),
			FMath::Sin(Yaw) *
				FMath::Cos(Pitch),
			FMath::Sin(Pitch));

		FIntVector End;
		if (!VoxelGeneration::TryRoundedWorldCell(FVector(Current) + Direction * Length, End) ||
			!ClampBelowSurface(End, InColumnSampler, End))
		{
			break;
		}

		FVoxelCaveSegment Segment;

		Segment.Start =
			Current;

		Segment.End =
			End;

		Segment.Radius =
			FMath::Max(
				1,
				Recipe->Settings.
					CaveMainRadius +
				Stream.RandRange(
					-1,
					0));

		if (!RetainCaveAppend(OutSegments, 1, OutError))
		{
			return false;
		}
		OutSegments.Add(
			Segment);

		const bool bInterior = IsDeepEnoughForInterior(End, InColumnSampler);

		if (bInterior &&
			Stream.RandRange(
				0,
				999) <
			Recipe->Settings.CaveRoomChancePermille)
		{
			FVoxelCaveSegment Room;

			Room.Start =
				End;

			Room.End =
				End;

			Room.Radius =
				Stream.RandRange(
					FMath::Max(
						Segment.Radius + 1,
						2),
					MaximumRoomRadius);

			if (!RetainCaveAppend(OutSegments, 1, OutError))
			{
				return false;
			}
			OutSegments.Add(
				Room);
		}

		if (bInterior &&
			Stream.RandRange(
				0,
				999) <
			Recipe->Settings.CaveBranchChancePermille)
		{
			if (!AddBranch(
				Stream,
				End,
				Yaw,
				Pitch,
				InColumnSampler,
				OutSegments,
				OutError,
				InCancel))
			{
				return false;
			}
		}

		Current =
			End;

		Yaw +=
			Stream.FRandRange(
				-0.55f,
				0.55f);

		Pitch =
			FMath::Clamp(
				Pitch +
					Stream.FRandRange(
						-0.16f,
						0.16f),
				-0.45,
				0.35);

		if (Current.Z <=
			MinimumWorldZ + 1)
		{
			Pitch =
				FMath::Abs(
					Pitch);
		}
	}

	OutError.Reset();
	return true;
}

bool FVoxelCaveGenerator::AddBranch(
	FRandomStream& InStream,
	const FIntVector& InStart,
	const double InYaw,
	const double InPitch,
	FVoxelCaveColumnSampler InColumnSampler,
	TArray<FVoxelCaveSegment>& OutSegments,
	FString& OutError,
	const TAtomic<bool>* InCancel) const
{
	FIntVector Current =
		InStart;

	double Yaw =
		InYaw +
		InStream.FRandRange(
			0.9f,
			1.8f) *
		(
			InStream.FRand() <
				0.5f
				? -1.0
				: 1.0
		);

	double Pitch =
		FMath::Clamp(
			InPitch +
				InStream.FRandRange(
					-0.15f,
					0.15f),
			-0.35,
			0.3);

	const int32 Count =
		InStream.RandRange(
			3,
			6);

	for (int32 Index = 0;
		Index < Count;
		++Index)
	{
		if (InCancel && InCancel->Load())
		{
			OutError = TEXT("Canceled");
			return false;
		}
		const int32 Length =
			InStream.RandRange(
				MinimumSegmentLength,
				FMath::Max(
					MinimumSegmentLength,
					MaximumSegmentLength -
						1));

		const FVector Direction(
			FMath::Cos(Yaw) *
				FMath::Cos(Pitch),
			FMath::Sin(Yaw) *
				FMath::Cos(Pitch),
			FMath::Sin(Pitch));

		FIntVector End;
		if (!VoxelGeneration::TryRoundedWorldCell(FVector(Current) + Direction * Length, End) ||
			!ClampBelowSurface(End, InColumnSampler, End))
		{
			break;
		}

		FVoxelCaveSegment Segment;

		Segment.Start =
			Current;

		Segment.End =
			End;

		Segment.Radius =
			FMath::Max(
				1,
				Recipe->Settings.
					CaveBranchRadius);

		if (!RetainCaveAppend(OutSegments, 1, OutError))
		{
			return false;
		}
		OutSegments.Add(
			Segment);

		Current =
			End;

		Yaw +=
			InStream.FRandRange(
				-0.45f,
				0.45f);

		Pitch =
			FMath::Clamp(
				Pitch +
					InStream.FRandRange(
						-0.12f,
						0.12f),
				-0.4,
				0.3);
	}

	OutError.Reset();
	return true;
}

bool FVoxelCaveGenerator::BuildEntranceCorridor(
	FRandomStream& InStream,
	const FIntPoint& InStartXY,
	const FVoxelColumnSample& InStartColumn,
	FVoxelCaveColumnSampler InColumnSampler,
	TArray<FVoxelCaveSegment>& InOutSegments,
	TArray<FVoxelGenerationBounds>& InOutClearVolumes,
	FVoxelCaveEntrance& OutEntrance,
	FIntVector& OutEnd,
	double& OutYaw,
	FString& OutError,
	const TAtomic<bool>* InCancel) const
{
	const FVoxelGenerationSettings& Settings = Recipe->Settings;
	const int32 TotalLength = FMath::Max(4, Settings.CaveEntranceLength);
	const int32 Radius = FMath::Max(2, Settings.CaveMainRadius);
	const int64 HalfWidthWide = FMath::Max<int64>(Radius, (int64(Settings.CaveEntranceMinWidth) + 1) / 2);
	const int64 HeightWide = FMath::Max<int64>(Settings.CaveEntranceMinHeight, int64(Radius) * 2);
	if (HalfWidthWide > MAX_int32 || HeightWide > MAX_int32)
	{
		OutError.Reset();
		return false;
	}
	const int32 HalfWidth = int32(HalfWidthWide);
	const int32 Height = int32(HeightWide);
	constexpr int32 HorizontalStep = 4;
	const int32 Steps = int32((int64(TotalLength) + HorizontalStep - 1) / HorizontalStep);
	const int32 RequiredDrop = int32((int64(Radius) + Steps) / Steps);
	const int32 DropPerStep = FMath::Clamp(
		FMath::Max(Settings.CaveEntranceDropPerStep, RequiredDrop), 1, 4);
	OutYaw = InStream.FRandRange(-PI, PI);
	FIntVector Current;
	if (!VoxelGeneration::TryWorldCell(FInt64Vector(InStartXY.X, InStartXY.Y,
		int64(InStartColumn.SurfaceZ) + 1), Current))
	{
		OutError.Reset();
		return false;
	}
	const FIntVector Mouth = Current;
	// 工作区先声明，所有临时 native 容器先析构，再归还同 consumer 的 Working。
	FVoxelGenerationWorkspace Workspace;
	TArray<FVoxelCaveSegment> LocalSegments;
	TArray<FVoxelGenerationBounds> LocalClearVolumes;
	if (!Workspace.Check(OutError))
	{
		return false;
	}
	const auto AppendRequiredClearance = [&LocalClearVolumes, &Workspace, &OutError](const FInt64Vector& Min, const FInt64Vector& Max)
	{
		FVoxelGenerationBounds Bounds;
		if (!VoxelGeneration::ClipWorldBounds(Min, Max, Bounds) ||
			FInt64Vector(Bounds.Min) != Min || FInt64Vector(Bounds.Max) != Max)
		{
			OutError.Reset();
			return false;
		}
		if (LocalClearVolumes.Num() == MAX_int32)
		{
			return Workspace.Fail(TEXT("Oversized: cave clearance exceeds native slot domain"), OutError);
		}
		if (!Workspace.ReserveArray(LocalClearVolumes, LocalClearVolumes.Num() + 1, OutError))
		{
			return false;
		}
		LocalClearVolumes.Add(Bounds);
		return true;
	};
	for (int64 Distance = 0; Distance <= Settings.CaveEntranceClearance; ++Distance)
	{
		if (InCancel && InCancel->Load())
		{
			OutError = TEXT("Canceled");
			return false;
		}
		FIntVector LandingPosition;
		if (!VoxelGeneration::TryRoundedWorldCell(FVector(InStartXY.X - FMath::Cos(OutYaw) * Distance,
			InStartXY.Y - FMath::Sin(OutYaw) * Distance, 0.0), LandingPosition))
		{
			OutError.Reset();
			return false;
		}
		FVoxelColumnSample Landing;
		if (!InColumnSampler(LandingPosition, Landing) ||
			Landing.bOcean || Landing.bLake || Landing.bRiver || Landing.bCoast ||
			Landing.SlopePermille > 450 ||
			FMath::Abs(int64(Landing.SurfaceZ) - InStartColumn.SurfaceZ) > 1)
		{
			OutError.Reset();
			return false;
		}
		if (!AppendRequiredClearance(
			FInt64Vector(int64(LandingPosition.X) - HalfWidth, int64(LandingPosition.Y) - HalfWidth,
				int64(Landing.SurfaceZ) + 1),
			FInt64Vector(int64(LandingPosition.X) + HalfWidth + 1, int64(LandingPosition.Y) + HalfWidth + 1,
				int64(Landing.SurfaceZ) + Height + 1)))
		{
			return false;
		}
	}

	for (int32 Travelled = 0; Travelled < TotalLength;)
	{
		if (InCancel && InCancel->Load())
		{
			OutError = TEXT("Canceled");
			return false;
		}
		const int32 Step = FMath::Min(HorizontalStep, TotalLength - Travelled);
		FIntVector NextXY;
		if (!VoxelGeneration::TryRoundedWorldCell(FVector(Current.X + FMath::Cos(OutYaw) * Step,
			Current.Y + FMath::Sin(OutYaw) * Step, 0.0), NextXY))
		{
			OutError.Reset();
			return false;
		}
		FVoxelColumnSample Column;
		if (!InColumnSampler(NextXY, Column) ||
			Column.bOcean || Column.bLake || Column.bRiver || Column.bCoast ||
			Column.SlopePermille > 450)
		{
			OutError.Reset();
			return false;
		}
		const int64 DesiredZ = int64(Current.Z) - DropPerStep;
		const int64 NextZ = FMath::Min(DesiredZ, int64(Column.SurfaceZ) + 1);
		FIntVector Next;
		if (int64(Current.Z) - NextZ > DropPerStep + 1 ||
			!VoxelGeneration::TryWorldCell(FInt64Vector(NextXY.X, NextXY.Y, NextZ), Next))
		{
			OutError.Reset();
			return false;
		}
		if (!Workspace.ReserveArray(LocalSegments, LocalSegments.Num() + 1, OutError))
		{
			return false;
		}
		LocalSegments.Add({ Current, Next, Radius });
		if (!AppendRequiredClearance(
			FInt64Vector(int64(FMath::Min(Current.X, Next.X)) - HalfWidth,
				int64(FMath::Min(Current.Y, Next.Y)) - HalfWidth, FMath::Min(Current.Z, Next.Z)),
			FInt64Vector(int64(FMath::Max(Current.X, Next.X)) + HalfWidth + 1,
				int64(FMath::Max(Current.Y, Next.Y)) + HalfWidth + 1, int64(FMath::Max(Current.Z, Next.Z)) + Height)))
		{
			return false;
		}
		Current = Next;
		Travelled += Step;
	}

	FVoxelColumnSample EndColumn;
	if (!InColumnSampler(FIntVector(Current.X, Current.Y, 0), EndColumn) ||
		int64(EndColumn.SurfaceZ) - Current.Z < Radius)
	{
		OutError.Reset();
		return false;
	}
	if (int64(InOutSegments.Num()) + LocalSegments.Num() > MAX_int32 ||
		int64(InOutClearVolumes.Num()) + LocalClearVolumes.Num() > MAX_int32)
	{
		return Workspace.Fail(TEXT("Oversized: cave entrance output exceeds native slot domain"), OutError);
	}
	if (!RetainCaveAppend(InOutSegments, LocalSegments.Num(), OutError) ||
		!RetainCaveAppend(InOutClearVolumes, LocalClearVolumes.Num(), OutError))
	{
		return false;
	}
	// scratch 与新正文复制峰值同时授予，Append 不采用可能重分配的 copy assignment。
	InOutSegments.Append(LocalSegments);
	InOutClearVolumes.Append(LocalClearVolumes);
	OutEntrance.MouthCenter = Mouth;
	OutEntrance.Facing = FVector2D(FMath::Cos(OutYaw), FMath::Sin(OutYaw));
	OutEntrance.HalfWidth = HalfWidth;
	OutEntrance.Height = Height;
	OutEntrance.Clearance = Settings.CaveEntranceClearance;
	OutEntrance.InteriorJoin = Current;
	OutEnd = Current;
	OutError.Reset();
	return true;
}

bool FVoxelCaveGenerator::IsDeepEnoughForInterior(
	const FIntVector& InPosition,
	FVoxelCaveColumnSampler InColumnSampler) const
{
	FVoxelColumnSample Column;
	return InColumnSampler(FIntVector(InPosition.X, InPosition.Y, 0), Column) &&
		int64(Column.SurfaceZ) - InPosition.Z >=
		FMath::Max(4, Recipe->Settings.CaveEntranceTransitionDepth);
}

bool FVoxelCaveGenerator::ClampBelowSurface(
	const FIntVector& InPosition,
	FVoxelCaveColumnSampler InColumnSampler,
	FIntVector& OutPosition) const
{
	FVoxelColumnSample Column;

	if (!InColumnSampler(
		FIntVector(
			InPosition.X,
			InPosition.Y,
			0),
		Column))
	{
		return false;
	}

	const int32 MinimumSurfaceDepth =
		FMath::Max(
			4,
			Recipe->Settings.CaveMinDepth);

	const int64 MinimumZ = int64(Recipe->Settings.MinZ) + 3;
	const int64 MaximumZ = int64(Column.SurfaceZ) - MinimumSurfaceDepth;
	if (MinimumZ > MaximumZ)
	{
		return false;
	}
	return VoxelGeneration::TryWorldCell(FInt64Vector(InPosition.X, InPosition.Y,
		FMath::Clamp<int64>(InPosition.Z, MinimumZ, MaximumZ)), OutPosition);
}
