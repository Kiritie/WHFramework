#include "Voxel/Datas/VoxelContainerData.h"

#include "Ability/AbilityModuleBPLibrary.h"
#include "Voxel/Voxels/VoxelContainer.h"
#include "Voxel/Voxels/Auxiliary/VoxelContainerAuxiliary.h"

UVoxelContainerData::UVoxelContainerData()
{
	VoxelClass = UVoxelContainer::StaticClass();
	AuxiliaryClass = AVoxelContainerAuxiliary::StaticClass();

	InventoryData = FInventorySaveData();
	InventoryData.SplitItems.Add(ESlotSplitType::Default).Items.SetNum(40);
}
