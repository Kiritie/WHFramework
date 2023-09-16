#include "Voxel/Datas/VoxelInteractData.h"

#include "Ability/AbilityModuleBPLibrary.h"
#include "Asset/AssetModuleBPLibrary.h"
#include "Voxel/Voxels/Voxel.h"
#include "Voxel/Voxels/VoxelPlant.h"
#include "Voxel/Voxels/VoxelInteract.h"
#include "Voxel/Voxels/Auxiliary/VoxelInteractAuxiliary.h"

UVoxelInteractData::UVoxelInteractData()
{
	VoxelClass = UVoxelInteract::StaticClass();
	AuxiliaryClass = AVoxelInteractAuxiliary::StaticClass();
}

const FVoxelMeshData& UVoxelInteractData::GetMeshData(const FVoxelItem& InVoxelItem) const
{
	if(MeshDatas.Num() == 2)
	{
		return MeshDatas[InVoxelItem.Owner && InVoxelItem.GetVoxel<UVoxelInteract>().IsOpened() ? 1 : 0];
	}
	return Super::GetMeshData(InVoxelItem);
}
