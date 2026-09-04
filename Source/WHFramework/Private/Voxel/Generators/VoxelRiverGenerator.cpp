
#include "Voxel/Generators/VoxelRiverGenerator.h"

#include "Voxel/VoxelModule.h"
#include "Voxel/Chunks/VoxelChunk.h"

UVoxelRiverGenerator::UVoxelRiverGenerator()
{
	Seed = 548921;
	RiverScale = 0.0045f;
	WarpScale = 0.0018f;
	RiverHalfWidth = 5.f;
	BankWidth = 5.f;
	RiverDepth = 4;
	RiverHeightAboveSea = 4;
}

void UVoxelRiverGenerator::Generate(UVoxelChunk* InChunk)
{
	const FIndex ChunkOrigin = InChunk->GetWorldIndex();
	const FIndex ChunkSize = Module->GetWorldData().ChunkSize;
	TMap<FIndex, int32> HeightCache;
	HeightCache.Reserve((ChunkSize.X + 8) * (ChunkSize.Y + 8));
	for(int32 X = ChunkOrigin.X - 4; X < ChunkOrigin.X + ChunkSize.X + 4; ++X)
	{
		for(int32 Y = ChunkOrigin.Y - 4; Y < ChunkOrigin.Y + ChunkSize.Y + 4; ++Y)
		{
			const FIndex WorldIndex(X, Y, 0);
			HeightCache.Add(WorldIndex, Module->SampleBaseTopographyByIndex(WorldIndex).Height);
		}
	}
	ITER_INDEX2D(LocalIndex, Module->GetWorldData().ChunkSize, false,
		const FIndex WorldIndex = InChunk->LocalIndexToWorld(LocalIndex);
		FVoxelTopography Topography = InChunk->GetTopography(LocalIndex);
		if(ApplyToTopographyCached(WorldIndex, Topography, HeightCache))
		{
			Topography.Index = LocalIndex;
			InChunk->SetTopography(LocalIndex, Topography);
		}
	)
}

bool UVoxelRiverGenerator::ApplyToTopography(FIndex InWorldIndex, FVoxelTopography& InOutTopography) const
{
	const int32 WaterHeight = CalculateWaterHeight();
	if(InOutTopography.RegionType == EVoxelRegionType::Ocean || InOutTopography.Height <= Module->GetWorldData().SeaLevel ||
		(InOutTopography.Continentalness < 0.08f && InOutTopography.Height < WaterHeight)) return false;
	const float Distance = GetClosedRiverDistance(InWorldIndex);
	const float OuterWidth = RiverHalfWidth + BankWidth;
	return Distance < OuterWidth ? ApplyRiverProfile(InWorldIndex, Distance, InOutTopography, WaterHeight) :
		ApplyRiverShore(InWorldIndex, Distance, InOutTopography, WaterHeight);
}

float UVoxelRiverGenerator::SampleNormalizedRiverDistance(FIndex InWorldIndex) const
{
	return GetClosedRiverDistance(InWorldIndex) / FMath::Max(RiverHalfWidth + BankWidth, KINDA_SMALL_NUMBER);
}

FVector2D UVoxelRiverGenerator::SampleRiverDirection(FIndex InWorldIndex) const
{
	constexpr int32 SampleRadius = 3;
	const float GradientX = GetTerrainAwareRiverField(InWorldIndex + FIndex(SampleRadius, 0, 0)) -
		GetTerrainAwareRiverField(InWorldIndex - FIndex(SampleRadius, 0, 0));
	const float GradientY = GetTerrainAwareRiverField(InWorldIndex + FIndex(0, SampleRadius, 0)) -
		GetTerrainAwareRiverField(InWorldIndex - FIndex(0, SampleRadius, 0));
	const FVector2D Tangent(-GradientY, GradientX);
	return Tangent.IsNearlyZero() ? FVector2D(1.f, 0.f) : Tangent.GetSafeNormal();
}

int32 UVoxelRiverGenerator::SampleRiverWaterHeight(FIndex InWorldIndex) const
{
	return CalculateWaterHeight();
}

