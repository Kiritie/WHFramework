#include "Voxel/Datas/VoxelContainerData.h"

#include "Ability/AbilityModuleBPLibrary.h"
#include "Asset/AssetModuleBPLibrary.h"
#include "Voxel/Voxels/Voxel.h"
#include "Voxel/Voxels/VoxelContainer.h"
#include "Voxel/Voxels/VoxelPlant.h"
#include "Voxel/Voxels/VoxelInteract.h"
#include "Voxel/Voxels/Auxiliary/VoxelContainerAuxiliary.h"
#include "Voxel/Voxels/Auxiliary/VoxelInteractAuxiliary.h"

UVoxelContainerData::UVoxelContainerData()
{
	VoxelClass = UVoxelContainer::StaticClass();
	AuxiliaryClass = AVoxelContainerAuxiliary::StaticClass();

	InventoryData = FInventorySaveData();
	InventoryData.Items.SetNum(40);
	InventoryData.SplitInfos.Add(ESplitSlotType::Default, FSplitSlotInfo(0, 40));
}
