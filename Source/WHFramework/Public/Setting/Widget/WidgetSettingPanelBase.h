// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "Widget/Screen/UserWidgetBase.h"

#include "WidgetSettingPanelBase.generated.h"

class UCommonButtonBase;
class UCommonActivatableWidgetSwitcher;
class UWidgetSettingPageBase;
class UCommonButton;
class UCommonButtonGroup;
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
	UFUNCTION(BlueprintNativeEvent)
	void OnPageItemSelected(UCommonButtonBase* SelectedTabButton, int32 ButtonIndex);

	UFUNCTION()
	void OnApplyButtonClicked();

	UFUNCTION()
	void OnResetButtonClicked();

protected:
	UFUNCTION(BlueprintNativeEvent)
	UCommonButton* SpawnPageItem(UWidgetSettingPageBase* InPage);

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (BindWidget, OptionalWidget = false), Category = "Components")
	UCommonActivatableWidgetSwitcher* Switcher_Page;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (BindWidget, OptionalWidget = false), Category = "Components")
	UCommonButton* Btn_Apply;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (BindWidget, OptionalWidget = false), Category = "Components")
	UCommonButton* Btn_Reset;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UCommonButtonGroup* PageItemGroup;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Components")
	TSubclassOf<UCommonButton> PageItemClass;

public:
	UFUNCTION(BlueprintPure)
	int32 GetCurrentPageIndex() const;

	UFUNCTION(BlueprintPure)
	UWidgetSettingPageBase* GetCurrentPage() const;

	UFUNCTION(BlueprintCallable)
	void SetCurrentPage(int32 InPageIndex);
};
