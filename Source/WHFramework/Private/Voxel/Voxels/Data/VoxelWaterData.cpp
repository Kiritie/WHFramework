#include "Voxel/Voxels/Data/VoxelWaterData.h"

#include "Voxel/Voxels/VoxelWater.h"

UVoxelWaterData::UVoxelWaterData()
{
	VoxelType = EVoxelType::Water;
	VoxelClass = UVoxelWater::StaticClass();
	Nature = EVoxelNature::Liquid;
}
