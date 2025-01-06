#include "Voxel/Datas/VoxelContainerData.h"

#include "Ability/AbilityModuleStatics.h"
#include "Ability/Inventory/AbilityInventoryBase.h"
#include "Voxel/Voxels/VoxelContainer.h"
#include "Voxel/Voxels/Auxiliary/VoxelContainerAuxiliary.h"

UVoxelContainerData::UVoxelContainerData()
{
	VoxelClass = UVoxelContainer::StaticClass();
	AuxiliaryClass = AVoxelContainerAuxiliary::StaticClass();

	InventoryData = FInventorySaveData();
	InventoryData.InventoryClass = UAbilityInventoryBase::StaticClass();
	InventoryData.SplitItems.Add(ESlotSplitType::Default, 40);
}
