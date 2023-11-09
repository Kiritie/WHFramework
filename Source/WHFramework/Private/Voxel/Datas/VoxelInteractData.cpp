#include "Voxel/Datas/VoxelInteractData.h"

#include "Ability/AbilityModuleStatics.h"
#include "Voxel/Voxels/VoxelInteract.h"
#include "Voxel/Voxels/Auxiliary/VoxelInteractAuxiliary.h"

UVoxelInteractData::UVoxelInteractData()
{
	VoxelClass = UVoxelInteract::StaticClass();
	AuxiliaryClass = AVoxelInteractAuxiliary::StaticClass();

	InteractActions.Add((EInteractAction)EVoxelInteractAction::Open);
	InteractActions.Add((EInteractAction)EVoxelInteractAction::Close);
}

const FVoxelMeshData& UVoxelInteractData::GetMeshData(const FVoxelItem& InVoxelItem) const
{
	return MeshDatas[InVoxelItem.Owner && MeshDatas.Num() >= 2 && InVoxelItem.GetVoxel<UVoxelInteract>().IsOpened() ? 1 : 0];
}
