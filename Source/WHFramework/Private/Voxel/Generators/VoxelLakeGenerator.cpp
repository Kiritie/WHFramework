
#include "Voxel/Generators/VoxelLakeGenerator.h"

#include "Math/MathHelper.h"
#include "Misc/ScopeRWLock.h"
#include "Scene/SceneModuleStatics.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/Chunks/VoxelChunk.h"
#include "Voxel/Generators/VoxelRiverGenerator.h"
#include "Voxel/Voxels/Data/VoxelData.h"

UVoxelLakeGenerator::UVoxelLakeGenerator()
{
	Seed = 8997632;
	SpawnRate = 0.35f;
	NoiseScale = 0.05f;
	MinDistance = 4;
	MinRadius = 4.f;
	MaxRadius = 30.f;
	MinDepth = 3.f;
	MaxDepth = 5.f;
	SpringSeed = 2183;
	SpringSpawnRate = 0.02f;
	SpringMaxDistance = 40.f;
}

void UVoxelLakeGenerator::Generate(UVoxelChunk* InChunk)
{
	const FIndex ChunkOrigin = InChunk->GetWorldIndex();
	const FIndex ChunkSize = Module->GetWorldData().ChunkSize;
	const FIndex ChunkMax = ChunkOrigin + FIndex(ChunkSize.X - 1, ChunkSize.Y - 1, 0);
	TArray<FVoxelLakeFeature> Features;
	GetLakeFeatures(ChunkOrigin, ChunkMax, Features);

	ITER_INDEX2D(LocalIndex, ChunkSize, false,
		const FIndex WorldIndex = InChunk->LocalIndexToWorld(LocalIndex);
		FVoxelTopography Topography = InChunk->GetTopography(LocalIndex);
		int32 WaterHeight = INDEX_NONE;
		for(const FVoxelLakeFeature& Feature : Features)
		{
			int32 BedHeight = 0;
			int32 FeatureWaterHeight = 0;
			if(EvaluateLake(Feature, WorldIndex, BedHeight, FeatureWaterHeight))
			{
				Topography.Height = FMath::Min(Topography.Height, BedHeight);
				Topography.RegionType = EVoxelRegionType::Lake;
				Topography.BiomeType = EVoxelBiomeType::River;
				Topography.Fertility = FMath::Max(Topography.Fertility, 0.8f);
				WaterHeight = FMath::Max(WaterHeight, FeatureWaterHeight);
			}
		}

		Topography.Index = LocalIndex;
		if(WaterHeight != INDEX_NONE) Topography.WaterHeight = WaterHeight;
		InChunk->SetTopography(LocalIndex, Topography);
	)
}

bool UVoxelLakeGenerator::ApplyToTopography(FIndex InWorldIndex, FVoxelTopography& InOutTopography) const
{
	TArray<FVoxelLakeFeature> Features;
	GetLakeFeatures(InWorldIndex, InWorldIndex, Features);
	bool bAffected = false;
	for(const FVoxelLakeFeature& Feature : Features)
	{
		int32 BedHeight = 0;
		int32 WaterHeight = 0;
		if(EvaluateLake(Feature, InWorldIndex, BedHeight, WaterHeight))
		{
			InOutTopography.Height = FMath::Min(InOutTopography.Height, BedHeight);
			InOutTopography.WaterHeight = FMath::Max(InOutTopography.WaterHeight, WaterHeight);
			InOutTopography.RegionType = EVoxelRegionType::Lake;
			InOutTopography.BiomeType = EVoxelBiomeType::River;
			InOutTopography.Fertility = FMath::Max(InOutTopography.Fertility, 0.8f);
			bAffected = true;
		}
	}
	return bAffected;
}

