
#include "Voxel/Generators/VoxelSurfaceGenerator.h"

#include "Voxel/VoxelModule.h"
#include "Voxel/Chunks/VoxelChunk.h"

UVoxelSurfaceGenerator::UVoxelSurfaceGenerator()
{
	TemperatureSeed = 143;
	HumiditySeed = 201;
	ClimateOctaves = 3;
	ClimateScale = 28.f;
	HeightLayers = {
		FVoxelSurfaceNoiseLayer(6.f, 0.2f),
		FVoxelSurfaceNoiseLayer(12.f, 0.3f),
		FVoxelSurfaceNoiseLayer(32.f, 0.4f)
	};
	ContinentScale = 28.f;
	ErosionScale = 14.f;
	RidgeScale = 7.f;
	ContinentHeight = 16.f;
	MountainHeight = 38.f;
}

void UVoxelSurfaceGenerator::Generate(UVoxelChunk* InChunk)
{
	ITER_INDEX2D(LocalIndex, Module->GetWorldData().ChunkSize, false,
		FVoxelTopography Topography = SampleTopography(InChunk->LocalIndexToWorld(LocalIndex));
		Topography.Index = LocalIndex;
		InChunk->SetTopography(LocalIndex, Topography);
	)
}

FVoxelTopography UVoxelSurfaceGenerator::SampleTopography(FIndex InWorldIndex) const
{
	FVoxelTopography Topography;
	Topography.Index = InWorldIndex;
	Topography.Continentalness = SampleContinentalness(InWorldIndex);
	Topography.Erosion = SampleErosion(InWorldIndex);
	Topography.PeaksAndValleys = SamplePeaksAndValleys(InWorldIndex);
	Topography.Height = SampleHeight(InWorldIndex, Topography.Continentalness, Topography.Erosion, Topography.PeaksAndValleys);
	Topography.Temperature = SampleTemperature(InWorldIndex, Topography.Height);
	Topography.Humidity = SampleHumidity(InWorldIndex);
	Topography.Fertility = FMath::Clamp(Topography.Humidity * (1.f - FMath::Abs(Topography.Temperature) * 0.45f) * (0.75f + Topography.Erosion * 0.25f), 0.f, 1.f);
	Topography.RegionType = SampleRegion(Topography);
	Topography.BiomeType = SampleBiome(Topography);
	if(Topography.BiomeType == EVoxelBiomeType::Desert) Topography.Fertility *= 0.1f;
	else if(Topography.BiomeType == EVoxelBiomeType::Snow) Topography.Fertility *= 0.15f;
	return Topography;
}

float UVoxelSurfaceGenerator::SampleTemperature(FIndex InWorldIndex, int32 InHeight) const
{
	float Result = 0.f;
	float Scale = FMath::Max(ClimateScale, 1.f);
	const FVector2D ChunkSize = Module->GetWorldData().ChunkSize;
	const int32 Octaves = FMath::Max(ClimateOctaves, 1);
	for(int32 Octave = 0; Octave < Octaves; ++Octave)
	{
		const FVector2D Position(InWorldIndex.X / ChunkSize.X / Scale + TemperatureSeed, InWorldIndex.Y / ChunkSize.Y / Scale - TemperatureSeed);
		const float Value = Module->GetVoxelNoise2D(Position);
		Result += FMath::Clamp(Value, -1.f, 1.f) / Octaves;
		Scale *= 2.f;
	}
	const float AltitudeCooling = FMath::Max(InHeight - Module->GetWorldData().SeaLevel, 0) * 0.0125f;
	return FMath::Clamp(Result - AltitudeCooling, -1.f, 1.f);
}

float UVoxelSurfaceGenerator::SampleHumidity(FIndex InWorldIndex) const
{
	float Result = 0.f;
	float Scale = FMath::Max(ClimateScale, 1.f);
	const FVector2D ChunkSize = Module->GetWorldData().ChunkSize;
	const int32 Octaves = FMath::Max(ClimateOctaves, 1);
	for(int32 Octave = 0; Octave < Octaves; ++Octave)
	{
		const FVector2D Position(InWorldIndex.X / ChunkSize.X / Scale + HumiditySeed, InWorldIndex.Y / ChunkSize.Y / Scale - HumiditySeed);
		const float Value = Module->GetVoxelNoise2D(Position);
		Result += FMath::Clamp(Value, -1.f, 1.f) / Octaves;
		Scale *= 2.f;
	}
	return FMath::SmoothStep(-0.5f, 0.5f, Result);
}

float UVoxelSurfaceGenerator::SampleContinentalness(FIndex InWorldIndex) const
{
	const FVector2D ChunkSize = Module->GetWorldData().ChunkSize;
	const FVector2D Position = InWorldIndex.ToVector2D() / FMath::Max(ContinentScale * ChunkSize.X, 1.f);
	const float Primary = Module->GetVoxelNoise2D(Position);
	const float Secondary = Module->GetVoxelNoise2D(Position * 2.07f + FVector2D(37.1f, -19.7f)) * 0.35f;
	return FMath::Clamp(Primary * 0.75f + Secondary, -1.f, 1.f);
}

