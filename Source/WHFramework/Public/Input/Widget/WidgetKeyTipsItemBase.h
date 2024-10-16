// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Widget/Screen/SubWidgetBase.h"
#include "WidgetKeyTipsItemBase.generated.h"

class UBorder;
class UHorizontalBox;
class UCommonTextBlockN;

UCLASS(BlueprintType)
class WHFRAMEWORK_API UWidgetKeyTipsItemBase : public UUserWidget, public IObjectPoolInterface
{
	GENERATED_BODY()

public:
	UWidgetKeyTipsItemBase(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void NativePreConstruct() override;

public:
	virtual int32 GetLimit_Implementation() const override { return -1; }

	virtual void OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams) override;

	virtual void OnDespawn_Implementation(bool bRecovery) override;

public:
	UFUNCTION(BlueprintNativeEvent)
	void RefreshData();

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Components", meta = (BindWidget, OptionalWidget = false))
	UHorizontalBox* Box_KeyIcon;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Components", meta = (BindWidget, OptionalWidget = false))
	UBorder* Border_KeyCode;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Components", meta = (BindWidget, OptionalWidget = false))
	UCommonTextBlockN* Txt_KeyCode;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Components", meta = (BindWidget, OptionalWidget = false))
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
