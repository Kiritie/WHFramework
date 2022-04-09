#include "Voxel/Datas/VoxelWaterData.h"

#include "Ability/AbilityModuleBPLibrary.h"
#include "Asset/AssetModuleBPLibrary.h"
#include "Voxel/Voxels/Voxel.h"
#include "Voxel/Voxels/VoxelPlant.h"
#include "Voxel/Voxels/VoxelWater.h"

UVoxelWaterData::UVoxelWaterData()
{
	VoxelType = EVoxelType::Water;
	VoxelClass = UVoxelWater::StaticClass();
	Transparency = EVoxelTransparency::Transparent;
}