bool UVoxelLakeGenerator::TryCreateLakeFeature(FIndex InChunkIndex, FVoxelLakeFeature& OutFeature) const
{
	const int32 Distance = FMath::Max(MinDistance, 1);
	if(InChunkIndex.X % Distance != 0 || InChunkIndex.Y % Distance != 0) return false;
	const FIndex CacheKey(InChunkIndex.X, InChunkIndex.Y, Module->GetWorldData().WorldSeed);
	{
		FRWScopeLock Lock(LakeFeatureCacheLock, SLT_ReadOnly);
		if(EvaluatedLakeAnchors.Contains(CacheKey))
		{
			if(const FVoxelLakeFeature* CachedFeature = LakeFeatureCache.Find(CacheKey))
			{
				OutFeature = *CachedFeature;
				return true;
			}
			return false;
		}
	}

	FVoxelLakeFeature Feature;
	const bool bCreated = BuildLakeFeature(InChunkIndex, Feature);
	{
		FRWScopeLock Lock(LakeFeatureCacheLock, SLT_Write);
		if(EvaluatedLakeAnchors.Contains(CacheKey))
		{
			if(const FVoxelLakeFeature* CachedFeature = LakeFeatureCache.Find(CacheKey))
			{
				OutFeature = *CachedFeature;
				return true;
			}
			return false;
		}
		if(EvaluatedLakeAnchors.Num() >= 1024)
		{
			EvaluatedLakeAnchors.Reset();
			LakeFeatureCache.Reset();
		}
		EvaluatedLakeAnchors.Add(CacheKey);
		if(bCreated) LakeFeatureCache.Add(CacheKey, Feature);
	}
	if(bCreated) OutFeature = Feature;
	return bCreated;
}

