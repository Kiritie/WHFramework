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

	bCustomMesh = true;
	
	MeshVertices = TArray<FVector>();
	MeshVertices.Add(FVector(-0.5f, -0.5f, -0.5f));
	MeshVertices.Add(FVector(-0.5f, -0.5f, 0.5f));
	MeshVertices.Add(FVector(0.5f, 0.5f, 0.5f));
	MeshVertices.Add(FVector(0.5f, 0.5f, -0.5f));
	MeshVertices.Add(FVector(0.5f, -0.5f, -0.5f));
	MeshVertices.Add(FVector(0.5f, -0.5f, 0.5f));
	MeshVertices.Add(FVector(-0.5f, 0.5f, 0.5f));
	MeshVertices.Add(FVector(-0.5f, 0.5f, -0.5f));

	MeshNormals = TArray<FVector>();
	MeshNormals.Add(FVector(1, -1, 0));
	MeshNormals.Add(FVector(-1, -1, 0));
}