float UVoxelSurfaceGenerator::SampleErosion(FIndex InWorldIndex) const
{
	const FVector2D ChunkSize = Module->GetWorldData().ChunkSize;
	const FVector2D Position = InWorldIndex.ToVector2D() / FMath::Max(ErosionScale * ChunkSize.X, 1.f);
	return FMath::Clamp(Module->GetVoxelNoise2D(Position) * 0.5f + 0.5f, 0.f, 1.f);
}

float UVoxelSurfaceGenerator::SamplePeaksAndValleys(FIndex InWorldIndex) const
{
	const FVector2D ChunkSize = Module->GetWorldData().ChunkSize;
	const FVector2D Position = InWorldIndex.ToVector2D() / FMath::Max(RidgeScale * ChunkSize.X, 1.f);
	return FMath::Clamp(1.f - FMath::Abs(Module->GetVoxelNoise2D(Position)), 0.f, 1.f);
}

int32 UVoxelSurfaceGenerator::SampleHeight(FIndex InWorldIndex, float InContinentalness, float InErosion, float InPeaksAndValleys) const
{
	const int32 SeaLevel = Module->GetWorldData().SeaLevel;
	const FVector2D ChunkSize = Module->GetWorldData().ChunkSize;
	float Detail = 0.f;
	for(const FVoxelSurfaceNoiseLayer& Layer : HeightLayers)
	{
		const float Scale = FMath::Max(Layer.Scale * ChunkSize.X, 1.f);
		Detail += Module->GetVoxelNoise2D(InWorldIndex.ToVector2D() / Scale) * Layer.Weight;
	}

	const float LandAlpha = FMath::SmoothStep(-0.35f, 0.15f, InContinentalness);
	const float OceanFloor = SeaLevel - 8.f + InContinentalness * 8.f;
	const float Plains = SeaLevel + 3.f + InContinentalness * ContinentHeight + Detail * 3.5f;
	const float Relief = 1.f - InErosion;
	const float HillMask = FMath::SmoothStep(-0.05f, 0.4f, InContinentalness) * FMath::SmoothStep(0.2f, 0.65f, Relief);
	const float MountainMask = FMath::SmoothStep(0.18f, 0.62f, InContinentalness) * FMath::SmoothStep(0.48f, 0.9f, Relief);
	const float Hills = FMath::Pow(InPeaksAndValleys, 1.15f) * HillMask * 9.f;
	const float Mountain = FMath::Pow(InPeaksAndValleys, 1.35f) * MountainMask * MountainHeight;
	const float Height = FMath::Lerp(OceanFloor, Plains + Hills + Mountain, LandAlpha);
	return FMath::Clamp(FMath::RoundToInt(Height), 1, Module->GetWorldData().SkyHeight - 1);
}

EVoxelBiomeType UVoxelSurfaceGenerator::SampleBiome(const FVoxelTopography& InTopography) const
{
	if(InTopography.RegionType == EVoxelRegionType::Ocean) return EVoxelBiomeType::Ocean;
	if(InTopography.RegionType == EVoxelRegionType::Mountain)
	{
		return InTopography.Temperature < -0.25f ? EVoxelBiomeType::Snow : EVoxelBiomeType::Mountains;
	}
	if(InTopography.RegionType == EVoxelRegionType::Hills) return EVoxelBiomeType::Hills;

	if(InTopography.Temperature < -0.35f) return InTopography.Humidity > 0.45f ? EVoxelBiomeType::Taiga : EVoxelBiomeType::Snow;
	if(InTopography.Temperature > 0.45f)
	{
		if(InTopography.Humidity < 0.28f) return EVoxelBiomeType::Desert;
		if(InTopography.Humidity < 0.5f) return EVoxelBiomeType::Savanna;
		return EVoxelBiomeType::Swamp;
	}
	if(InTopography.Humidity > 0.62f) return EVoxelBiomeType::Forest;
	if(InTopography.Humidity < 0.22f) return EVoxelBiomeType::Dry;
	return EVoxelBiomeType::Plains;
}

EVoxelRegionType UVoxelSurfaceGenerator::SampleRegion(const FVoxelTopography& InTopography) const
{
	if(InTopography.Height <= Module->GetWorldData().SeaLevel) return EVoxelRegionType::Ocean;
	const float MountainScore = InTopography.PeaksAndValleys * (1.f - InTopography.Erosion) * FMath::Max(InTopography.Continentalness, 0.f);
	if(MountainScore > 0.27f) return EVoxelRegionType::Mountain;
	if(InTopography.Temperature < -0.35f) return EVoxelRegionType::Icefield;
	if(InTopography.Temperature > 0.45f && InTopography.Humidity < 0.28f) return EVoxelRegionType::Desert;
	if(MountainScore > 0.1f || InTopography.Height > Module->GetWorldData().SeaLevel + 15) return EVoxelRegionType::Hills;
	if(InTopography.Erosion > 0.42f) return EVoxelRegionType::Plain;
	return EVoxelRegionType::Wilderness;
}
