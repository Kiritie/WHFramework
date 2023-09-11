#include "Voxel/Datas/VoxelPlantData.h"

#include "Ability/AbilityModuleBPLibrary.h"
#include "Asset/AssetModuleBPLibrary.h"
#include "Voxel/Voxels/Voxel.h"
#include "Voxel/Voxels/VoxelPlant.h"

UVoxelPlantData::UVoxelPlantData()
{
	VoxelClass = UVoxelPlant::StaticClass();
	
	Transparency = EVoxelTransparency::Transparent;

	MeshData[0].bCustomMesh = true;

	MeshData[0].MeshVertices.Add(FVector(-0.5f, -0.5f, -0.5f));
	MeshData[0].MeshVertices.Add(FVector(-0.5f, -0.5f, 0.5f));
	MeshData[0].MeshVertices.Add(FVector(0.5f, 0.5f, 0.5f));
	MeshData[0].MeshVertices.Add(FVector(0.5f, 0.5f, -0.5f));
	MeshData[0].MeshVertices.Add(FVector(0.5f, -0.5f, -0.5f));
	MeshData[0].MeshVertices.Add(FVector(0.5f, -0.5f, 0.5f));
	MeshData[0].MeshVertices.Add(FVector(-0.5f, 0.5f, 0.5f));
	MeshData[0].MeshVertices.Add(FVector(-0.5f, 0.5f, -0.5f));

	MeshData[0].MeshUVDatas.SetNum(2);

	MeshData[0].MeshScale = FVector(0.6f, 0.6f, 0.8f);
	MeshData[0].MeshOffset = FVector(0.f, 0.f, -0.1f);

	MeshData[0].MeshNormals.Add(FVector(1, -1, 0));
	MeshData[0].MeshNormals.Add(FVector(-1, -1, 0));
}
