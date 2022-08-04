#include "Voxel/Datas/VoxelData.h"

#include "Ability/AbilityModuleBPLibrary.h"
#include "Asset/AssetModuleBPLibrary.h"
#include "Voxel/Voxels/Voxel.h"

UVoxelData::UVoxelData()
{
	Type = UAbilityModuleBPLibrary::ItemTypeToAssetType(EAbilityItemType::Voxel);
	MaxCount = 64;
	VoxelType = EVoxelType::Empty;
	VoxelClass = nullptr;
	AuxiliaryClass = nullptr;
	Transparency = EVoxelTransparency::Solid;
	bCustomMesh = false;
	Range = FVector::OneVector;
	Offset = FVector::ZeroVector;
	MeshVertices = TArray<FVector>();
	MeshNormals = TArray<FVector>();
	for (int i = 0; i < 6; i++)
	{
		MeshUVDatas.Add(FVoxelMeshUVData());
	}
	GenerateSound = nullptr;
	ReplaceSound = nullptr;
	DestroySound = nullptr;
}

bool UVoxelData::GetMeshDatas(TArray<FVector>& OutMeshVertices, TArray<FVector>& OutMeshNormals, FRotator InRotation, FVector InScale) const
{
	if(bCustomMesh)
	{
		const FVector range = GetFinalRange(InRotation, InScale);

		for(auto Iter : MeshVertices)
		{
			OutMeshVertices.Add(Iter * range);
		}

		for(auto Iter : MeshNormals)
		{
			OutMeshNormals.Add(Iter.GetSafeNormal());
		}
	}
	else
	{
		OutMeshVertices = MeshVertices;
		OutMeshNormals = MeshNormals;
	}
	return OutMeshVertices.Num() > 0;
}
