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
	MeshScale = FVector::OneVector;
	MeshOffset = FVector::ZeroVector;
	MeshType = EVoxelMeshType::Cube;
	MeshVertices = TArray<FVector>();
	MeshNormals = TArray<FVector>();
	MeshUVDatas.SetNum(6);
	GenerateSound = nullptr;
	DestroySound = nullptr;
}

void UVoxelData::GetDefaultMeshData(const FVoxelItem& InVoxelItem, FVector& OutMeshScale, FVector& OutMeshOffset) const
{
	OutMeshScale = MeshScale;
	OutMeshOffset = MeshOffset;
}

void UVoxelData::GetCustomMeshData(const FVoxelItem& InVoxelItem, TArray<FVector>& OutMeshVertices, TArray<FVector>& OutMeshNormals) const
{
	if(MeshType == EVoxelMeshType::Custom)
	{
		for(auto Iter : MeshVertices)
		{
			OutMeshVertices.Add(Iter);
		}
		for(auto Iter : MeshNormals)
		{
			OutMeshNormals.Add(Iter.GetSafeNormal());
		}
	}
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
