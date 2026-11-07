#pragma once

#include "CoreMinimal.h"
#include "Scene/SceneActorSaveTypes.h"
#include "Ability/Inventory/InventorySnapshotTypes.h"

#include "AbilityActorSaveTypes.generated.h"

class UAbilityActorDataBase;

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FActorSaveData : public FSceneActorSaveData
{
	GENERATED_BODY()

public:
	FORCEINLINE FActorSaveData()
	{
		AssetID = FPrimaryAssetId();
		Name = NAME_None;
		Level = 1;
		InventorySnapshot = FInventorySnapshot();
		BirthTransform = FTransform::Identity;
	}

	FORCEINLINE FActorSaveData(const FSceneActorSaveData& InSceneActorSaveData) : FSceneActorSaveData(InSceneActorSaveData)
	{
		AssetID = FPrimaryAssetId();
		Name = NAME_None;
		Level = 1;
		InventorySnapshot = FInventorySnapshot();
		BirthTransform = FTransform::Identity;
	}

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FPrimaryAssetId AssetID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Level;

	UPROPERTY(BlueprintReadWrite)
	FInventorySnapshot InventorySnapshot;

	UPROPERTY()
	FTransform BirthTransform;

public:
	virtual bool IsValid() const override
	{
		return AssetID.IsValid();
	}



	virtual bool InitData(const UAbilityActorDataBase* InDefinition, FRandomStream InRandomStream = FRandomStream());
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FVitalitySaveData : public FActorSaveData
{
	GENERATED_BODY()

public:
	FORCEINLINE FVitalitySaveData()
	{
		RaceID = NAME_None;
	}

	FORCEINLINE FVitalitySaveData(const FActorSaveData& InActorSaveData) : FActorSaveData(InActorSaveData)
	{
		RaceID = NAME_None;
	}

public:
	UPROPERTY(BlueprintReadWrite)
	FName RaceID;

};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FPawnSaveData : public FVitalitySaveData
{
	GENERATED_BODY()

public:
	FORCEINLINE FPawnSaveData()
	{

	}

	FORCEINLINE FPawnSaveData(const FVitalitySaveData& InVitalitySaveData) : FVitalitySaveData(InVitalitySaveData)
	{

	}
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FCharacterSaveData : public FPawnSaveData
{
	GENERATED_BODY()

public:
	FORCEINLINE FCharacterSaveData()
	{
	}

	FORCEINLINE FCharacterSaveData(const FPawnSaveData& InPawnSaveData) : FPawnSaveData(InPawnSaveData)
	{
	}
};
