#include "Voxel/Datas/VoxelData.h"

#include "Ability/AbilityModuleStatics.h"
#include "Ability/PickUp/AbilityPickUpVoxel.h"
#include "Voxel/VoxelModuleStatics.h"
#include "Voxel/Voxels/Voxel.h"

UVoxelData::UVoxelData()
{
	Type = FName("Voxel");
	MaxCount = 64;
	MaxLevel = 0;
	PickUpClass = AAbilityPickUpVoxel::StaticClass();
	VoxelType = EVoxelType::Empty;
	VoxelClass = UVoxel::StaticClass();
	AuxiliaryClass = nullptr;
	Nature = EVoxelNature::None;
	Hardness = 1.f;
	bMainPart = true;
	PartDatas = TMap<FIndex, UVoxelData*>();
	PartIndex = FIndex::ZeroIndex;
	MainData = nullptr;
	GatherData = nullptr;
	MeshDatas.SetNum(1);
	Sounds = TMap<EVoxelSoundType, USoundBase*>();
}

bool UVoxelData::IsEmpty() const
{
	return VoxelType == EVoxelType::Empty;
}

bool UVoxelData::IsUnknown() const
{
	return VoxelType == EVoxelType::Unknown;
}

bool UVoxelData::IsMainPart() const
{
	return bMainPart;
}

bool UVoxelData::IsCustom() const
{
	return VoxelType >= EVoxelType::Custom1;
}

bool UVoxelData::HasPartData(FIndex InIndex) const
{
	if(MainData) return MainData->HasPartData(InIndex);
	if(InIndex == FIndex::ZeroIndex) return true;
	return PartDatas.Contains(InIndex);
}

UVoxelData& UVoxelData::GetPartData(FIndex InIndex)
{
	if(MainData) return MainData->GetPartData(InIndex);
	if(InIndex == FIndex::ZeroIndex) return *this;
	if(PartDatas.Contains(InIndex)) return *PartDatas[InIndex];
	return UReferencePoolModuleStatics::GetReference<UVoxelData>();
}

UVoxelData& UVoxelData::GetMainData()
{
	if(MainData) return *MainData;
	return *this;
}

EVoxelTransparency UVoxelData::GetTransparency() const
{
	return UVoxelModuleStatics::VoxelNatureToTransparency(Nature);
}

FVector UVoxelData::GetRange(ERightAngle InAngle) const
{
	FVector Range = FVector::OneVector;
	if(PartDatas.Num() > 0)
	{
		FVector PartRange;
		for(const auto& Iter : PartDatas)
		{
			PartRange.X = FMath::Max(Iter.Key.X, PartRange.X);
			PartRange.Y = FMath::Max(Iter.Key.Y, PartRange.Y);
			PartRange.Z = FMath::Max(Iter.Key.Z, PartRange.Z);
		}
		Range += PartRange;
	}
	if(Range != FVector::OneVector)
	{
		Range = UMathStatics::RotatorVector(Range, InAngle, true, true);
	}
	return Range;
}

USoundBase* UVoxelData::GetSound(EVoxelSoundType InSoundType) const
{
	if(Sounds.Contains(InSoundType))
	{
		return Sounds[InSoundType];
	}
	return nullptr;
}

const FVoxelMeshData& UVoxelData::GetMeshData(const FVoxelItem& InVoxelItem) const
{
	return MeshDatas[0];
}
