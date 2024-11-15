#pragma once

#include "Ability/Item/AbilityItemDataBase.h"
#include "Voxel/VoxelModuleTypes.h"

#include "VoxelData.generated.h"

UCLASS(BlueprintType)
class WHFRAMEWORK_API UVoxelData : public UAbilityItemDataBase
{
	GENERATED_BODY()

public:
	UVoxelData();

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EVoxelType VoxelType;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSubclassOf<UVoxel> VoxelClass;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSubclassOf<AVoxelAuxiliary> AuxiliaryClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EVoxelTransparency Transparency;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bMainPart;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditConditionHides, EditCondition = "bMainPart == true"))
	TMap<FIndex, UVoxelData*> PartDatas;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditConditionHides, EditCondition = "bMainPart == false"))
	FIndex PartIndex;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditConditionHides, EditCondition = "bMainPart == false"))
	UVoxelData* MainData;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TMap<EVoxelSoundType, USoundBase*> Sounds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FVoxelMeshData> MeshDatas;

public:
	bool IsEmpty() const;

	bool IsUnknown() const;

	bool IsMainPart() const;

	bool IsCustom() const;

public:
	virtual bool HasPartData(FIndex InIndex) const;

	virtual UVoxelData& GetPartData(FIndex InIndex);

	virtual FVector GetRange(ERightAngle InAngle = ERightAngle::RA_0) const;

	virtual USoundBase* GetSound(EVoxelSoundType InSoundType) const;

	virtual const FVoxelMeshData& GetMeshData(const FVoxelItem& InVoxelItem) const;
};
