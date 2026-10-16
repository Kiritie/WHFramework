#include "Setting/Widget/Entry/WidgetNumberSettingEntryBase.h"

#include "Setting/SettingEntry.h"
#include "Widget/Common/CommonRange.h"

UWidgetNumberSettingEntryBase::UWidgetNumberSettingEntryBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

UWidget* UWidgetNumberSettingEntryBase::GetValueControl() const
{
	return Range_Value;
}

FName UWidgetNumberSettingEntryBase::GetRendererName() const
{
	return TEXT("Number");
}

void UWidgetNumberSettingEntryBase::BindValueControl()
{
	if (Range_Value)
	{
		Range_Value->OnValueChanged.AddUniqueDynamic(this, &ThisClass::OnValueChanged);
	}
}

void UWidgetNumberSettingEntryBase::UnbindValueControl()
{
	if (Range_Value)
	{
		Range_Value->OnValueChanged.RemoveDynamic(this, &ThisClass::OnValueChanged);
	}
}

void UWidgetNumberSettingEntryBase::ConfigureValueControl()
{
	if (Range_Value && SettingEntry)
	{
		const FSettingNumberDisplay& Display = SettingEntry->GetDefinition().NumberDisplay;
		FCommonRangeConfig Config;
		Config.bHasMin = Display.bHasMin;
		Config.bHasMax = Display.bHasMax;
		Config.Min = Display.Min;
		Config.Max = Display.Max;
		Config.Step = Display.Step;
		Config.DisplayScale = Display.Scale;
		Config.DecimalPlaces = Display.DecimalPlaces;
		Config.Suffix = Display.Suffix;
		Range_Value->Configure(Config);
	}
}

void UWidgetNumberSettingEntryBase::ApplyValueToControl_Implementation(const FParameter& InValue)
{
	if (Range_Value)
	{
		Range_Value->SetValue(InValue);
	}
}

void UWidgetNumberSettingEntryBase::OnValueChanged(UCommonRange* InRange, const FParameter& InValue)
{
	CommitUserValue(InValue);
}
