#include "Voxel/Datas/VoxelData.h"

#include "Ability/AbilityModuleBPLibrary.h"
#include "Voxel/Voxels/Voxel.h"

UVoxelData::UVoxelData()
{
	Type = FName("Voxel");
	MaxCount = 64;
	VoxelType = EVoxelType::Empty;
	VoxelClass = UVoxel::StaticClass();
	AuxiliaryClass = nullptr;
	Transparency = EVoxelTransparency::Solid;
	bMainPart = true;
	PartDatas = TMap<FIndex, UVoxelData*>();
	PartIndex = FIndex::ZeroIndex;
	MainData = nullptr;
	MeshDatas.SetNum(1);
	Sounds = TMap<EVoxelSoundType, USoundBase*>();
}

void UVoxelData::ResetData_Implementation()
{
	Super::ResetData_Implementation();

	Icon = nullptr;
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
		for(const auto& Iter : PartDatas)
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

const FVoxelMeshData& UVoxelData::GetMeshData(const FVoxelItem& InVoxelItem) const
{
	return MeshDatas[0];
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
