#include "Voxel/Datas/VoxelDoorData.h"

#include "Ability/AbilityModuleBPLibrary.h"
#include "Asset/AssetModuleBPLibrary.h"
#include "Voxel/Voxels/Voxel.h"
#include "Voxel/Voxels/VoxelDoor.h"
#include "Voxel/Voxels/VoxelPlant.h"
#include "Voxel/Voxels/Auxiliary/VoxelDoorAuxiliary.h"

UVoxelDoorData::UVoxelDoorData()
{
	MaxCount = 10;
	
	VoxelClass = UVoxelDoor::StaticClass();
	AuxiliaryClass = AVoxelDoorAuxiliary::StaticClass();

	Transparency = EVoxelTransparency::SemiTransparent;

	MeshData.SetNum(2);

	MeshData[0].MeshScale = FVector(0.1f, 1.f, 1.f);
	MeshData[0].MeshOffset = FVector(-0.45f, 0.f, 0.f);
	
	MeshData[1].MeshScale = FVector(1.f, 0.1f, 1.f);
	MeshData[1].MeshOffset = FVector(0.f, 0.45f, 0.f);
}

const FVoxelMeshData& UVoxelDoorData::GetMeshData(const FVoxelItem& InVoxelItem) const
{
	return MeshData[InVoxelItem.Owner && InVoxelItem.GetVoxel<UVoxelDoor>().IsOpened() ? 1 : 0];
}
