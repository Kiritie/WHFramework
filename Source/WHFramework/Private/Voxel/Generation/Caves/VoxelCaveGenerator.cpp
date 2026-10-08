#include "Voxel/Generation/Caves/VoxelCaveGenerator.h"

#include "ProfilingDebugging/CpuProfilerTrace.h"
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

void FVoxelCavePlan::Finalize()
{
	InfluenceBounds.Init();
	SegmentIndicesBySection.Reset();
	ClearVolumeIndicesBySection.Reset();

	for (int32 SegmentIndex = 0;
		SegmentIndex < Segments.Num();
		++SegmentIndex)
	{
		const FVoxelCaveSegment& Segment =
			Segments[SegmentIndex];

		FVoxelGenerationBounds Bounds;
		if (!Segment.IsValid() || !VoxelGeneration::ClipWorldBounds(FInt64Vector(
			FMath::Min<int64>(
				int64(Segment.Start.X),
				Segment.End.X) -
				Segment.Radius,
			FMath::Min<int64>(
				int64(Segment.Start.Y),
				Segment.End.Y) -
				Segment.Radius,
			FMath::Min<int64>(
				int64(Segment.Start.Z),
				Segment.End.Z) -
				Segment.Radius), FInt64Vector(
			FMath::Max<int64>(
				int64(Segment.Start.X),
				Segment.End.X) +
				Segment.Radius + 1,
			FMath::Max<int64>(
				int64(Segment.Start.Y),
				Segment.End.Y) +
				Segment.Radius + 1,
			FMath::Max<int64>(
				int64(Segment.Start.Z),
				Segment.End.Z) +
				Segment.Radius + 1), Bounds))
		{
			continue;
		}

		InfluenceBounds += FVector(Bounds.Min);
		InfluenceBounds += FVector(Bounds.Max);

		const FIntVector MinSection(
			VoxelGeneration::FloorDivide(
				Bounds.Min.X,
				16),
			VoxelGeneration::FloorDivide(
				Bounds.Min.Y,
				16),
			VoxelGeneration::FloorDivide(
				Bounds.Min.Z,
				16));

		const FIntVector MaxSection(
			VoxelGeneration::FloorDivide(
				Bounds.Max.X - 1,
				16),
			VoxelGeneration::FloorDivide(
				Bounds.Max.Y - 1,
				16),
			VoxelGeneration::FloorDivide(
				Bounds.Max.Z - 1,
				16));

		for (int32 Z = MinSection.Z;
			Z <= MaxSection.Z;
			++Z)
		{
			for (int32 Y = MinSection.Y;
				Y <= MaxSection.Y;
				++Y)
			{
				for (int32 X = MinSection.X;
					X <= MaxSection.X;
					++X)
				{
					SegmentIndicesBySection.
						FindOrAdd(
							FIntVector(
								X,
								Y,
								Z)).
						Add(
							SegmentIndex);
				}
			}
		}
	}
	for (int32 VolumeIndex = 0; VolumeIndex < ClearVolumes.Num(); ++VolumeIndex)
	{
		const FVoxelGenerationBounds& Volume = ClearVolumes[VolumeIndex];
		if (!Volume.IsValid())
		{
			continue;
		}
		InfluenceBounds += FVector(Volume.Min);
		InfluenceBounds += FVector(Volume.Max);
		const FIntVector MinSection(
			VoxelGeneration::FloorDivide(Volume.Min.X, 16),
			VoxelGeneration::FloorDivide(Volume.Min.Y, 16),
			VoxelGeneration::FloorDivide(Volume.Min.Z, 16));
		const FIntVector MaxSection(
			VoxelGeneration::FloorDivide(Volume.Max.X - 1, 16),
			VoxelGeneration::FloorDivide(Volume.Max.Y - 1, 16),
			VoxelGeneration::FloorDivide(Volume.Max.Z - 1, 16));
		for (int32 Z = MinSection.Z; Z <= MaxSection.Z; ++Z)
		{
			for (int32 Y = MinSection.Y; Y <= MaxSection.Y; ++Y)
			{
				for (int32 X = MinSection.X; X <= MaxSection.X; ++X)
				{
					ClearVolumeIndicesBySection.FindOrAdd(FIntVector(X, Y, Z)).Add(VolumeIndex);
				}
			}
		}
	}
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
		Segments.GetAllocatedSize() +
		Entrances.GetAllocatedSize() +
		ClearVolumes.GetAllocatedSize() +
		SegmentIndicesBySection.
			GetAllocatedSize() +
		ClearVolumeIndicesBySection.GetAllocatedSize();

	for (const TPair<
		FIntVector,
		TArray<int32>>& Pair :
		SegmentIndicesBySection)
	{
		Bytes +=
			Pair.Value.
				GetAllocatedSize();
	}
	for (const auto& Pair : ClearVolumeIndicesBySection)
	{
		Bytes += Pair.Value.GetAllocatedSize();
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

	Plan.Finalize();

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
		if (!bHasEntrance && EntranceError == TEXT("Canceled"))
		{
			OutError = MoveTemp(EntranceError);
			return false;
		}
		if (bHasEntrance)
		{
			Entrance.Id = VoxelGeneration::MakeStableId(Recipe->Settings.Seed,
				FIntVector(InAnchorGrid.X, InAnchorGrid.Y, 0),
				0x43415645454E5452ull);
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
				InCancel))
			{
				OutError = TEXT("Canceled");
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
	TArray<FVoxelCaveSegment> LocalSegments;
	TArray<FVoxelGenerationBounds> LocalClearVolumes;
	const auto AppendRequiredClearance = [&LocalClearVolumes](const FInt64Vector& Min, const FInt64Vector& Max)
	{
		FVoxelGenerationBounds Bounds;
		if (!VoxelGeneration::ClipWorldBounds(Min, Max, Bounds) ||
			FInt64Vector(Bounds.Min) != Min || FInt64Vector(Bounds.Max) != Max)
		{
			return false;
		}
		LocalClearVolumes.Add(Bounds);
		return true;
	};
	for (int64 Distance = 0; Distance <= Settings.CaveEntranceClearance; ++Distance)
	{
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
			OutError.Reset();
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
		LocalSegments.Add({ Current, Next, Radius });
		if (!AppendRequiredClearance(
			FInt64Vector(int64(FMath::Min(Current.X, Next.X)) - HalfWidth,
				int64(FMath::Min(Current.Y, Next.Y)) - HalfWidth, FMath::Min(Current.Z, Next.Z)),
			FInt64Vector(int64(FMath::Max(Current.X, Next.X)) + HalfWidth + 1,
				int64(FMath::Max(Current.Y, Next.Y)) + HalfWidth + 1, int64(FMath::Max(Current.Z, Next.Z)) + Height)))
		{
			OutError.Reset();
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
	InOutSegments.Append(MoveTemp(LocalSegments));
	InOutClearVolumes.Append(MoveTemp(LocalClearVolumes));
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
