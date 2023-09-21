#include "Voxel/Datas/VoxelContainerData.h"

#include "Ability/AbilityModuleBPLibrary.h"
#include "Voxel/Voxels/VoxelContainer.h"
#include "Voxel/Voxels/Auxiliary/VoxelContainerAuxiliary.h"

UVoxelContainerData::UVoxelContainerData()
{
	VoxelClass = UVoxelContainer::StaticClass();
	AuxiliaryClass = AVoxelContainerAuxiliary::StaticClass();

	InventoryData = FInventorySaveData();
	InventoryData.Items.SetNum(40);
	InventoryData.SplitInfos.Add(ESplitSlotType::Default, FSplitSlotInfo(0, 40));
}
