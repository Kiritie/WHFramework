
#include "Voxel/Generators/VoxelCaveGenerator.h"

#include "Math/MathHelper.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/Chunks/VoxelChunk.h"

UVoxelCaveGenerator::UVoxelCaveGenerator()
{
	Seed = 739391;
	SystemSpacingChunks = 5;
	SystemSpawnRate = 0.65f;
	MinSegments = 14;
	MaxSegments = 28;
	MinSegmentLength = 3.f;
	MaxSegmentLength = 6.f;
	MinTunnelRadius = 1.25f;
	MaxTunnelRadius = 2.2f;
	EntranceRate = 0.82f;
	BranchRate = 0.12f;
	RoomRate = 0.06f;
	MaxRoomRadius = 4.f;
	MinSurfaceDepth = 4;
	MinWorldHeight = 3;
}

void UVoxelCaveGenerator::Generate(UVoxelChunk* InChunk)
{
	const FIndex ChunkOrigin = InChunk->GetWorldIndex();
	const FIndex ChunkSize = Module->GetWorldData().ChunkSize;
	TArray<FVoxelCaveSystem> Systems;
	GetCaveSystems(
		ChunkOrigin,
		ChunkOrigin + FIndex(ChunkSize.X - 1, ChunkSize.Y - 1, 0),
		Systems);
	for(const FVoxelCaveSystem& System : Systems) CarveSystemSlice(InChunk, System);
}

bool UVoxelCaveGenerator::TryCreateCaveSystem(FIndex InAnchorChunkIndex, FVoxelCaveSystem& OutSystem) const
{
	const FVector2D Key = InAnchorChunkIndex.ToVector2D();
	if(FMathHelper::HashRand(Key, Seed) > SystemSpawnRate) return false;

	const int32 RandomSeed = FMathHelper::Hash21(Key) ^ Seed ^ Module->GetWorldData().WorldSeed;
	FRandomStream Stream(RandomSeed);
	const FIndex ChunkSize = Module->GetWorldData().ChunkSize;
	const FIndex ChunkOrigin = Module->ChunkIndexToVoxelIndex(InAnchorChunkIndex);
	const int32 StartX = ChunkOrigin.X + Stream.RandRange(0, FMath::Max(ChunkSize.X - 1, 0));
	const int32 StartY = ChunkOrigin.Y + Stream.RandRange(0, FMath::Max(ChunkSize.Y - 1, 0));
	const int32 SurfaceHeight = Module->SampleTopographyByIndex(FIndex(StartX, StartY, 0)).Height;
	if(SurfaceHeight <= Module->GetWorldData().SeaLevel + 1) return false;

	const bool bHasEntrance = Stream.FRand() <= EntranceRate;
	const int32 UndergroundMax = FMath::Max(SurfaceHeight - MinSurfaceDepth - 4, MinWorldHeight + 4);
	FVector Current(StartX, StartY, bHasEntrance ? SurfaceHeight + 1 : Stream.RandRange(MinWorldHeight + 3, UndergroundMax));
	float Yaw = Stream.FRandRange(-PI, PI);
	float Pitch = bHasEntrance ? Stream.FRandRange(-0.75f, -0.45f) : Stream.FRandRange(-0.2f, 0.2f);
	const int32 SegmentCount = Stream.RandRange(FMath::Max(MinSegments, 1), FMath::Max(MaxSegments, MinSegments));

	OutSystem.Segments.Reset();
	for(int32 SegmentIndex = 0; SegmentIndex < SegmentCount; ++SegmentIndex)
	{
		const float Length = Stream.FRandRange(FMath::Max(MinSegmentLength, 1.f), FMath::Max(MaxSegmentLength, MinSegmentLength));
		const FVector Direction(FMath::Cos(Yaw) * FMath::Cos(Pitch), FMath::Sin(Yaw) * FMath::Cos(Pitch), FMath::Sin(Pitch));
		FVector End = Current + Direction * Length;
		const int32 EndSurfaceHeight = Module->SampleTopographyByIndex(FIndex(End)).Height;
		End.Z = FMath::Clamp(End.Z, static_cast<float>(MinWorldHeight), static_cast<float>(EndSurfaceHeight - MinSurfaceDepth));

		FVoxelCaveSegment Segment;
		Segment.Start = Current;
		Segment.End = End;
		Segment.Radius = Stream.FRandRange(FMath::Max(MinTunnelRadius, 0.5f), FMath::Max(MaxTunnelRadius, MinTunnelRadius));
		OutSystem.Segments.Add(Segment);

		if(SegmentIndex > 2 && Stream.FRand() < RoomRate)
		{
			FVoxelCaveSegment Room;
			Room.Start = End;
			Room.End = End;
			Room.Radius = Stream.FRandRange(MaxTunnelRadius + 0.5f, FMath::Max(MaxRoomRadius, MaxTunnelRadius + 0.5f));
			OutSystem.Segments.Add(Room);
		}
		if(SegmentIndex > 3 && Stream.FRand() < BranchRate) AddBranch(Stream, End, Yaw, Pitch, OutSystem);

		Current = End;
		Yaw += Stream.FRandRange(-0.55f, 0.55f);
		Pitch = FMath::Clamp(Pitch + Stream.FRandRange(-0.16f, 0.16f), -0.45f, 0.35f);
		if(bHasEntrance && SegmentIndex < 4) Pitch = FMath::Min(Pitch, -0.3f);
		if(Current.Z <= MinWorldHeight + 1) Pitch = FMath::Abs(Pitch);
	}
	return !OutSystem.Segments.IsEmpty();
}

