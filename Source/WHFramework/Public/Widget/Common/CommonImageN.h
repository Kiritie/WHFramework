#pragma once

#include "Components/Image.h"
#include "GameplayTagContainer.h"
#include "ObjectPool/ObjectPoolInterface.h"

#include "CommonImageN.generated.h"

UCLASS(BlueprintType, Blueprintable, ClassGroup = UI, meta = (Category = "Common UI", DisplayName = "Common Image N"))
class WHFRAMEWORK_API UCommonImageN : public UImage, public IObjectPoolInterface
{
	GENERATED_BODY()

public:
	UCommonImageN(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void SynchronizeProperties() override;

	//////////////////////////////////////////////////////////////////////////
	/// ObjectPool
public:
	virtual void OnSpawn_Implementation(const FParameter& InParam) override;

	virtual void OnDespawn_Implementation(EObjectDespawnMode InMode)
		override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Style", meta = (Categories = "Style.Brush"))
	FGameplayTag StyleTag;

private:
	void ApplyStyleTag();

public:
	UFUNCTION(BlueprintPure, Category = "Style")
	FGameplayTag GetStyleTag() const { return StyleTag; }

	UFUNCTION(BlueprintCallable, Category = "Style")
	void SetStyleTag(FGameplayTag InStyleTag);
};