bool UVoxelLakeGenerator::BuildLakeFeature(FIndex InChunkIndex, FVoxelLakeFeature& OutFeature) const
{
	const int32 Distance = FMath::Max(MinDistance, 1);
	const FVector2D Key = InChunkIndex.ToVector2D();
	if((1.f - FMathHelper::HashRand(Key, Seed)) > SpawnRate) return false;
	const UVoxelRiverGenerator* RiverGenerator = Module->GetVoxelGenerator<UVoxelRiverGenerator>();
	if(!RiverGenerator) return false;

	const FIndex ChunkSize = Module->GetWorldData().ChunkSize;
	const FIndex ChunkOrigin = Module->ChunkIndexToVoxelIndex(InChunkIndex);
	const int32 CellSizeX = FMath::Max(ChunkSize.X * Distance, 1);
	const int32 CellSizeY = FMath::Max(ChunkSize.Y * Distance, 1);
	FIndex BestCenter = ChunkOrigin;
	float BestScore = TNumericLimits<float>::Max();
	float BestRiverDistance = TNumericLimits<float>::Max();

	constexpr int32 CandidateGridSize = 5;
	for(int32 GridX = 0; GridX < CandidateGridSize; ++GridX)
	{
		for(int32 GridY = 0; GridY < CandidateGridSize; ++GridY)
		{
			const FVector2D CandidateKey = Key + FVector2D(GridX * 17.13f, GridY * 23.71f);
			const float JitterX = FMathHelper::HashRand(CandidateKey, Seed + 101);
			const float JitterY = FMathHelper::HashRand(CandidateKey, Seed + 211);
			const int32 X = ChunkOrigin.X + FMath::Clamp(FMath::FloorToInt((GridX + JitterX) / CandidateGridSize * CellSizeX), 0, CellSizeX - 1);
			const int32 Y = ChunkOrigin.Y + FMath::Clamp(FMath::FloorToInt((GridY + JitterY) / CandidateGridSize * CellSizeY), 0, CellSizeY - 1);
			const FIndex Candidate(X, Y, 0);
			const float RiverDistance = RiverGenerator->SampleNormalizedRiverDistance(Candidate);
			const int32 Height = Module->SampleBaseTopographyByIndex(Candidate).Height;
			if(Height <= Module->GetWorldData().SeaLevel) continue;
			const float Score = Height + RiverDistance * 10.f;
			if(Score < BestScore)
			{
				BestCenter = Candidate;
				BestScore = Score;
				BestRiverDistance = RiverDistance;
			}
		}
	}
	if(BestRiverDistance > 2.f) return false;

	static const FIndex Directions[] = {
		FIndex(1, 0, 0), FIndex(-1, 0, 0), FIndex(0, 1, 0), FIndex(0, -1, 0),
		FIndex(1, 1, 0), FIndex(-1, 1, 0), FIndex(1, -1, 0), FIndex(-1, -1, 0)
	};
	for(const int32 StepSize : { 8, 4, 2, 1 })
	{
		for(int32 Iteration = 0; Iteration < 4; ++Iteration)
		{
			FIndex Next = BestCenter;
			float NextScore = Module->SampleBaseTopographyByIndex(BestCenter).Height + RiverGenerator->SampleNormalizedRiverDistance(BestCenter) * 8.f;
			for(const FIndex& Direction : Directions)
			{
				const FIndex Candidate = BestCenter + Direction * StepSize;
				const float RiverDistance = RiverGenerator->SampleNormalizedRiverDistance(Candidate);
				const int32 Height = Module->SampleBaseTopographyByIndex(Candidate).Height;
				if(Height <= Module->GetWorldData().SeaLevel) continue;
				const float Score = Height + RiverDistance * 8.f;
				if(Score < NextScore)
				{
					Next = Candidate;
					NextScore = Score;
				}
			}
			if(Next == BestCenter) break;
			BestCenter = Next;
		}
	}
	if(RiverGenerator->SampleNormalizedRiverDistance(BestCenter) > 1.15f) return false;

	const float MajorRadiusMin = FMath::Min(FMath::Max(MinRadius * 2.f, MinRadius), MaxRadius);
	const float MajorRadius = FMathHelper::HashRandRange(Key + FVector2D(7.f, 19.f), MajorRadiusMin, FMath::Max(MaxRadius, MajorRadiusMin), Seed + 307);
	const float AspectRatio = FMathHelper::HashRandRange(Key + FVector2D(31.f, -5.f), 1.7f, 3.8f, Seed + 353);
	const float MinorRadius = FMath::Clamp(MajorRadius / AspectRatio, MinRadius, MajorRadius * 0.72f);
	const float Depth = FMathHelper::HashRandRange(Key + FVector2D(-13.f, 29.f), MinDepth, MaxDepth, Seed + 401);
	FVector2D RiverDirection = RiverGenerator->SampleRiverDirection(BestCenter);
	const float DirectionJitter = FMathHelper::HashRandRange(Key + FVector2D(-37.f, 41.f), -0.35f, 0.35f, Seed + 457);
	const float Rotation = FMath::Atan2(RiverDirection.Y, RiverDirection.X) + DirectionJitter;
	const int32 WaterHeight = RiverGenerator->SampleRiverWaterHeight(BestCenter);
	if(WaterHeight <= Module->GetWorldData().SeaLevel) return false;

	OutFeature.Center = FIndex(BestCenter.X, BestCenter.Y, WaterHeight);
	OutFeature.MajorRadius = MajorRadius;
	OutFeature.MinorRadius = MinorRadius;
	OutFeature.Rotation = Rotation;
	OutFeature.Depth = Depth;

	const int32 BoundaryReach = FMath::CeilToInt(MajorRadius * 1.25f) + 2;
	for(int32 Y = BestCenter.Y - BoundaryReach; Y <= BestCenter.Y + BoundaryReach; ++Y)
	for(int32 X = BestCenter.X - BoundaryReach; X <= BestCenter.X + BoundaryReach; ++X)
	{
		const FIndex ShoreIndex(X, Y, 0);
		if(CalculateShapeAlpha(OutFeature, ShoreIndex) < 1.f) continue;
		bool bTouchesLake = false;
		for(const FIndex& Direction : Directions)
		{
			if(Direction.X != 0 && Direction.Y != 0) continue;
			if(CalculateShapeAlpha(OutFeature, ShoreIndex + Direction) < 1.f)
			{
				bTouchesLake = true;
				break;
			}
		}
		if(!bTouchesLake) continue;

		FVoxelTopography Shore = Module->SampleBaseTopographyByIndex(ShoreIndex);
		RiverGenerator->ApplyToTopography(ShoreIndex, Shore);
		if(Shore.WaterHeight != WaterHeight && Shore.Height < WaterHeight) return false;
	}
	return true;
}

