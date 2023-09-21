#include "Voxel/Datas/VoxelPlantData.h"

#include "Voxel/Voxels/VoxelPlant.h"

UVoxelPlantData::UVoxelPlantData()
{
	VoxelClass = UVoxelPlant::StaticClass();
	
	Transparency = EVoxelTransparency::Transparent;

	MeshDatas[0].bCustomMesh = true;

	MeshDatas[0].MeshVertices.Add(FVector(-0.5f, -0.5f, -0.5f));
	MeshDatas[0].MeshVertices.Add(FVector(-0.5f, -0.5f, 0.5f));
	MeshDatas[0].MeshVertices.Add(FVector(0.5f, 0.5f, 0.5f));
	MeshDatas[0].MeshVertices.Add(FVector(0.5f, 0.5f, -0.5f));
	MeshDatas[0].MeshVertices.Add(FVector(0.5f, -0.5f, -0.5f));
	MeshDatas[0].MeshVertices.Add(FVector(0.5f, -0.5f, 0.5f));
	MeshDatas[0].MeshVertices.Add(FVector(-0.5f, 0.5f, 0.5f));
	MeshDatas[0].MeshVertices.Add(FVector(-0.5f, 0.5f, -0.5f));

	MeshDatas[0].MeshUVDatas.SetNum(2);

	MeshDatas[0].MeshScale = FVector(0.6f, 0.6f, 0.8f);
	MeshDatas[0].MeshOffset = FVector(0.f, 0.f, -0.1f);

	MeshDatas[0].MeshNormals.Add(FVector(1, -1, 0));
	MeshDatas[0].MeshNormals.Add(FVector(-1, -1, 0));
}
