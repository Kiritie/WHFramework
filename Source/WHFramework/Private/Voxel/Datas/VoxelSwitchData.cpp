#include "Voxel/Datas/VoxelSwitchData.h"

#include "Voxel/Voxels/VoxelSwitch.h"
#include "Voxel/Voxels/Auxiliary/VoxelSwitchAuxiliary.h"

UVoxelSwitchData::UVoxelSwitchData()
{
	VoxelClass = UVoxelSwitch::StaticClass();
	AuxiliaryClass = AVoxelSwitchAuxiliary::StaticClass();

	InteractActions.Empty();
	InteractActions.Add(EVoxelInteractAction::Open);
	InteractActions.Add(EVoxelInteractAction::Close);
}

const FVoxelMeshData& UVoxelSwitchData::GetMeshData(const FVoxelItem& InVoxelItem) const
{
	return MeshDatas[InVoxelItem.Owner && MeshDatas.Num() >= 2 && InVoxelItem.GetVoxel<UVoxelSwitch>().IsOpened() ? 1 : 0];
}
