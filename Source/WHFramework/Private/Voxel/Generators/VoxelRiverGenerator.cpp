
#include "Voxel/Generators/VoxelRiverGenerator.h"

#include "Math/MathHelper.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/Chunks/VoxelChunk.h"

UVoxelRiverGenerator::UVoxelRiverGenerator()
{
	Seed = 548921;
	RiverScale = 0.0045f;
	WarpScale = 0.0018f;
	RiverWidth = 0.035f;
	BankWidth = 0.035f;
	RiverDepth = 4;
	MinAltitudeAboveSea = 1;
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
	if(InOutTopography.Height <= Module->GetWorldData().SeaLevel + MinAltitudeAboveSea) return false;
	const float Distance = GetRiverDistance(InWorldIndex);
	const float OuterWidth = RiverWidth + BankWidth;
	if(Distance >= OuterWidth) return false;
	return ApplyRiverProfile(InWorldIndex, Distance, InOutTopography, CalculateWaterHeight(InWorldIndex));
}

float UVoxelRiverGenerator::SampleNormalizedRiverDistance(FIndex InWorldIndex) const
{
	return GetRiverDistance(InWorldIndex) / FMath::Max(RiverWidth + BankWidth, KINDA_SMALL_NUMBER);
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
	return CalculateWaterHeight(InWorldIndex);
}

bool UVoxelRiverGenerator::ApplyToTopographyCached(FIndex InWorldIndex, FVoxelTopography& InOutTopography, const TMap<FIndex, int32>& InHeightCache) const
{
	if(InOutTopography.Height <= Module->GetWorldData().SeaLevel + MinAltitudeAboveSea) return false;
	const float Distance = GetRiverDistance(InWorldIndex, &InHeightCache);
	if(Distance >= RiverWidth + BankWidth) return false;
	return ApplyRiverProfile(InWorldIndex, Distance, InOutTopography, CalculateWaterHeight(InWorldIndex, &InHeightCache));
}

int32 UVoxelRiverGenerator::CalculateWaterHeight(FIndex InWorldIndex, const TMap<FIndex, int32>* InHeightCache) const
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
	int32 WaterHeight = SampleHeight(InWorldIndex) - 1;
	static const FIndex WaterSamples[] = {
		FIndex(4, 0, 0), FIndex(-4, 0, 0), FIndex(0, 4, 0), FIndex(0, -4, 0),
		FIndex(3, 3, 0), FIndex(-3, 3, 0), FIndex(3, -3, 0), FIndex(-3, -3, 0)
	};
	for(const FIndex& Offset : WaterSamples) WaterHeight = FMath::Min(WaterHeight, SampleHeight(InWorldIndex + Offset) - 1);
	return FMath::Max(WaterHeight, Module->GetWorldData().SeaLevel);
}

