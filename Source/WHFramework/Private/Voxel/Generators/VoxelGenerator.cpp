
#include "Voxel/Generators/VoxelGenerator.h"

UVoxelGenerator::UVoxelGenerator()
{
	Module = nullptr;
	Stage = 0;
}

void UVoxelGenerator::Initialize(UVoxelModule* InModule, int32 InStage)
{
	Module = InModule;
	Stage = InStage;
}

void UVoxelGenerator::PrepareBatch(const TArray<FIndex>& InChunkIndices)
{
}

void UVoxelGenerator::Generate(UVoxelChunk* InChunk)
{
	
}

void UVoxelGenerator::CompleteBatch(bool bCancelled)
{
}
