#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Asset/DataTableTypes.h"
#include "Ability/Inventory/InventoryItemTypes.h"

#include "AbilityRaceTypes.generated.h"

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FRaceItem
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FPrimaryAssetId ID;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 MinCount;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 MaxCount;

	FORCEINLINE FRaceItem()
	{
		ID = FPrimaryAssetId();
		MinCount = 1;
		MaxCount = 1;
	}
};

/**
 * 种族数据
 */
USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FRaceDataBase : public FDataTableRowBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Detail;

	FORCEINLINE FRaceDataBase()
	{
		Name = NAME_None;
		Detail = TEXT("");
	}
};

/**
 * 种族数据
 */
USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FRaceData : public FRaceDataBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector NoiseScale;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 MinLevel;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 MaxLevel;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FRaceItem> Items;

	FORCEINLINE FRaceData()
	{
		NoiseScale = FVector::OneVector;
		MinLevel = 1;
		MaxLevel = 1;
		Items = TArray<FRaceItem>();
	}
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FVitalityRaceData : public FRaceData
{
	GENERATED_BODY()

public:
	FORCEINLINE FVitalityRaceData()
	{
	}
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FCharacterRaceData : public FRaceData
{
	GENERATED_BODY()

public:
	FORCEINLINE FCharacterRaceData()
	{
	}
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FPlayerRaceData : public FRaceDataBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FAbilityItem> Items;

	FORCEINLINE FPlayerRaceData()
	{
		Items = TArray<FAbilityItem>();
	}
};
