#pragma once

#include "Blueprint/UserWidget.h"
#include "Setting/SettingModuleTypes.h"
#include "WidgetCustomSettingValueBase.generated.h"

class UWidgetCustomSettingValueBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCustomSettingValueChanged, UWidgetCustomSettingValueBase*, InControl, const FParameter&, InValue);

UCLASS(Abstract, Blueprintable)
class WHFRAMEWORK_API UWidgetCustomSettingValueBase : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent)
	bool CanEditValue(const FParameter& InValue) const;

	UFUNCTION(BlueprintImplementableEvent)
	void Configure(const FSettingDefinition& InDefinition);

	UFUNCTION(BlueprintImplementableEvent)
	void PresentValue(const FParameter& InValue);

	UFUNCTION(BlueprintCallable)
	void SubmitValue(const FParameter& InValue);

	UPROPERTY(BlueprintAssignable)
	FOnCustomSettingValueChanged OnValueChanged;
};
