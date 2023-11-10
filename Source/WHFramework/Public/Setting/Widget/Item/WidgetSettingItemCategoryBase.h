// Fill out your copyright notice in the Description Item of Project Settings.

#pragma once
#include "Widget/Screen/UMG/SubWidgetBase.h"

#include "WidgetSettingItemCategoryBase.generated.h"

class UTextBlock;
/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UWidgetSettingItemCategoryBase : public USubWidgetBase
{
	GENERATED_BODY()
	
public:
	UWidgetSettingItemCategoryBase(const FObjectInitializer& ObjectInitializer);

public:
	virtual int32 GetLimit_Implementation() const override { return 1000; }

	virtual void OnSpawn_Implementation(const TArray<FParameter>& InParams) override;

	virtual void OnDespawn_Implementation(bool bRecovery) override;

public:
	virtual void OnCreate(UUserWidgetBase* InOwner, const TArray<FParameter>& InParams) override;

	virtual void OnInitialize(const TArray<FParameter>& InParams) override;

	virtual void OnRefresh() override;

	virtual void OnDestroy() override;

public:
	UFUNCTION(BlueprintPure)
	FText GetCategory() const;

	UFUNCTION(BlueprintCallable)
	void SetCategory(FText InText);

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Components", meta = (BindWidget, OptionalWidget = false))
	UTextBlock* Txt_Category;
};
