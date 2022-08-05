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
	PartType = EVoxelPartType::Main;
	PartDatas = TMap<FIndex, UVoxelData*>();
	PartIndex = FIndex::ZeroIndex;
	MainData = nullptr;
	Offset = FVector::ZeroVector;
	MeshVertices = TArray<FVector>();
	MeshNormals = TArray<FVector>();
	for (int i = 0; i < 6; i++)
	{
		MeshUVDatas.Add(FVoxelMeshUVData());
	}
	GenerateSound = nullptr;
	DestroySound = nullptr;
}

bool UVoxelData::HasPartData(FIndex InIndex) const
{
	if(InIndex == FIndex::ZeroIndex) return true;
	if(MainData) return MainData->HasPartData(InIndex);
	return PartDatas.Contains(InIndex);
}

UVoxelData& UVoxelData::GetPartData(FIndex InIndex)
{
	if(InIndex == FIndex::ZeroIndex) return *this;
	if(MainData) return MainData->GetPartData(InIndex);
	if(PartDatas.Contains(InIndex)) return *PartDatas[InIndex];
	return UReferencePoolModuleBPLibrary::GetReference<UVoxelData>();
}

bool UVoxelData::GetMeshDatas(TArray<FVector>& OutMeshVertices, TArray<FVector>& OutMeshNormals, FRotator InRotation, FVector InScale) const
{
	if(bCustomMesh)
	{
		const FVector range = GetRange(InRotation, InScale);

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
