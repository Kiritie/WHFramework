
#include "Voxel/Generators/VoxelFoliageGenerator.h"

#include "Math/MathHelper.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/Chunks/VoxelChunk.h"

UVoxelFoliageGenerator::UVoxelFoliageGenerator()
{
	Seed = 1317;
	GrassRate = 0.1f;
	FlowerRate = 0.02f;
	TreeRate = 0.006f;
}

void UVoxelFoliageGenerator::Generate(UVoxelChunk* InChunk)
{
	GenerateTrees(InChunk);
	GeneratePlants(InChunk);
}

void UVoxelFoliageGenerator::GeneratePlants(UVoxelChunk* InChunk) const
{
	const FIndex ChunkSize = Module->GetWorldData().ChunkSize;
	ITER_INDEX2D(LocalIndex, ChunkSize, false,
		const FVoxelTopography& Topography = InChunk->GetTopography(LocalIndex);
		if(Topography.Height <= Module->GetWorldData().SeaLevel) continue;
		if(Topography.Fertility < 0.18f || Topography.BiomeType == EVoxelBiomeType::None || Topography.BiomeType == EVoxelBiomeType::Stone ||
			Topography.BiomeType == EVoxelBiomeType::Snow || Topography.BiomeType == EVoxelBiomeType::Desert || Topography.BiomeType == EVoxelBiomeType::Ocean || Topography.BiomeType == EVoxelBiomeType::River ||
			Topography.BiomeType == EVoxelBiomeType::Mountains || Topography.BiomeType == EVoxelBiomeType::Badlands) continue;
		const FIndex SurfaceIndex(LocalIndex.X, LocalIndex.Y, Topography.Height);
		const FIndex PlantIndex(LocalIndex.X, LocalIndex.Y, Topography.Height + 1);
		if(!InChunk->HasVoxel(SurfaceIndex, true) || InChunk->GetVoxel(SurfaceIndex).GetVoxelType() == EVoxelType::Water || InChunk->HasVoxel(PlantIndex, true)) continue;

		const FIndex WorldIndex = InChunk->LocalIndexToWorld(LocalIndex);
		const float ClimateFactor = FMath::Clamp(0.35f + Topography.Fertility, 0.f, 2.f);
		const float Random = FMathHelper::HashRand(WorldIndex.ToVector2D(), Seed + 503);
		EVoxelType PlantType = EVoxelType::Empty;
		if(Random <= FlowerRate * ClimateFactor)
		{
			const int32 FirstFlower = static_cast<int32>(EVoxelType::Flower_Allium);
			const int32 FlowerCount = static_cast<int32>(EVoxelType::Flower_Tulip_White) - FirstFlower + 1;
			PlantType = static_cast<EVoxelType>(FirstFlower + FMathHelper::HashRandRange(WorldIndex.ToVector2D(), 0, FMath::Max(FlowerCount, 1), Seed + 607));
		}
		else if(Random <= (FlowerRate + GrassRate) * ClimateFactor)
		{
			PlantType = EVoxelType::Tall_Grass;
		}
		if(PlantType != EVoxelType::Empty) InChunk->SetVoxel(PlantIndex, PlantType);
	)
}

void UVoxelFoliageGenerator::GenerateTrees(UVoxelChunk* InChunk) const
{
	const int32 Margin = 4;
	const FIndex ChunkOrigin = InChunk->GetWorldIndex();
	const FIndex ChunkSize = Module->GetWorldData().ChunkSize;
	for(int32 X = ChunkOrigin.X - Margin; X <= ChunkOrigin.X + ChunkSize.X - 1 + Margin; ++X)
	{
		for(int32 Y = ChunkOrigin.Y - Margin; Y <= ChunkOrigin.Y + ChunkSize.Y - 1 + Margin; ++Y)
		{
			FVoxelTreeFeature Feature;
			if(TryCreateTreeFeature(FIndex(X, Y, 0), Feature)) PlaceTreeSlice(InChunk, Feature);
		}
	}
}