bool UVoxelRiverGenerator::ApplyToTopographyCached(FIndex InWorldIndex, FVoxelTopography& InOutTopography, const TMap<FIndex, int32>& InHeightCache) const
{
	const int32 WaterHeight = CalculateWaterHeight();
	if(InOutTopography.RegionType == EVoxelRegionType::Ocean || InOutTopography.Height <= Module->GetWorldData().SeaLevel ||
		(InOutTopography.Continentalness < 0.08f && InOutTopography.Height < WaterHeight)) return false;
	const float Distance = GetClosedRiverDistance(InWorldIndex, &InHeightCache);
	return Distance < RiverHalfWidth + BankWidth ? ApplyRiverProfile(InWorldIndex, Distance, InOutTopography, WaterHeight) :
		ApplyRiverShore(InWorldIndex, Distance, InOutTopography, WaterHeight, &InHeightCache);
}

int32 UVoxelRiverGenerator::CalculateWaterHeight() const
{
	return Module->GetWorldData().SeaLevel + RiverHeightAboveSea;
}

float UVoxelRiverGenerator::SampleBankNoise(FIndex InWorldIndex) const
{
	const FVector2D Position = (InWorldIndex.ToVector2D() + FVector2D(Seed * 0.029f, -Seed * 0.037f)) * 0.035f;
	const float LargeShape = Module->GetVoxelNoise2D(Position);
	const float Detail = Module->GetVoxelNoise2D(Position * 2.17f + FVector2D(-31.7f, 19.3f));
	return FMath::Clamp(LargeShape * 0.72f + Detail * 0.28f, -1.f, 1.f);
}

bool UVoxelRiverGenerator::ApplyRiverProfile(FIndex InWorldIndex, float InRiverDistance, FVoxelTopography& InOutTopography, int32 InWaterHeight) const
{
	const float OuterWidth = FMath::Max(RiverHalfWidth + BankWidth, KINDA_SMALL_NUMBER);
	const float CrossSectionAlpha = FMath::Clamp(InRiverDistance / OuterWidth, 0.f, 1.f);
	const int32 OriginalHeight = InOutTopography.Height;

	const FVector2D DepthPosition = (InWorldIndex.ToVector2D() + FVector2D(Seed * 0.057f, -Seed * 0.043f)) * 0.0012f;
	const float DepthNoise = FMath::Clamp(Module->GetVoxelNoise2D(DepthPosition) * 0.5f + 0.5f, 0.f, 1.f);
	const int32 MinRiverDepth = FMath::Max(RiverDepth - 2, 1);
	const int32 LocalRiverDepth = FMath::RoundToInt(FMath::Lerp(static_cast<float>(MinRiverDepth), static_cast<float>(RiverDepth), DepthNoise));
	const int32 BedHeight = FMath::Max(InWaterHeight - LocalRiverDepth, 1);

	const float CoreAlpha = FMath::Clamp(RiverHalfWidth / OuterWidth, 0.f, 0.95f);
	const float BankAlpha = FMath::Clamp((CrossSectionAlpha - CoreAlpha) / FMath::Max(1.f - CoreAlpha, KINDA_SMALL_NUMBER), 0.f, 1.f);
	const int32 BankHeight = BedHeight + FMath::RoundToInt(BankAlpha * (LocalRiverDepth + 1));
	InOutTopography.Height = CrossSectionAlpha <= CoreAlpha ? FMath::Min(OriginalHeight, BedHeight) : FMath::Min(BankHeight, InWaterHeight);
	const float Strength = CrossSectionAlpha <= CoreAlpha ? 1.f : 1.f - BankAlpha;
	const bool bSandBank = CrossSectionAlpha > CoreAlpha && BankAlpha > 0.25f && InOutTopography.Height >= InWaterHeight - 1 &&
		SampleBankNoise(InWorldIndex) > 0.22f;
	if(Strength > 0.2f || bSandBank)
	{
		InOutTopography.RegionType = EVoxelRegionType::River;
		InOutTopography.BiomeType = bSandBank ? EVoxelBiomeType::Desert : EVoxelBiomeType::River;
		InOutTopography.Fertility = bSandBank ? 0.f : FMath::Max(InOutTopography.Fertility, 0.75f * Strength);
	}
	if(InOutTopography.Height <= InWaterHeight) InOutTopography.WaterHeight = InWaterHeight;
	return true;
}