void UVoxelLakeGenerator::GetLakeFeatures(FIndex InMinWorldIndex, FIndex InMaxWorldIndex, TArray<FVoxelLakeFeature>& OutFeatures) const
{
	OutFeatures.Reset();
	const FIndex ChunkSize = Module->GetWorldData().ChunkSize;
	const int32 AnchorReach = FMath::Max(ChunkSize.X, ChunkSize.Y) * FMath::Max(MinDistance, 1);
	const int32 Margin = AnchorReach + FMath::CeilToInt(MaxRadius * 1.5f) + 2;
	const FIndex MinChunk = Module->VoxelIndexToChunkIndex(InMinWorldIndex - FIndex(Margin, Margin, 0));
	const FIndex MaxChunk = Module->VoxelIndexToChunkIndex(InMaxWorldIndex + FIndex(Margin, Margin, 0));
	const int32 Distance = FMath::Max(MinDistance, 1);
	auto AlignToNextAnchor = [Distance](int32 Value)
	{
		const int32 Remainder = ((Value % Distance) + Distance) % Distance;
		return Remainder == 0 ? Value : Value + Distance - Remainder;
	};
	for(int32 X = AlignToNextAnchor(MinChunk.X); X <= MaxChunk.X; X += Distance)
	{
		for(int32 Y = AlignToNextAnchor(MinChunk.Y); Y <= MaxChunk.Y; Y += Distance)
		{
			FVoxelLakeFeature Feature;
			if(TryCreateLakeFeature(FIndex(X, Y, 0), Feature)) OutFeatures.Add(Feature);
		}
	}
}

bool UVoxelLakeGenerator::EvaluateLake(const FVoxelLakeFeature& InFeature, FIndex InWorldIndex, int32& OutBedHeight, int32& OutWaterHeight) const
{
	float Alpha = CalculateShapeAlpha(InFeature, InWorldIndex);
	if(Alpha >= 1.f) return false;

	const int32 BaseHeight = Module->SampleBaseTopographyByIndex(InWorldIndex).Height;
	if(BaseHeight <= Module->GetWorldData().SeaLevel) return false;
	const float HeightAboveWater = FMath::Max(static_cast<float>(BaseHeight - InFeature.Center.Z), 0.f);
	Alpha += HeightAboveWater * 0.16f;
	if(Alpha >= 1.f) return false;
	const float SmoothAlpha = Alpha * Alpha * (3.f - 2.f * Alpha);
	const float Depression = FMath::Lerp(InFeature.Depth, 0.75f, SmoothAlpha);
	OutWaterHeight = InFeature.Center.Z;
	OutBedHeight = FMath::FloorToInt(InFeature.Center.Z - Depression);
	return true;
}

float UVoxelLakeGenerator::CalculateShapeAlpha(const FVoxelLakeFeature& InFeature, FIndex InWorldIndex) const
{
	const FVector2D Delta = InWorldIndex.ToVector2D() - InFeature.Center.ToVector2D();
	const float CosAngle = FMath::Cos(InFeature.Rotation);
	const float SinAngle = FMath::Sin(InFeature.Rotation);
	const float LocalX = Delta.X * CosAngle + Delta.Y * SinAngle;
	const float LocalY = -Delta.X * SinAngle + Delta.Y * CosAngle;
	const float EllipseDistance = FMath::Sqrt(
		FMath::Square(LocalX / FMath::Max(InFeature.MajorRadius, 1.f)) +
		FMath::Square(LocalY / FMath::Max(InFeature.MinorRadius, 1.f)));
	const FVector2D NoisePosition = (InWorldIndex.ToVector2D() + FVector2D(Seed * 0.017f, -Seed * 0.023f)) * NoiseScale * 0.3f;
	const float ShoreVariation = 1.f + Module->GetVoxelNoise2D(NoisePosition) * 0.18f;
	return EllipseDistance / FMath::Max(ShoreVariation, 0.7f);
}

void UVoxelLakeGenerator::GenerateSprings(UVoxelChunk* InChunk) const
{
	const int32 Margin = FMath::CeilToInt(FMath::Max(SpringMaxDistance, 1.f));
	const FIndex ChunkOrigin = InChunk->GetWorldIndex();
	const FIndex ChunkSize = Module->GetWorldData().ChunkSize;
	const FIndex MinChunk = Module->VoxelIndexToChunkIndex(ChunkOrigin - FIndex(Margin, Margin, 0));
	const FIndex MaxChunk = Module->VoxelIndexToChunkIndex(ChunkOrigin + FIndex(ChunkSize.X - 1 + Margin, ChunkSize.Y - 1 + Margin, 0));
	for(int32 X = MinChunk.X; X <= MaxChunk.X; ++X)
	{
		for(int32 Y = MinChunk.Y; Y <= MaxChunk.Y; ++Y)
		{
			FIndex Source;
			if(TryGetSpringSource(FIndex(X, Y, 0), Source)) GenerateSpringSlice(InChunk, Source);
		}
	}
}

