#include "Voxel/Datas/VoxelBedData.h"

#include "Voxel/Voxels/VoxelBed.h"

UVoxelBedData::UVoxelBedData()
{
	VoxelClass = UVoxelBed::StaticClass();

	InteractActions.Add(EVoxelInteractAction::Interact);
}
