#include "Voxel/Generation/Caves/VoxelCaveGenerator.h"

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
	SegmentIndicesBySection.Reset();

	for (int32 SegmentIndex = 0;
		SegmentIndex < Segments.Num();
		++SegmentIndex)
	{
		const FVoxelCaveSegment& Segment =
			Segments[SegmentIndex];

		const FIntVector Min(
			FMath::Min(
				Segment.Start.X,
				Segment.End.X) -
				Segment.Radius,
			FMath::Min(
				Segment.Start.Y,
				Segment.End.Y) -
				Segment.Radius,
			FMath::Min(
				Segment.Start.Z,
				Segment.End.Z) -
				Segment.Radius);

		const FIntVector Max(
			FMath::Max(
				Segment.Start.X,
				Segment.End.X) +
				Segment.Radius,
			FMath::Max(
				Segment.Start.Y,
				Segment.End.Y) +
				Segment.Radius,
			FMath::Max(
				Segment.Start.Z,
				Segment.End.Z) +
				Segment.Radius);

		const FIntVector MinSection(
			VoxelGeneration::FloorDivide(
				Min.X,
				16),
			VoxelGeneration::FloorDivide(
				Min.Y,
				16),
			VoxelGeneration::FloorDivide(
				Min.Z,
				16));

		const FIntVector MaxSection(
			VoxelGeneration::FloorDivide(
				Max.X,
				16),
			VoxelGeneration::FloorDivide(
				Max.Y,
				16),
			VoxelGeneration::FloorDivide(
				Max.Z,
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
}

bool FVoxelCavePlan::Carves(
	const FIntVector& InCell) const
{
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
		SegmentIndicesBySection.
			GetAllocatedSize();

	for (const TPair<
		FIntVector,
		TArray<int32>>& Pair :
		SegmentIndicesBySection)
	{
		Bytes +=
			Pair.Value.
				GetAllocatedSize();
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
	if (!InBounds.IsValid())
	{
		OutError =
			TEXT("Voxel cave bounds are invalid");

		return false;
	}

	const int32 Spacing =
		FMath::Max(
			64,
			Recipe->Settings.
				CaveSpacing);

	const int32 Reach =
		MaximumSegments *
			MaximumSegmentLength +
		MaximumRoomRadius +
		8;

	const int32 MinAnchorX =
		VoxelGeneration::FloorDivide(
			InBounds.Min.X -
				Reach,
			Spacing);

	const int32 MaxAnchorX =
		VoxelGeneration::FloorDivide(
			InBounds.Max.X +
				Reach -
				1,
			Spacing);

	const int32 MinAnchorY =
		VoxelGeneration::FloorDivide(
			InBounds.Min.Y -
				Reach,
			Spacing);

	const int32 MaxAnchorY =
		VoxelGeneration::FloorDivide(
			InBounds.Max.Y +
				Reach -
				1,
			Spacing);

	FVoxelCavePlan Plan;

	for (int32 AnchorY = MinAnchorY;
		AnchorY <= MaxAnchorY;
		++AnchorY)
	{
		for (int32 AnchorX = MinAnchorX;
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
						AnchorX,
						AnchorY),
					InColumnSampler,
					Plan.Segments,
					BuildError);

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
	FVoxelCaveColumnSampler InColumnSampler,
	TArray<FVoxelCaveSegment>& OutSegments,
	FString& OutError) const
{
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
		SpawnPermille)
	{
		OutError.Reset();
		return false;
	}

	FRandomStream Stream(
		MakeStreamSeed(
			Seed));

	const int32 StartX =
		InAnchorGrid.X *
			Spacing +
		Stream.RandRange(
			0,
			Spacing - 1);

	const int32 StartY =
		InAnchorGrid.Y *
			Spacing +
		Stream.RandRange(
			0,
			Spacing - 1);

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

	if (StartColumn.SurfaceZ <=
		Recipe->Settings.SeaLevel +
			1)
	{
		OutError.Reset();
		return false;
	}

	const bool bHasEntrance =
		Stream.RandRange(
			0,
			999) <
		EntrancePermille;

	const int32 MinimumWorldZ =
		Recipe->Settings.MinZ +
		3;

	const int32 MinimumSurfaceDepth =
		FMath::Max(
			4,
			FMath::Min(
				Recipe->Settings.
					CaveMinDepth,
				12));

	const int32 UndergroundMaximum =
		FMath::Max(
			MinimumWorldZ + 4,
			StartColumn.SurfaceZ -
				MinimumSurfaceDepth -
				4);

	FIntVector Current(
		StartX,
		StartY,
		bHasEntrance
			? StartColumn.SurfaceZ + 1
			: Stream.RandRange(
				MinimumWorldZ + 3,
				UndergroundMaximum));

	double Yaw =
		Stream.FRandRange(
			-PI,
			PI);

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

		FIntVector End(
			FMath::RoundToInt(
				Current.X +
				Direction.X *
					Length),
			FMath::RoundToInt(
				Current.Y +
				Direction.Y *
					Length),
			FMath::RoundToInt(
				Current.Z +
				Direction.Z *
					Length));

		End =
			ClampBelowSurface(
				End,
				InColumnSampler);

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

		if (SegmentIndex > 2 &&
			Stream.RandRange(
				0,
				999) <
			RoomPermille)
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

		if (SegmentIndex > 3 &&
			Stream.RandRange(
				0,
				999) <
			BranchPermille)
		{
			AddBranch(
				Stream,
				End,
				Yaw,
				Pitch,
				InColumnSampler,
				OutSegments);
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

		if (bHasEntrance &&
			SegmentIndex < 4)
		{
			Pitch =
				FMath::Min(
					Pitch,
					-0.3);
		}

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

void FVoxelCaveGenerator::AddBranch(
	FRandomStream& InStream,
	const FIntVector& InStart,
	const double InYaw,
	const double InPitch,
	FVoxelCaveColumnSampler InColumnSampler,
	TArray<FVoxelCaveSegment>& OutSegments) const
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

		FIntVector End(
			FMath::RoundToInt(
				Current.X +
				Direction.X *
					Length),
			FMath::RoundToInt(
				Current.Y +
					Direction.Y *
					Length),
			FMath::RoundToInt(
				Current.Z +
					Direction.Z *
						Length));

		End =
			ClampBelowSurface(
				End,
				InColumnSampler);

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
}

FIntVector FVoxelCaveGenerator::ClampBelowSurface(
	const FIntVector& InPosition,
	FVoxelCaveColumnSampler InColumnSampler) const
{
	FVoxelColumnSample Column;

	if (!InColumnSampler(
		FIntVector(
			InPosition.X,
			InPosition.Y,
			0),
		Column))
	{
		return InPosition;
	}

	const int32 MinimumSurfaceDepth =
		FMath::Max(
			4,
			FMath::Min(
				Recipe->Settings.
					CaveMinDepth,
				12));

	FIntVector Result =
		InPosition;

	Result.Z =
		FMath::Clamp(
			Result.Z,
			Recipe->Settings.MinZ +
				3,
			Column.SurfaceZ -
				MinimumSurfaceDepth);

	return Result;
}
