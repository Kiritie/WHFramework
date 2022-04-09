#include "Voxel/Datas/VoxelTorchData.h"

#include "Ability/AbilityModuleBPLibrary.h"
#include "Asset/AssetModuleBPLibrary.h"
#include "Voxel/Voxels/Voxel.h"
#include "Voxel/Voxels/VoxelPlant.h"
#include "Voxel/Voxels/Auxiliary/VoxelTorchAuxiliary.h"

UVoxelTorchData::UVoxelTorchData()
{
	VoxelClass = UVoxelTorchData::StaticClass();
	AuxiliaryClass = AVoxelTorchAuxiliary::StaticClass();

	Transparency = EVoxelTransparency::Transparent;

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
