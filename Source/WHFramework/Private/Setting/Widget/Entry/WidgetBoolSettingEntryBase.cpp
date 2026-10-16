#include "Setting/Widget/Entry/WidgetBoolSettingEntryBase.h"

#include "Setting/SettingEntry.h"
#include "Widget/Common/CommonToggle.h"

UWidgetBoolSettingEntryBase::UWidgetBoolSettingEntryBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

UWidget* UWidgetBoolSettingEntryBase::GetValueControl() const
{
	return Toggle_Value;
}

FName UWidgetBoolSettingEntryBase::GetRendererName() const
{
	return TEXT("Bool");
}

void UWidgetBoolSettingEntryBase::BindValueControl()
{
	if (Toggle_Value)
	{
		Toggle_Value->OnValueChanged.AddUniqueDynamic(this, &ThisClass::OnValueChanged);
	}
}

void UWidgetBoolSettingEntryBase::UnbindValueControl()
{
	if (Toggle_Value)
	{
		Toggle_Value->OnValueChanged.RemoveDynamic(this, &ThisClass::OnValueChanged);
	}
}

void UWidgetBoolSettingEntryBase::ApplyValueToControl_Implementation(const FParameter& InValue)
{
	if (Toggle_Value)
	{
		Toggle_Value->SetIsChecked(InValue.Get<bool>());
	}
}

void UWidgetBoolSettingEntryBase::OnValueChanged(UCommonToggle* InToggle, bool bInChecked)
{
	CommitUserValue(FParameter(bInChecked));
}
