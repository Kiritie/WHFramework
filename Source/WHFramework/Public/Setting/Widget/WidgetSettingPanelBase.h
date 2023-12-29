// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "Widget/Screen/UMG/UserWidgetBase.h"

#include "WidgetSettingPanelBase.generated.h"

class UWidgetSettingPageBase;
class UWidgetSettingPageItemBase;
class UCommonButtonGroup;
class UCommonButton;
/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UWidgetSettingPanelBase : public UUserWidgetBase
{
	GENERATED_BODY()
	
public:
	UWidgetSettingPanelBase(const FObjectInitializer& ObjectInitializer);
	
public:
	virtual void OnCreate(UObject* InOwner, const TArray<FParameter>& InParams) override;
	
	virtual void OnInitialize(UObject* InOwner, const TArray<FParameter>& InParams) override;

	virtual void OnOpen(const TArray<FParameter>& InParams, bool bInstant) override;

	virtual void OnClose(bool bInstant) override;

protected:
	UFUNCTION()
	void OnApplyButtonClicked();

	UFUNCTION()
	void OnResetButtonClicked();

protected:
	UFUNCTION(BlueprintNativeEvent)
	UWidgetSettingPageItemBase* SpawnPageItem(UWidgetSettingPageBase* InPage);

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Components", meta = (BindWidget, OptionalWidget = false))
	UCommonButton* Btn_Apply;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Components", meta = (BindWidget, OptionalWidget = false))
	UCommonButton* Btn_Reset;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UCommonButtonGroup* PageItemGroup;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Components")
	TSubclassOf<UWidgetSettingPageItemBase> PageItemClass;

public:
	UFUNCTION(BlueprintPure)
	int32 GetCurrentPageIndex() const;

	UFUNCTION(BlueprintPure)
	UWidgetSettingPageBase* GetCurrentPage() const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void SetCurrentPage(int32 InPageIndex);
};