bool UVoxelRiverGenerator::ApplyRiverProfile(FIndex InWorldIndex, float InRiverDistance, FVoxelTopography& InOutTopography, int32 InWaterHeight) const
{
	const float OuterWidth = FMath::Max(RiverWidth + BankWidth, KINDA_SMALL_NUMBER);
	const float CrossSectionAlpha = FMath::Clamp(InRiverDistance / OuterWidth, 0.f, 1.f);
	const int32 OriginalHeight = InOutTopography.Height;

	const FVector2D DepthPosition = (InWorldIndex.ToVector2D() + FVector2D(Seed * 0.057f, -Seed * 0.043f)) * 0.0012f;
	const float DepthNoise = FMath::Clamp(Module->GetVoxelNoise2D(DepthPosition) * 0.5f + 0.5f, 0.f, 1.f);
	const int32 MinRiverDepth = FMath::Max(RiverDepth - 2, 1);
	const int32 LocalRiverDepth = FMath::RoundToInt(FMath::Lerp(static_cast<float>(MinRiverDepth), static_cast<float>(RiverDepth), DepthNoise));
	const float RiverDepthAlpha = RiverDepth > MinRiverDepth ? static_cast<float>(LocalRiverDepth - MinRiverDepth) / (RiverDepth - MinRiverDepth) : 1.f;
	const int32 BedHeight = FMath::Max(InWaterHeight - LocalRiverDepth, 1);

	const float CoreAlpha = FMath::Clamp(RiverWidth / OuterWidth, 0.f, 0.95f);
	const float BankAlpha = FMath::Clamp((CrossSectionAlpha - CoreAlpha) / FMath::Max(1.f - CoreAlpha, KINDA_SMALL_NUMBER), 0.f, 1.f);
	const FIndex SandPatchIndex(FMath::FloorToInt(InWorldIndex.X * 0.25f), FMath::FloorToInt(InWorldIndex.Y * 0.25f), 0);
	if(CrossSectionAlpha > CoreAlpha && BankAlpha > 0.35f && OriginalHeight - InWaterHeight <= 2 &&
		FMathHelper::HashRand(SandPatchIndex.ToVector2D(), Seed + 791) < 0.06f)
	{
		const float SandSlope = FMath::SmoothStep(0.35f, 1.f, BankAlpha);
		InOutTopography.Height = FMath::RoundToInt(FMath::Lerp(static_cast<float>(BedHeight), static_cast<float>(InWaterHeight), SandSlope));
		InOutTopography.WaterHeight = InWaterHeight;
		InOutTopography.RegionType = EVoxelWorldRegionType::River;
		InOutTopography.BiomeType = EVoxelBiomeType::Desert;
		InOutTopography.Fertility = 0.f;
		return true;
	}

	float Strength = 1.f;
	if(CrossSectionAlpha > CoreAlpha)
	{
		const float SmoothBankAlpha = BankAlpha * BankAlpha * (3.f - 2.f * BankAlpha);
		const float BankExponent = FMath::Lerp(4.f, 2.4f, RiverDepthAlpha);
		Strength = 1.f - FMath::Pow(SmoothBankAlpha, BankExponent);
	}
	InOutTopography.Height = FMath::RoundToInt(FMath::Lerp(static_cast<float>(OriginalHeight), static_cast<float>(FMath::Min(OriginalHeight, BedHeight)), Strength));
	if(Strength > 0.2f)
	{
		InOutTopography.RegionType = EVoxelWorldRegionType::River;
		InOutTopography.BiomeType = EVoxelBiomeType::River;
		InOutTopography.Fertility = FMath::Max(InOutTopography.Fertility, 0.75f * Strength);
	}
	if(InWaterHeight < OriginalHeight && InOutTopography.Height <= InWaterHeight) InOutTopography.WaterHeight = InWaterHeight;
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
	const EVoxelWorldRegionType RegionType = Module->SampleBaseTopographyByIndex(InWorldIndex).RegionType;
	const float RegionPenalty = RegionType == EVoxelWorldRegionType::Mountain ? 1.f : RegionType == EVoxelWorldRegionType::Hills ? 0.72f : 0.f;
	return FMath::Max3(ReliefPenalty, AltitudePenalty * 0.65f, RegionPenalty);
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
	const float AvoidanceSide = AvoidanceSideNoise >= 0.f ? 1.f : -1.f;
	ChannelField += AvoidanceSide * TerrainDifficulty * 0.22f;
	return ChannelField;
}

float UVoxelRiverGenerator::GetRiverDistance(FIndex InWorldIndex, const TMap<FIndex, int32>* InHeightCache) const
{
	const float ChannelDistance = FMath::Abs(GetTerrainAwareRiverField(InWorldIndex, InHeightCache));

	constexpr int32 GradientSampleRadius = 2;
	constexpr float MaxRiverHalfWidthInVoxels = 8.f;
	const float GradientX = (GetBaseRiverField(InWorldIndex + FIndex(GradientSampleRadius, 0, 0)) -
		GetBaseRiverField(InWorldIndex - FIndex(GradientSampleRadius, 0, 0))) / (GradientSampleRadius * 2.f);
	const float GradientY = (GetBaseRiverField(InWorldIndex + FIndex(0, GradientSampleRadius, 0)) -
		GetBaseRiverField(InWorldIndex - FIndex(0, GradientSampleRadius, 0))) / (GradientSampleRadius * 2.f);
	const float FieldGradient = FMath::Max(FMath::Sqrt(GradientX * GradientX + GradientY * GradientY), 0.0001f);
	const float EstimatedDistanceInVoxels = ChannelDistance / FieldGradient;
	const float WidthLimitedDistance = EstimatedDistanceInVoxels / MaxRiverHalfWidthInVoxels * (RiverWidth + BankWidth);
	return FMath::Max(ChannelDistance, WidthLimitedDistance);
}
