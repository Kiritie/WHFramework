// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "Widget/Screen/UserWidgetBase.h"

#include "WidgetSettingPanelBase.generated.h"

class UCommonButtonBase;
class UCommonActivatableWidgetSwitcher;
class UWidgetSettingPageBase;
class UCommonButton;
class UCommonButtonGroup;
class UPanelWidget;
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
	virtual void OnCreate(const FParameter& InParam) override;
	
	virtual void OnInitialize(const FParameter& InParam) override;

	virtual void OnOpen(const FParameter& InParam, bool bInstant) override;

	virtual void OnClose(bool bInstant) override;

protected:
	UFUNCTION(BlueprintNativeEvent)
	void OnPageItemSelected(UCommonButtonBase* AssociatedButton, int32 ButtonIndex);

	UFUNCTION()
	void OnApplyButtonClicked();

	UFUNCTION()
	void OnResetButtonClicked();

	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnConfirmationRequested")
	void K2_OnConfirmationRequested(float InTimeout);

protected:
	UFUNCTION(BlueprintNativeEvent)
	UCommonButton* SpawnPageItem(UWidgetSettingPageBase* InPage);

	void GenerateSettingPages();

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (BindWidget, OptionalWidget = false), Category = "Components")
	UCommonActivatableWidgetSwitcher* Switcher_Page;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (BindWidget, OptionalWidget = false), Category = "Components")
	UCommonButton* Btn_Apply;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (BindWidget, OptionalWidget = false), Category = "Components")
	UCommonButton* Btn_Reset;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (BindWidget, OptionalWidget = true), Category = "Components")
	TObjectPtr<UPanelWidget> PageItemContainer;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UCommonButtonGroup* PageItemGroup;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Components")
	TSubclassOf<UCommonButton> PageItemClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Components")
	TSubclassOf<UWidgetSettingPageBase> SettingPageClass;

public:
	UFUNCTION(BlueprintPure)
	int32 GetCurrentPageIndex() const;

	UFUNCTION(BlueprintPure)
	UWidgetSettingPageBase* GetCurrentPage() const;

	UFUNCTION(BlueprintCallable)
	void SetCurrentPage(int32 InPageIndex);

	UFUNCTION(BlueprintCallable)
	bool ConfirmPendingSettings();

	UFUNCTION(BlueprintCallable)
	void RejectPendingSettings();
};