void UVoxelCaveGenerator::GetCaveSystems(FIndex InMinWorldIndex, FIndex InMaxWorldIndex, TArray<FVoxelCaveSystem>& OutSystems) const
{
	OutSystems.Reset();
	const int32 Reach = FMath::CeilToInt(FMath::Max(MaxSegments, 1) * FMath::Max(MaxSegmentLength, 1.f) + MaxRoomRadius + 2.f);
	const FIndex MinChunk = Module->VoxelIndexToChunkIndex(InMinWorldIndex - FIndex(Reach, Reach, 0));
	const FIndex MaxChunk = Module->VoxelIndexToChunkIndex(InMaxWorldIndex + FIndex(Reach, Reach, 0));
	const int32 Spacing = FMath::Max(SystemSpacingChunks, 1);
	auto AlignToNextAnchor = [Spacing](int32 Value)
	{
		const int32 Remainder = ((Value % Spacing) + Spacing) % Spacing;
		return Remainder == 0 ? Value : Value + Spacing - Remainder;
	};
	for(int32 X = AlignToNextAnchor(MinChunk.X); X <= MaxChunk.X; X += Spacing)
	{
		for(int32 Y = AlignToNextAnchor(MinChunk.Y); Y <= MaxChunk.Y; Y += Spacing)
		{
			FVoxelCaveSystem System;
			if(TryCreateCaveSystem(FIndex(X, Y, 0), System)) OutSystems.Add(MoveTemp(System));
		}
	}
}

