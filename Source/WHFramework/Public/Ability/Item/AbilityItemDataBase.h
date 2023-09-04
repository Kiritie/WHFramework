#pragma once

#include "Ability/AbilityModuleTypes.h"
#include "Asset/Primary/PrimaryAssetBase.h"

#include "AbilityItemDataBase.generated.h"

UCLASS(BlueprintType)
class WHFRAMEWORK_API UAbilityItemDataBase : public UPrimaryAssetBase
{
	GENERATED_BODY()

public:
	UAbilityItemDataBase();

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText Name;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (MultiLine = "true"))
	FText Detail;
		
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (AllowedClasses="/Script/Engine.Texture,/Script/Engine.MaterialInterface"))
	UObject* Icon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 Price;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 MaxCount;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 MaxLevel;
		
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSubclassOf<class UItemAbilityBase> AbilityClass;

public:
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void SetIconByTexture(UTexture* InTexture, FVector2D InSize = FVector2D::UnitVector, int32 InIndex = 0);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void ReleaseIconAsset();

public:
	UFUNCTION(BlueprintPure)
	EAbilityItemType GetItemType() const;
};
