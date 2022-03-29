#include "Voxel/Voxels/VoxelAssetBase.h"

#include "Ability/AbilityModuleBPLibrary.h"
#include "Voxel/Voxels/Voxel.h"
#include "Voxel/Voxels/VoxelPlant.h"

UVoxelAssetBase::UVoxelAssetBase()
{
	MaxCount = 64;
	VoxelType = EVoxelType::Empty;
	VoxelClass = nullptr;
	AuxiliaryClass = nullptr;
	Transparency = ETransparency::Solid;
	bCustomMesh = false;
	Range = FVector::OneVector;
	Offset = FVector::ZeroVector;
	MeshVertices = TArray<FVector>();
	MeshNormals = TArray<FVector>();
	for (int i = 0; i < 6; i++)
	{
		MeshUVDatas.Add(FMeshUVData());
	}
	GenerateSound = nullptr;
	OperationSounds = TArray<USoundBase*>();
}

bool UVoxelAssetBase::GetMeshDatas(TArray<FVector>& OutMeshVertices, TArray<FVector>& OutMeshNormals, FRotator InRotation, FVector InScale) const
{
	if(VoxelClass == UVoxelPlant::StaticClass())
	{
		const FVector range = GetFinalRange(InRotation, InScale);

		OutMeshVertices = TArray<FVector>();
		OutMeshVertices.Add(FVector(-0.5f, -0.5f, -0.5f) * range);
		OutMeshVertices.Add(FVector(-0.5f, -0.5f, 0.5f) * range);
		OutMeshVertices.Add(FVector(0.5f, 0.5f, 0.5f) * range);
		OutMeshVertices.Add(FVector(0.5f, 0.5f, -0.5f) * range);
		OutMeshVertices.Add(FVector(0.5f, -0.5f, -0.5f) * range);
		OutMeshVertices.Add(FVector(0.5f, -0.5f, 0.5f) * range);
		OutMeshVertices.Add(FVector(-0.5f, 0.5f, 0.5f) * range);
		OutMeshVertices.Add(FVector(-0.5f, 0.5f, -0.5f) * range);

		OutMeshNormals = TArray<FVector>();
		OutMeshNormals.Add(FVector(1, -1, 0).GetSafeNormal());
		OutMeshNormals.Add(FVector(-1, -1, 0).GetSafeNormal());
	}
	else
	{
		OutMeshVertices = MeshVertices;
		OutMeshNormals = MeshNormals;
	}
	return OutMeshVertices.Num() > 0;
}