bool UVoxelLakeGenerator::TryGetSpringSource(FIndex InChunkIndex, FIndex& OutSource) const
{
	const FVector2D Key = InChunkIndex.ToVector2D();
	if(FMathHelper::HashRand(Key + FVector2D(13.51f, 2.16f), SpringSeed) > SpringSpawnRate) return false;
	const FIndex ChunkSize = Module->GetWorldData().ChunkSize;
	const FIndex ChunkOrigin = Module->ChunkIndexToVoxelIndex(InChunkIndex);
	const int32 OffsetX = FMathHelper::HashRandRange(Key + FVector2D(7.f, -19.f), 0, FMath::Max(ChunkSize.X, 1), SpringSeed + 71);
	const int32 OffsetY = FMathHelper::HashRandRange(Key + FVector2D(-23.f, 5.f), 0, FMath::Max(ChunkSize.Y, 1), SpringSeed + 83);
	OutSource = FIndex(ChunkOrigin.X + OffsetX, ChunkOrigin.Y + OffsetY, 0);
	const FVoxelTopography BaseTopography = Module->SampleBaseTopographyByIndex(OutSource);
	const FVoxelTopography Topography = Module->SampleTopographyByIndex(OutSource);
	return BaseTopography.Height == Topography.Height && Topography.Height > Module->GetWorldData().SeaLevel + 2;
}

void UVoxelLakeGenerator::GenerateSpringSlice(UVoxelChunk* InChunk, FIndex InSource) const
{
	const FIndex ChunkOrigin = InChunk->GetWorldIndex();
	const FIndex ChunkSize = Module->GetWorldData().ChunkSize;
	FIndex Current = InSource;
	TSet<FIndex> Visited;
	static const FIndex Directions[] = {
		FIndex(1, 0, 0), FIndex(-1, 0, 0), FIndex(0, 1, 0), FIndex(0, -1, 0)
	};
	int32 PreviousWaterHeight = INDEX_NONE;
	for(int32 Step = 0; Step < FMath::FloorToInt(SpringMaxDistance); ++Step)
	{
		if(Visited.Contains(Current)) break;
		Visited.Add(Current);
		const FVoxelTopography CurrentTopography = Module->SampleTopographyByIndex(Current);
		const int32 WaterHeight = CurrentTopography.Height + 1;
		if(Current.X >= ChunkOrigin.X && Current.X < ChunkOrigin.X + ChunkSize.X && Current.Y >= ChunkOrigin.Y && Current.Y < ChunkOrigin.Y + ChunkSize.Y)
		{
			const int32 Top = PreviousWaterHeight == INDEX_NONE ? WaterHeight : FMath::Max(WaterHeight, PreviousWaterHeight);
			for(int32 Z = WaterHeight; Z <= Top; ++Z)
			{
				const FIndex LocalIndex(Current.X - ChunkOrigin.X, Current.Y - ChunkOrigin.Y, Z);
				const FVoxelItem& Item = InChunk->GetVoxel(LocalIndex);
				if(!Item.IsValid() || Item.GetData().Nature == EVoxelNature::Foliage || Item.GetData().Nature == EVoxelNature::SemiFoliage)
				{
					InChunk->SetVoxel(LocalIndex, EVoxelType::Water);
				}
			}
		}
		PreviousWaterHeight = WaterHeight;
		if(CurrentTopography.Height <= Module->GetWorldData().SeaLevel) break;
		FIndex Next = Current;
		int32 NextHeight = CurrentTopography.Height;
		for(const FIndex& Direction : Directions)
		{
			const FIndex Candidate = Current + Direction;
			const int32 CandidateHeight = Module->SampleTopographyByIndex(Candidate).Height;
			if(CandidateHeight < NextHeight || CandidateHeight == NextHeight && FMathHelper::HashRand(Candidate.ToVector2D(), SpringSeed + Step) < 0.25f)
			{
				Next = Candidate;
				NextHeight = CandidateHeight;
			}
		}
		if(Next == Current) break;
		Current = Next;
	}
}