bool UVoxelFoliageGenerator::TryCreateTreeFeature(FIndex InWorldIndex, FVoxelTreeFeature& OutFeature) const
{
	const FVoxelTopography BaseTopography = Module->SampleBaseTopographyByIndex(InWorldIndex);
	const FVoxelTopography Topography = Module->SampleTopographyByIndex(InWorldIndex);
	if(Topography.Height != BaseTopography.Height || Topography.Height <= Module->GetWorldData().SeaLevel) return false;
	if(Topography.BiomeType == EVoxelBiomeType::None || Topography.BiomeType == EVoxelBiomeType::Snow || Topography.BiomeType == EVoxelBiomeType::Stone || Topography.BiomeType == EVoxelBiomeType::Desert ||
		Topography.BiomeType == EVoxelBiomeType::Ocean || Topography.BiomeType == EVoxelBiomeType::River || Topography.BiomeType == EVoxelBiomeType::Mountains ||
		Topography.BiomeType == EVoxelBiomeType::Badlands || Topography.BiomeType == EVoxelBiomeType::Swamp) return false;

	float BiomeDensity = 0.35f;
	switch(Topography.BiomeType)
	{
		case EVoxelBiomeType::Forest: BiomeDensity = 2.2f; break;
		case EVoxelBiomeType::Taiga: BiomeDensity = 1.5f; break;
		case EVoxelBiomeType::Green: BiomeDensity = 1.f; break;
		case EVoxelBiomeType::Savanna: BiomeDensity = 0.45f; break;
		case EVoxelBiomeType::Plains: BiomeDensity = 0.25f; break;
		case EVoxelBiomeType::Hills: BiomeDensity = 0.55f; break;
		default: break;
	}
	const float ClimateFactor = FMath::Clamp((0.35f + Topography.Fertility) * BiomeDensity, 0.1f, 2.5f);
	const float Random = FMathHelper::HashRand(InWorldIndex.ToVector2D(), Seed);
	if(Random > TreeRate * ClimateFactor) return false;

	OutFeature.Root = FIndex(InWorldIndex.X, InWorldIndex.Y, Topography.Height + 1);
	OutFeature.Height = FMathHelper::HashRandRange(InWorldIndex.ToVector2D(), 4, 7, Seed + 101);
	OutFeature.LeafRadius = OutFeature.Height >= 6 ? 3 : 2;
	if(Topography.Temperature < -0.3f)
	{
		OutFeature.WoodType = EVoxelType::Birch;
		OutFeature.LeafType = EVoxelType::Birch_Leaves;
	}
	else
	{
		OutFeature.WoodType = EVoxelType::Oak;
		OutFeature.LeafType = EVoxelType::Oak_Leaves;
	}
	return true;
}

void UVoxelFoliageGenerator::PlaceTreeSlice(UVoxelChunk* InChunk, const FVoxelTreeFeature& InFeature) const
{
	const FIndex ChunkOrigin = InChunk->GetWorldIndex();
	const FIndex ChunkSize = Module->GetWorldData().ChunkSize;
	auto IsInsideChunk = [&ChunkOrigin, &ChunkSize](int32 X, int32 Y)
	{
		return X >= ChunkOrigin.X && X < ChunkOrigin.X + ChunkSize.X && Y >= ChunkOrigin.Y && Y < ChunkOrigin.Y + ChunkSize.Y;
	};

	if(IsInsideChunk(InFeature.Root.X, InFeature.Root.Y))
	{
		for(int32 Z = 0; Z < InFeature.Height; ++Z)
		{
			const FIndex LocalIndex(InFeature.Root.X - ChunkOrigin.X, InFeature.Root.Y - ChunkOrigin.Y, InFeature.Root.Z + Z);
			if(!InChunk->HasVoxel(LocalIndex, true)) InChunk->SetVoxel(LocalIndex, InFeature.WoodType);
		}
	}

	const int32 CanopyCenterZ = InFeature.Root.Z + InFeature.Height - 1;
	for(int32 Z = -2; Z <= 2; ++Z)
	{
		const int32 LayerRadius = FMath::Max(InFeature.LeafRadius - FMath::Abs(Z) / 2, 1);
		for(int32 X = -LayerRadius; X <= LayerRadius; ++X)
		{
			for(int32 Y = -LayerRadius; Y <= LayerRadius; ++Y)
			{
				if(X * X + Y * Y > LayerRadius * LayerRadius + 1) continue;
				const int32 WorldX = InFeature.Root.X + X;
				const int32 WorldY = InFeature.Root.Y + Y;
				if(!IsInsideChunk(WorldX, WorldY)) continue;
				const FIndex LocalIndex(WorldX - ChunkOrigin.X, WorldY - ChunkOrigin.Y, CanopyCenterZ + Z);
				if(!InChunk->HasVoxel(LocalIndex, true)) InChunk->SetVoxel(LocalIndex, InFeature.LeafType);
			}
		}
	}
}