bool UVoxelRiverGenerator::ApplyRiverShore(FIndex InWorldIndex, float InRiverDistance, FVoxelTopography& InOutTopography, int32 InWaterHeight,
	const TMap<FIndex, int32>* InHeightCache) const
{
	const float OuterWidth = FMath::Max(RiverHalfWidth + BankWidth, KINDA_SMALL_NUMBER);
	const float ShoreStepWidth = OuterWidth / 10.f;
	const float BankNoise = SampleBankNoise(InWorldIndex);
	const float ShoreProgress = (InRiverDistance - OuterWidth) / ShoreStepWidth + BankNoise * 1.4f;
	int32 ShoreStep = FMath::Max(FMath::FloorToInt(ShoreProgress) + 1, 1);
	for(const FIndex Direction : { FIndex(1, 0, 0), FIndex(-1, 0, 0), FIndex(0, 1, 0), FIndex(0, -1, 0) })
	{
		const FIndex NeighborIndex = InWorldIndex + Direction;
		if(GetClosedRiverDistance(NeighborIndex, InHeightCache) < OuterWidth)
		{
			ShoreStep = 1;
			break;
		}
	}
	if(ShoreStep > FMath::Max(RiverDepth * 4, 8)) return false;

	const int32 MinShoreHeight = InWaterHeight - ShoreStep + 1;
	const int32 MaxShoreHeight = InWaterHeight + ShoreStep;
	InOutTopography.Height = FMath::Clamp(InOutTopography.Height, MinShoreHeight, MaxShoreHeight);
	if(InOutTopography.Height <= InWaterHeight + 2 && BankNoise > 0.22f)
	{
		InOutTopography.RegionType = EVoxelRegionType::River;
		InOutTopography.BiomeType = EVoxelBiomeType::Desert;
		InOutTopography.Fertility = 0.f;
	}
	return true;
}

float UVoxelRiverGenerator::CalculateTerrainDifficulty(FIndex InWorldIndex, const TMap<FIndex, int32>* InHeightCache) const
{
	auto SampleHeight = [this, InHeightCache](FIndex Index)
	{
		Index.Z = 0;
		if(InHeightCache)
		{
			if(const int32* Height = InHeightCache->Find(Index)) return *Height;
		}
		return Module->SampleBaseTopographyByIndex(Index).Height;
	};

	const int32 CenterHeight = SampleHeight(InWorldIndex);
	int32 MinHeight = CenterHeight;
	int32 MaxHeight = CenterHeight;
	static const FIndex TerrainSamples[] = {
		FIndex(4, 0, 0), FIndex(-4, 0, 0), FIndex(0, 4, 0), FIndex(0, -4, 0),
		FIndex(3, 3, 0), FIndex(-3, 3, 0), FIndex(3, -3, 0), FIndex(-3, -3, 0)
	};
	for(const FIndex& Offset : TerrainSamples)
	{
		const int32 Height = SampleHeight(InWorldIndex + Offset);
		MinHeight = FMath::Min(MinHeight, Height);
		MaxHeight = FMath::Max(MaxHeight, Height);
	}

	const float ReliefPenalty = FMath::Clamp((MaxHeight - MinHeight - 2.f) / 7.f, 0.f, 1.f);
	const float AltitudeAboveSea = static_cast<float>(CenterHeight - Module->GetWorldData().SeaLevel);
	const float AltitudePenalty = FMath::Clamp((AltitudeAboveSea - 16.f) / 28.f, 0.f, 1.f);
	const FVoxelTopography Topography = Module->SampleBaseTopographyByIndex(InWorldIndex);
	const float MountainScore = Topography.PeaksAndValleys * (1.f - Topography.Erosion) * FMath::Max(Topography.Continentalness, 0.f);
	const float MountainPenalty = FMath::SmoothStep(0.1f, 0.27f, MountainScore);
	return FMath::Max3(ReliefPenalty, AltitudePenalty * 0.65f, MountainPenalty);
}

