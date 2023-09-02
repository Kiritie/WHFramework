#include "Voxel/Datas/VoxelTreeData.h"

#include "Ability/AbilityModuleBPLibrary.h"
#include "Asset/AssetModuleBPLibrary.h"
#include "Voxel/Voxels/Voxel.h"
#include "Voxel/Voxels/VoxelTree.h"

UVoxelTreeData::UVoxelTreeData()
{
	TreeRange = FVector(6.f);
	TreeHeight = FVector2D(4.f, 5.f);
	LeavesHeight = FVector2D(2.f);
	LeavesWidth = FVector2D(3.f, 5.f);
}
