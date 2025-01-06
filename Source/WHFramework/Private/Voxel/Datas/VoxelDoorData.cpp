#include "Voxel/Datas/VoxelDoorData.h"

#include "Voxel/Voxels/VoxelDoor.h"
#include "Voxel/Voxels/Auxiliary/VoxelDoorAuxiliary.h"

UVoxelDoorData::UVoxelDoorData()
{
	MaxCount = 10;
	
	VoxelClass = UVoxelDoor::StaticClass();
	AuxiliaryClass = AVoxelDoorAuxiliary::StaticClass();

	Nature = EVoxelNature::SemiSolid;

	MeshDatas.SetNum(2);

	MeshDatas[0].MeshScale = FVector(0.1f, 1.f, 1.f);
	MeshDatas[0].MeshOffset = FVector(-0.45f, 0.f, 0.f);
	
	MeshDatas[1].MeshScale = FVector(1.f, 0.1f, 1.f);
	MeshDatas[1].MeshOffset = FVector(0.f, 0.45f, 0.f);
}
