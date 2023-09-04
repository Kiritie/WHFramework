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
	bMainPart = true;
	PartDatas = TMap<FIndex, UVoxelData*>();
	PartIndex = FIndex::ZeroIndex;
	MainData = nullptr;
	MeshScale = FVector::OneVector;
	MeshOffset = FVector::ZeroVector;
	bCustomMesh = false;
	MeshVertices = TArray<FVector>();
	MeshNormals = TArray<FVector>();
	MeshUVDatas.SetNum(6);
	Sounds = TMap<EVoxelSoundType, USoundBase*>();
}

bool UVoxelData::HasPartData(FIndex InIndex) const
{
	if(!bMainPart) return false;

	if(InIndex == FIndex::ZeroIndex) return true;
	if(MainData) return MainData->HasPartData(InIndex);
	return PartDatas.Contains(InIndex);
}

UVoxelData& UVoxelData::GetPartData(FIndex InIndex)
{
	if(!bMainPart) return UReferencePoolModuleBPLibrary::GetReference<UVoxelData>();

	if(InIndex == FIndex::ZeroIndex) return *this;
	if(MainData) return MainData->GetPartData(InIndex);
	if(PartDatas.Contains(InIndex)) return *PartDatas[InIndex];
	return UReferencePoolModuleBPLibrary::GetReference<UVoxelData>();
}

FVector UVoxelData::GetRange(ERightAngle InAngle) const
{
	FVector Range = FVector::OneVector;
	if(bMainPart && PartDatas.Num() > 0)
	{
		FVector PartRange;
		for(auto Iter : PartDatas)
		{
			PartRange.X = FMath::Max(Iter.Key.X, PartRange.X);
			PartRange.Y = FMath::Max(Iter.Key.Y, PartRange.Y);
			PartRange.Z = FMath::Max(Iter.Key.Z, PartRange.Z);
		}
		Range += PartRange;
	}
	Range = UMathBPLibrary::RotatorVector(Range, InAngle, true, true);
	return Range;
}

USoundBase* UVoxelData::GetSound(EVoxelSoundType InSoundType) const
{
	if(Sounds.Contains(InSoundType))
	{
		Sounds[InSoundType];
	}
	return nullptr;
}

void UVoxelData::GetMeshData(const FVoxelItem& InVoxelItem, FVector& OutMeshScale, FVector& OutMeshOffset) const
{
	OutMeshScale = MeshScale;
	OutMeshOffset = MeshOffset;
}

void UVoxelData::GetMeshData(const FVoxelItem& InVoxelItem, TArray<FVector>& OutMeshVertices, TArray<FVector>& OutMeshNormals) const
{
	if(!bCustomMesh) return;

	for(auto Iter : MeshVertices)
	{
		OutMeshVertices.Add(Iter);
	}
	for(auto Iter : MeshNormals)
	{
		OutMeshNormals.Add(Iter.GetSafeNormal());
	}
}

void UVoxelData::GetUVData(const FVoxelItem& InVoxelItem, int32 InFaceIndex, FVector2D InUVSize, FVector2D& OutUVCorner, FVector2D& OutUVSpan) const
{
	if(!MeshUVDatas.IsValidIndex(InFaceIndex)) return;
	
	const FVoxelMeshUVData& UVData = MeshUVDatas[InFaceIndex];
	OutUVCorner = FVector2D(UVData.UVCorner.X * InUVSize.X, (1.f / InUVSize.Y - UVData.UVCorner.Y - UVData.UVSpan.Y) * InUVSize.Y);
	OutUVSpan = FVector2D(UVData.UVSpan.X * InUVSize.X, UVData.UVSpan.Y * InUVSize.Y);
}

bool UVoxelData::IsEmpty() const
{
	return VoxelType == EVoxelType::Empty;
}

bool UVoxelData::IsUnknown() const
{
	return VoxelType == EVoxelType::Unknown;
}

bool UVoxelData::IsCustom() const
{
	return VoxelType >= EVoxelType::Custom1;
}
