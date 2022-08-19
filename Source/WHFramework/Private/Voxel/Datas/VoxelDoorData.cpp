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

	MeshScale = FVector(0.1f, 1.f, 1.f);
	MeshOffset = FVector(-0.45f, 0.f, 0.f);
	
	OpenMeshScale = FVector(1.f, 0.1f, 1.f);
	OpenMeshOffset = FVector(0.f, 0.45f, 0.f);

	OpenSound = nullptr;
	CloseSound = nullptr;
}

void UVoxelDoorData::GetDefaultMeshData(const FVoxelItem& InVoxelItem, EVoxelMeshNature InMeshNature, FVector& OutMeshScale, FVector& OutMeshOffset) const
{
	if(InMeshNature == EVoxelMeshNature::Chunk && InVoxelItem.GetVoxel<UVoxelDoor>().IsOpened())
	{
		OutMeshScale = OpenMeshScale;
		OutMeshOffset = OpenMeshOffset;
	}
	else
	{
		OutMeshScale = MeshScale;
		OutMeshOffset = MeshOffset;
	}
}
