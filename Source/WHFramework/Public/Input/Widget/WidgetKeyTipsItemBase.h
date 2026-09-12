// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "ObjectPool/ObjectPoolInterface.h"
#include "Widget/Pool/PoolWidgetBase.h"

#include "WidgetKeyTipsItemBase.generated.h"

class UBorder;
class UHorizontalBox;
class UCommonTextBlockN;

UCLASS(BlueprintType)
class WHFRAMEWORK_API UWidgetKeyTipsItemBase : public UPoolWidgetBase
{
	GENERATED_BODY()

public:
	UWidgetKeyTipsItemBase(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void NativePreConstruct() override;

	//////////////////////////////////////////////////////////////////////////
	/// ObjectPool
public:

	virtual void OnSpawn_Implementation(const FParameter& InParam) override;
		
	virtual void OnDespawn_Implementation(EObjectDespawnMode InMode)
		override;

public:
	UFUNCTION(BlueprintNativeEvent)
	void RefreshData();

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (BindWidget, OptionalWidget = false), Category = "Components")
	UHorizontalBox* Box_KeyIcon;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (BindWidget, OptionalWidget = false), Category = "Components")
	UBorder* Border_KeyCode;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (BindWidget, OptionalWidget = false), Category = "Components")
	UCommonTextBlockN* Txt_KeyCode;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (BindWidget, OptionalWidget = false), Category = "Components")
	UCommonTextBlockN* Txt_DisplayName;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	FText KeyDisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (Categories = "Input"))
	TArray<FGameplayTag> ActionTags;

public:
	UFUNCTION(BlueprintPure)
	FText GetKeyDisplayName() const;

	UFUNCTION(BlueprintCallable)
	void SetKeyDisplayName(const FText InKeyDisplayName);

	UFUNCTION(BlueprintPure, Category = "Input")
	const TArray<FGameplayTag>& GetActionTags() const { return ActionTags; }

	UFUNCTION(BlueprintCallable, Category = "Input")
	void SetActionTags(const TArray<FGameplayTag>& InActionTags);
};
