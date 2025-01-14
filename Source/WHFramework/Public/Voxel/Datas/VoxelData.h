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
	virtual void OnInitialize_Implementation() override;

	virtual void OnReset_Implementation() override;

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EVoxelType VoxelType;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSubclassOf<UVoxel> VoxelClass;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSubclassOf<AVoxelAuxiliary> AuxiliaryClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EVoxelNature Nature;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Hardness;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bRotatable;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bMainPart;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditConditionHides, EditCondition = "bMainPart == true"))
	TArray<UVoxelData*> PartDatas;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditConditionHides, EditCondition = "bMainPart == false"))
	FIndex PartIndex;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditConditionHides, EditCondition = "bMainPart == false"))
	UVoxelData* MainData;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditConditionHides, EditCondition = "bMainPart == true"))
	UVoxelData* GatherData;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TMap<EVoxelSoundType, USoundBase*> Sounds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FVoxelMeshData> MeshDatas;

protected:
	UPROPERTY(Transient)
	TMap<FIndex, UVoxelData*> _PartDatas;

public:
	bool IsEmpty() const;

	bool IsUnknown() const;

	bool IsMainPart() const;

	bool IsCustom() const;

public:
	virtual UVoxelData& GetMainData();

	virtual bool HasPartData(FIndex InIndex) const;

	virtual UVoxelData& GetPartData(FIndex InIndex);

	virtual TArray<UVoxelData*> GetPartDatas();

	virtual EVoxelTransparency GetTransparency() const;

	virtual FVector GetRange(ERightAngle InAngle = ERightAngle::RA_0) const;

	virtual USoundBase* GetSound(EVoxelSoundType InSoundType) const;

	virtual const FVoxelMeshData& GetMeshData(const FVoxelItem& InVoxelItem) const;
};
