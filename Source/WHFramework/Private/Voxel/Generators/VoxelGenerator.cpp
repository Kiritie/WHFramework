
#include "Voxel/Generators/VoxelGenerator.h"

UVoxelGenerator::UVoxelGenerator()
{
	Module = nullptr;
	bGenerationEnabled = true;
}

void UVoxelGenerator::Initialize(UVoxelModule* InModule)
{
	Module = InModule;
}

void UVoxelGenerator::Generate(UVoxelChunk* InChunk)
{
	
}
