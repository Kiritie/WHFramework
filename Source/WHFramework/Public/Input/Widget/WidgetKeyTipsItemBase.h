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
	virtual int32 GetLimit_Implementation() const override { return -1; }

	virtual void OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams) override;
		
	virtual void OnDespawn_Implementation(bool bRecovery) override;

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
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "KeyMapping")
	FText KeyDisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "KeyMapping")
	FString KeyMappingName;

public:
	UFUNCTION(BlueprintPure)
	FText GetKeyDisplayName() const;

	UFUNCTION(BlueprintCallable)
	void SetKeyDisplayName(const FText InKeyDisplayName);

	UFUNCTION(BlueprintPure)
	FString GetKeyMappingName() const;

	UFUNCTION(BlueprintCallable)
	void SetKeyMappingName(const FString& InKeyMappingName);
};
