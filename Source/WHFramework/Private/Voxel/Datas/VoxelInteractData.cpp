#include "Voxel/Datas/VoxelInteractData.h"

#include "Voxel/Voxels/VoxelInteract.h"
#include "Voxel/Voxels/Auxiliary/VoxelInteractAuxiliary.h"

UVoxelInteractData::UVoxelInteractData()
{
	VoxelClass = UVoxelInteract::StaticClass();
	AuxiliaryClass = AVoxelInteractAuxiliary::StaticClass();
}
