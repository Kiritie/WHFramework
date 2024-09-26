#pragma once

#include "Ability/AbilityModuleTypes.h"
#include "Asset/Primary/PrimaryAssetBase.h"

#include "AbilityItemDataBase.generated.h"

class AAbilityPickUpBase;

UCLASS(BlueprintType)
class WHFRAMEWORK_API UAbilityItemDataBase : public UPrimaryAssetBase
{
	GENERATED_BODY()

public:
	UAbilityItemDataBase();

public:
	virtual void ResetData_Implementation() override;

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText Name;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (MultiLine = "true"))
	FText Detail;
		
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (AllowedClasses = "/Script/Engine.Texture,/Script/Engine.MaterialInterface"))
	UObject* Icon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 MaxCount;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 MaxLevel;
		
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSubclassOf<class UItemAbilityBase> AbilityClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<AAbilityPickUpBase> PickUpClass;
	
	UPROPERTY(Transient)
	UMaterialInterface* IconSourceMat;

public:
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void SetIconByTexture(UTexture* InTexture, FVector2D InSize = FVector2D::UnitVector, int32 InIndex = 0);

public:
	UFUNCTION(BlueprintPure)
	EAbilityItemType GetItemType() const;
};
