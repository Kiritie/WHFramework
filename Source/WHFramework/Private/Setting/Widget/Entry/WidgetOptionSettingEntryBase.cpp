#include "Setting/Widget/Entry/WidgetOptionSettingEntryBase.h"

#include "Setting/SettingEntry.h"
#include "Widget/Common/CommonOptionSelector.h"

UWidgetOptionSettingEntryBase::UWidgetOptionSettingEntryBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

UWidget* UWidgetOptionSettingEntryBase::GetValueControl() const
{
	return Selector_Value;
}

FName UWidgetOptionSettingEntryBase::GetRendererName() const
{
	return TEXT("Option");
}

void UWidgetOptionSettingEntryBase::BindValueControl()
{
	if (Selector_Value)
	{
		Selector_Value->OnOptionChanged.AddUniqueDynamic(this, &ThisClass::OnOptionChanged);
	}
}

void UWidgetOptionSettingEntryBase::UnbindValueControl()
{
	if (Selector_Value)
	{
		Selector_Value->OnOptionChanged.RemoveDynamic(this, &ThisClass::OnOptionChanged);
	}
}

void UWidgetOptionSettingEntryBase::ConfigureValueControl()
{
	if (Selector_Value && SettingEntry)
	{
		TArray<FText> Labels;
		for (const FSettingOption& Option : SettingEntry->GetDefinition().Options)
		{
			Labels.Add(Option.DisplayName);
		}
		Selector_Value->SetUseDropdown(bUseDropdown);
		Selector_Value->SetOptions(Labels);
	}
}

void UWidgetOptionSettingEntryBase::ApplyValueToControl_Implementation(const FParameter& InValue)
{
	if (Selector_Value && SettingEntry)
	{
		const int32 Index = SettingEntry->GetDefinition().Options.IndexOfByPredicate([&InValue](const FSettingOption& Option)
		{
			return Option.Value == InValue;
		});
		Selector_Value->SetOptionIndex(Index);
	}
}

void UWidgetOptionSettingEntryBase::OnOptionChanged(UCommonOptionSelector* InSelector, int32 InIndex)
{
	if (SettingEntry && SettingEntry->GetDefinition().Options.IsValidIndex(InIndex))
	{
		CommitUserValue(SettingEntry->GetDefinition().Options[InIndex].Value);
	}
}
