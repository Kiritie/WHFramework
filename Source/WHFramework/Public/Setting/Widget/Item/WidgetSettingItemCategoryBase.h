// Fill out your copyright notice in the Description Item of Project Settings.

#pragma once
#include "Widget/Pool/PoolWidgetBase.h"

#include "WidgetSettingItemCategoryBase.generated.h"

class UTextBlock;
/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UWidgetSettingItemCategoryBase : public UPoolWidgetBase
{
	GENERATED_BODY()
	
public:
	UWidgetSettingItemCategoryBase(const FObjectInitializer& ObjectInitializer);

	//////////////////////////////////////////////////////////////////////////
	/// ObjectPool
public:

	virtual void OnSpawn_Implementation(const FParameter& InParam) override;
		
	virtual void OnDespawn_Implementation(EObjectDespawnMode InMode)
		override;

public:
	UFUNCTION(BlueprintPure)
	FText GetCategory() const;

	UFUNCTION(BlueprintCallable)
	void SetCategory(FText InText);

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (BindWidget, OptionalWidget = false), Category = "Components")
	UTextBlock* Txt_Category;
};
