#pragma once

#include "Ability/AbilityModuleTypes.h"
#include "Asset/Primary/PrimaryAssetBase.h"

#include "AbilityItemDataBase.generated.h"

class UItemAbilityBase;
class AAbilityPickUpBase;

UCLASS(BlueprintType)
class WHFRAMEWORK_API UAbilityItemDataBase : public UPrimaryAssetBase
{
	GENERATED_BODY()

public:
	UAbilityItemDataBase();

public:
	virtual void OnInitialize_Implementation() override;
	
	virtual void OnReset_Implementation() override;

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText Name;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (MultiLine = "true"))
	FText Detail;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EAbilityItemRarity Rarity;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (AllowedClasses = "/Script/Engine.Texture,/Script/Engine.MaterialInterface"))
	UObject* Icon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 MaxCount;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 MaxLevel;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSubclassOf<UItemAbilityBase> AbilityClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<AAbilityPickUpBase> PickUpClass;

protected:
	UPROPERTY(Transient)
	UObject* InitIcon;

public:
	UFUNCTION(BlueprintPure)
	int32 ClampCount(int32 InCount) const;

	UFUNCTION(BlueprintPure)
	int32 ClampLevel(int32 InLevel) const;

	UFUNCTION(BlueprintPure)
	EAbilityItemType GetItemType() const;

	UFUNCTION(BlueprintPure)
	virtual FString GetItemAbilityInfo(int32 InLevel = 1) const;

	UFUNCTION(BlueprintPure)
	virtual FString GetItemErrorInfo(AActor* InOwner, int32 InLevel = 1) const;
};
