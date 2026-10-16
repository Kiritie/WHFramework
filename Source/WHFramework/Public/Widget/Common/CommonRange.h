#pragma once

#include "Components/EditableTextBox.h"
#include "Widget/Common/CommonRangeTypes.h"
#include "Widget/Common/CommonWidget.h"
#include "CommonRange.generated.h"

class UCommonInput;
class USlider;
class USpinBox;
class UCommonTextBlockN;
class UCommonRange;
class UCommonButton;
class UCommonImageN;
class UOverlay;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCommonRangeChanged, UCommonRange*, InRange, const FParameter&, InValue);

UCLASS(BlueprintType, Blueprintable)
class WHFRAMEWORK_API UCommonRange : public UCommonWidget
{
	GENERATED_BODY()

public:
	UCommonRange(const FObjectInitializer& ObjectInitializer);
	virtual void OnDespawn_Implementation(EObjectDespawnMode InMode) override;

	UPROPERTY(BlueprintAssignable)
	FOnCommonRangeChanged OnValueChanged;

	UPROPERTY(BlueprintAssignable)
	FOnCommonRangeChanged OnValueCommitted;

	UFUNCTION(BlueprintCallable)
	void Configure(const FCommonRangeConfig& InConfig);

	UFUNCTION(BlueprintCallable)
	void SetValue(const FParameter& InValue, bool bNotify = false);

	UFUNCTION(BlueprintPure)
	FParameter GetValue() const;

	UFUNCTION(BlueprintCallable)
	void SetInputFocus();

protected:
	virtual EWidgetStyleKind GetStyleKind() const override;
	virtual void BuildNativeLayout() override;
	virtual void ApplyResolvedStyle(const UObject* InStyle) override;
	virtual void BindControlEvents() override;
	virtual void UnbindControlEvents() override;
	virtual void SynchronizeProperties() override;

	UFUNCTION()
	void HandleSliderChanged(float InValue);

	UFUNCTION()
	void HandleSpinChanged(float InValue);

	UFUNCTION()
	void HandleSpinCommitted(float InValue, ETextCommit::Type InCommitMethod);

	UFUNCTION()
	void HandleInputCommitted(UCommonInput* InInput, const FText& InText, ETextCommit::Type InCommitMethod);

	UFUNCTION()
	void HandleCaptureEnd();

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<USlider> Slider_Value;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<USpinBox> SpinBox_Value;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UCommonInput> Input_Value;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlockN> Txt_Value;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlockN> Txt_MinValue;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlockN> Txt_MaxValue;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlockN> Txt_Suffix;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UCommonButton> Btn_Previous;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UCommonButton> Btn_Next;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FCommonRangeConfig Config;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FParameter Value = FParameter(0.0);

	UPROPERTY(Transient)
	TObjectPtr<UOverlay> NativeValueArea;

	UPROPERTY(Transient)
	TObjectPtr<UCommonImageN> NativeValueBackground;

private:
	friend class FCommonRangeStyleRefreshTest;

	void RefreshControls();
	void AcceptUserValue(FParameter InValue, bool bCommit);
	void StepValue(int32 InDirection);
	void OnPreviousClicked();
	void OnNextClicked();
	FParameter MakeTypedValue(double InValue) const;
	static bool NumericValue(const FParameter& InValue, double& OutValue);
	FText FormatValue(double InValue) const;
};
