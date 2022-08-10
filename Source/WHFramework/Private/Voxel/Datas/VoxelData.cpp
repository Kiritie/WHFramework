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
	PartType = EVoxelPartType::Main;
	PartDatas = TMap<FIndex, UVoxelData*>();
	PartIndex = FIndex::ZeroIndex;
	MainData = nullptr;
	Offset = FVector::ZeroVector;
	MeshType = EVoxelMeshType::Cube;
	MeshVertices = TArray<FVector>();
	MeshNormals = TArray<FVector>();
	MeshUVDatas.SetNumZeroed(6);
	GenerateSound = nullptr;
	DestroySound = nullptr;
}

bool UVoxelData::HasPartData(FIndex InIndex) const
{
	if(PartType == EVoxelPartType::Main)
	{
		if(InIndex == FIndex::ZeroIndex) return true;
		if(MainData) return MainData->HasPartData(InIndex);
		return PartDatas.Contains(InIndex);
	}
	return false;
}

UVoxelData& UVoxelData::GetPartData(FIndex InIndex)
{
	if(PartType == EVoxelPartType::Main)
	{
		if(InIndex == FIndex::ZeroIndex) return *this;
		if(MainData) return MainData->GetPartData(InIndex);
		if(PartDatas.Contains(InIndex)) return *PartDatas[InIndex];
	}
	return UReferencePoolModuleBPLibrary::GetReference<UVoxelData>();
}

bool UVoxelData::GetMeshDatas(const FVoxelItem& InVoxelItem, TArray<FVector>& OutMeshVertices, TArray<FVector>& OutMeshNormals, FRotator InRotation) const
{
	if(MeshType == EVoxelMeshType::Custom)
	{
		const FVector range = GetRange(InRotation);

		for(auto Iter : MeshVertices)
		{
			OutMeshVertices.Add(Iter * range);
		}

		for(auto Iter : MeshNormals)
		{
			OutMeshNormals.Add(Iter.GetSafeNormal());
		}
		return true;
	}
	return false;
}
