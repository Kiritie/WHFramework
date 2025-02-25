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
	virtual int32 GetLimit_Implementation() const override { return -1; }

	virtual void OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams) override;
		
	virtual void OnDespawn_Implementation(bool bRecovery) override;

public:
	UFUNCTION(BlueprintPure)
	FText GetCategory() const;

	UFUNCTION(BlueprintCallable)
	void SetCategory(FText InText);

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (BindWidget, OptionalWidget = false), Category = "Components")
	UTextBlock* Txt_Category;
};
