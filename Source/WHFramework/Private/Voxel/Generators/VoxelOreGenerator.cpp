
#include "Voxel/Generators/VoxelOreGenerator.h"

#include "Math/MathHelper.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/Chunks/VoxelChunk.h"

UVoxelOreGenerator::UVoxelOreGenerator()
{
	Seed = 34834;
	SpawnRate = 0.5f;
	GenerateDatas = {
		FVoxelOreGenerateData(EVoxelType::Diamond_Ore, 10, 0.01f, 1, 3),
		FVoxelOreGenerateData(EVoxelType::Emerald_Ore, 20, 0.03f, 1, 5),
		FVoxelOreGenerateData(EVoxelType::Gold_Ore, 30, 0.05f, 3, 8),
		FVoxelOreGenerateData(EVoxelType::Iron_Ore, 50, 0.1f, 5, 10),
		FVoxelOreGenerateData(EVoxelType::Coal_Ore, -1, 0.45f, 10, 20),
	};
}

void UVoxelOreGenerator::Generate(UVoxelChunk* InChunk)
{
	ITER_INDEX2D(Index, Module->GetWorldData().ChunkSize, false,
		const int32 Height = InChunk->GetTopography(Index).Height - 2;
		for(int32 Z = 1; Z < Height; ++Z)
		{
			const FIndex LocalIndex(Index.X, Index.Y, Z);
			if(InChunk->HasVoxel(LocalIndex)) continue;
			const FIndex WorldIndex = InChunk->LocalIndexToWorld(FIndex(Index.X, Index.Y, Z));
			for(int32 DataIndex = 0; DataIndex < GenerateDatas.Num(); ++DataIndex)
			{
				const FVoxelOreGenerateData& Data = GenerateDatas[DataIndex];
				if(Data.MaxHeight != -1 && WorldIndex.Z > Data.MaxHeight) continue;
				const float AverageSize = FMath::Max((Data.MinSize + Data.MaxSize) * 0.5f, 1.f);
				const float Scale = 0.13f / FMath::Sqrt(AverageSize);
				const FVector NoisePosition = WorldIndex.ToVector() * Scale + FVector(DataIndex * 31.7f, Seed * 0.001f, -DataIndex * 17.3f);
				const float VeinNoise = Module->GetVoxelNoise3D(NoisePosition);
				const int32 Hash = FMathHelper::Hash31(FVector(WorldIndex.X + Seed, WorldIndex.Y - Seed, WorldIndex.Z + DataIndex * 7919));
				const float Random = static_cast<float>(FMath::Abs(Hash % 10000)) / 9999.f;
				if(VeinNoise >= SpawnRate && Random <= Data.SpawnRate)
				{
					InChunk->SetVoxel(LocalIndex, Data.VoxelType);
					break;
				}
			}
		}
	)
}