void UVoxelCaveGenerator::CarveSystemSlice(UVoxelChunk* InChunk, const FVoxelCaveSystem& InSystem) const
{
	const FIndex ChunkOrigin = InChunk->GetWorldIndex();
	const FIndex ChunkSize = Module->GetWorldData().ChunkSize;
	for(const FVoxelCaveSegment& Segment : InSystem.Segments)
	{
		const int32 Radius = FMath::CeilToInt(Segment.Radius + 0.5f);
		const int32 MinX = FMath::Max(ChunkOrigin.X, FMath::FloorToInt(FMath::Min(Segment.Start.X, Segment.End.X)) - Radius);
		const int32 MaxX = FMath::Min(ChunkOrigin.X + ChunkSize.X - 1, FMath::CeilToInt(FMath::Max(Segment.Start.X, Segment.End.X)) + Radius);
		const int32 MinY = FMath::Max(ChunkOrigin.Y, FMath::FloorToInt(FMath::Min(Segment.Start.Y, Segment.End.Y)) - Radius);
		const int32 MaxY = FMath::Min(ChunkOrigin.Y + ChunkSize.Y - 1, FMath::CeilToInt(FMath::Max(Segment.Start.Y, Segment.End.Y)) + Radius);
		const int32 MinZ = FMath::Max(MinWorldHeight, FMath::FloorToInt(FMath::Min(Segment.Start.Z, Segment.End.Z)) - Radius);
		const int32 MaxZ = FMath::Min(Module->GetWorldData().SkyHeight - 1, FMath::CeilToInt(FMath::Max(Segment.Start.Z, Segment.End.Z)) + Radius);
		const float RadiusSquared = FMath::Square(Segment.Radius + 0.35f);
		for(int32 X = MinX; X <= MaxX; ++X)
		{
			for(int32 Y = MinY; Y <= MaxY; ++Y)
			{
				const int32 SurfaceHeight = Module->SampleTopographyByIndex(FIndex(X, Y, 0)).Height;
				for(int32 Z = MinZ; Z <= FMath::Min(MaxZ, SurfaceHeight + 1); ++Z)
				{
					if(PointSegmentDistanceSquared(FVector(X, Y, Z), Segment) <= RadiusSquared)
					{
						InChunk->SetVoxel(FIndex(X - ChunkOrigin.X, Y - ChunkOrigin.Y, Z), FVoxelItem::Empty);
					}
				}
			}
		}
	}
}

void UVoxelCaveGenerator::AddBranch(FRandomStream& InStream, const FVector& InStart, float InYaw, float InPitch, FVoxelCaveSystem& OutSystem) const
{
	FVector Current = InStart;
	float Yaw = InYaw + InStream.FRandRange(0.9f, 1.8f) * (InStream.FRand() < 0.5f ? -1.f : 1.f);
	float Pitch = FMath::Clamp(InPitch + InStream.FRandRange(-0.15f, 0.15f), -0.35f, 0.3f);
	const int32 Count = InStream.RandRange(3, 6);
	for(int32 Index = 0; Index < Count; ++Index)
	{
		const float Length = InStream.FRandRange(MinSegmentLength, FMath::Max(MaxSegmentLength - 1.f, MinSegmentLength));
		const FVector Direction(FMath::Cos(Yaw) * FMath::Cos(Pitch), FMath::Sin(Yaw) * FMath::Cos(Pitch), FMath::Sin(Pitch));
		FVector End = Current + Direction * Length;
		End.Z = FMath::Max(End.Z, static_cast<float>(MinWorldHeight));
		FVoxelCaveSegment Segment;
		Segment.Start = Current;
		Segment.End = End;
		Segment.Radius = InStream.FRandRange(MinTunnelRadius, FMath::Max(MaxTunnelRadius - 0.25f, MinTunnelRadius));
		OutSystem.Segments.Add(Segment);
		Current = End;
		Yaw += InStream.FRandRange(-0.45f, 0.45f);
		Pitch = FMath::Clamp(Pitch + InStream.FRandRange(-0.12f, 0.12f), -0.4f, 0.3f);
	}
}

float UVoxelCaveGenerator::PointSegmentDistanceSquared(const FVector& InPoint, const FVoxelCaveSegment& InSegment) const
{
	const FVector Segment = InSegment.End - InSegment.Start;
	const float LengthSquared = Segment.SizeSquared();
	if(LengthSquared <= KINDA_SMALL_NUMBER) return FVector::DistSquared(InPoint, InSegment.Start);
	const float Alpha = FMath::Clamp(FVector::DotProduct(InPoint - InSegment.Start, Segment) / LengthSquared, 0.f, 1.f);
	return FVector::DistSquared(InPoint, InSegment.Start + Segment * Alpha);
}
