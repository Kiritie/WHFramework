#pragma once

#include "Widget/Common/CommonInput.h"
#include "CommonOptionSelector.generated.h"

class UCommonButton;
class UCommonTextBlockN;
class UCommonOptionSelector;
class UNativeWidgetHost;
class UCommonImageN;
class SScrollBar;
class SBox;
template<typename OptionType>
class SComboBox;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCommonOptionChanged, UCommonOptionSelector*, InSelector, int32, InIndex);

UCLASS(BlueprintType, Blueprintable)
class WHFRAMEWORK_API UCommonOptionSelector : public UCommonInput
{
	GENERATED_BODY()

public:
	UCommonOptionSelector(const FObjectInitializer& ObjectInitializer);
	virtual void OnSpawn_Implementation(const FParameter& InParam) override;
	virtual void OnDespawn_Implementation(EObjectDespawnMode InMode) override;
	virtual void SetValue(const FParameter& InValue) override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

	UPROPERTY(BlueprintAssignable)
	FOnCommonOptionChanged OnOptionChanged;

	UFUNCTION(BlueprintCallable)
	void SetOptions(const TArray<FText>& InOptions);

	UFUNCTION(BlueprintCallable)
	void SetOptionNames(const TArray<FString>& InOptionNames);

	UFUNCTION(BlueprintPure)
	TArray<FString> GetOptionNames() const;

	UFUNCTION(BlueprintCallable)
	void SetOptionIndex(int32 InOptionIndex, bool bNotify = false);

	UFUNCTION(BlueprintPure)
	int32 GetOptionIndex() const;

	UFUNCTION(BlueprintPure)
	FString GetSelectedOption() const;

	UFUNCTION(BlueprintCallable)
	void SetUseDropdown(bool bInUseDropdown);

protected:
	virtual void BuildNativeLayout() override;
	virtual void ApplyResolvedStyle(const UObject* InStyle) override;
	virtual void BindControlEvents() override;
	virtual void UnbindControlEvents() override;
	virtual void SynchronizeProperties() override;
	virtual void HandleTextChanged(const FText& InText) override;

	void HandleSelectionChanged(FName InKey, ESelectInfo::Type InSelectionType);

	UFUNCTION()
	UWidget* GenerateOption(FName InKey);

	UFUNCTION()
	UWidget* GenerateSelectedOption(FName InKey);

	void OnPreviousClicked();
	void OnNextClicked();
	void RefreshValue();
	void BuildDropdown();
	void RefreshSelectedLabel();
	TSharedRef<SWidget> GenerateOptionSlate(FName InKey);

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UCommonButton> Btn_Last;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UCommonButton> Btn_Next;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UNativeWidgetHost> Combo_Value;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bUseDropdown = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bEditable = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FText> Options;

	UPROPERTY(BlueprintReadOnly)
	int32 OptionIndex = INDEX_NONE;

private:
	friend class FCommonOptionLabelLifetimeTest;
	friend class FCommonOptionStyleRefreshTest;
	friend class FCommonControlPoolResetTest;
	TSharedPtr<SComboBox<FName>> Dropdown;
	TSharedPtr<SScrollBar> DropdownScrollbar;
	TSharedPtr<SBox> SelectedContent;

	UPROPERTY(Transient)
	FComboBoxStyle DropdownStyle;

	UPROPERTY(Transient)
	FTableRowStyle DropdownItemStyle;

	UPROPERTY(Transient)
	FScrollBarStyle DropdownScrollStyle;

	FVector2D DropdownScrollBarThickness = FVector2D(5.f);

	TArray<FName> OptionKeys;
	FGameplayTag OptionTextStyleTag;

	UPROPERTY(Transient)
	TObjectPtr<UCommonImageN> NativeValueBackground;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UCommonTextBlockN>> GeneratedLabels;

	UPROPERTY(Transient)
	TObjectPtr<UCommonTextBlockN> SelectedLabel;
};
