#include "Voxel/Datas/VoxelInteractData.h"

#include "Voxel/Voxels/VoxelInteract.h"
#include "Voxel/Voxels/Auxiliary/VoxelInteractAuxiliary.h"

UVoxelInteractData::UVoxelInteractData()
{
	VoxelClass = UVoxelInteract::StaticClass();
	AuxiliaryClass = AVoxelInteractAuxiliary::StaticClass();

	InteractActions.Add(EVoxelInteractAction::Interact);
	InteractActions.Add(EVoxelInteractAction::UnInteract);
}