float UVoxelRiverGenerator::GetBaseRiverField(FIndex InWorldIndex) const
{
	const FVector2D WorldPosition = InWorldIndex.ToVector2D();
	const float WarpX = Module->GetVoxelNoise2D((WorldPosition + FVector2D(Seed * 0.013f, -Seed * 0.009f)) * WarpScale);
	const float WarpY = Module->GetVoxelNoise2D((WorldPosition + FVector2D(-Seed * 0.007f, Seed * 0.017f)) * WarpScale);
	const FVector2D WarpedPosition = WorldPosition + FVector2D(WarpX, WarpY) * 32.f;
	const float MainChannel = Module->GetVoxelNoise2D((WarpedPosition + FVector2D(Seed * 0.021f, Seed * 0.011f)) * RiverScale);
	const float DetailChannel = Module->GetVoxelNoise2D((WarpedPosition + FVector2D(-Seed * 0.031f, Seed * 0.019f)) * RiverScale * 2.3f);
	return MainChannel + DetailChannel * 0.22f;
}

float UVoxelRiverGenerator::GetTerrainAwareRiverField(FIndex InWorldIndex, const TMap<FIndex, int32>* InHeightCache) const
{
	const FVector2D WorldPosition = InWorldIndex.ToVector2D();
	float ChannelField = GetBaseRiverField(InWorldIndex);

	const float TerrainDifficulty = CalculateTerrainDifficulty(InWorldIndex, InHeightCache);
	const float AvoidanceSideNoise = Module->GetVoxelNoise2D((WorldPosition + FVector2D(Seed * 0.041f, -Seed * 0.027f)) * RiverScale * 0.28f);
	ChannelField += AvoidanceSideNoise * TerrainDifficulty * 0.22f;
	return ChannelField;
}

float UVoxelRiverGenerator::GetClosedRiverDistance(FIndex InWorldIndex, const TMap<FIndex, int32>* InHeightCache) const
{
	const float Distance = GetRiverDistance(InWorldIndex, InHeightCache);
	const float OuterWidth = RiverHalfWidth + BankWidth;
	if(Distance < OuterWidth || Distance >= OuterWidth + 8.f) return Distance;

	const float Left = GetRiverDistance(InWorldIndex - FIndex(1, 0, 0), InHeightCache);
	const float Right = GetRiverDistance(InWorldIndex + FIndex(1, 0, 0), InHeightCache);
	const float Backward = GetRiverDistance(InWorldIndex - FIndex(0, 1, 0), InHeightCache);
	const float Forward = GetRiverDistance(InWorldIndex + FIndex(0, 1, 0), InHeightCache);
	return (Left < OuterWidth && Right < OuterWidth) || (Backward < OuterWidth && Forward < OuterWidth) ? OuterWidth - KINDA_SMALL_NUMBER : Distance;
}
float UVoxelRiverGenerator::GetRiverDistance(FIndex InWorldIndex, const TMap<FIndex, int32>* InHeightCache) const
{
	const float ChannelDistance = FMath::Abs(GetTerrainAwareRiverField(InWorldIndex, InHeightCache));

	constexpr int32 GradientSampleRadius = 2;
	const float GradientX = (GetBaseRiverField(InWorldIndex + FIndex(GradientSampleRadius, 0, 0)) -
		GetBaseRiverField(InWorldIndex - FIndex(GradientSampleRadius, 0, 0))) / (GradientSampleRadius * 2.f);
	const float GradientY = (GetBaseRiverField(InWorldIndex + FIndex(0, GradientSampleRadius, 0)) -
		GetBaseRiverField(InWorldIndex - FIndex(0, GradientSampleRadius, 0))) / (GradientSampleRadius * 2.f);
	const float FieldGradient = FMath::Max(FMath::Sqrt(GradientX * GradientX + GradientY * GradientY), 0.0001f);
	return ChannelDistance / FieldGradient;
}
