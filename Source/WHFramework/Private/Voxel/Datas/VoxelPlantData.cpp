#include "Voxel/Datas/VoxelPlantData.h"

#include "Ability/AbilityModuleBPLibrary.h"
#include "Asset/AssetModuleBPLibrary.h"
#include "Voxel/Voxels/Voxel.h"
#include "Voxel/Voxels/VoxelPlant.h"

UVoxelPlantData::UVoxelPlantData()
{
	VoxelClass = UVoxelPlant::StaticClass();
	
	Transparency = EVoxelTransparency::Transparent;

	MeshType = EVoxelMeshType::Custom;
	
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
