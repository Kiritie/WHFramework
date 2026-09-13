#include "Setting/Widget/Entry/WidgetEnumSettingEntryBase.h"

#include "Components/ComboBoxString.h"
#include "Setting/SettingEntry.h"

UWidgetEnumSettingEntryBase::UWidgetEnumSettingEntryBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UWidgetEnumSettingEntryBase::OnSpawn_Implementation(const FParameter& InParam)
{
	Super::OnSpawn_Implementation(InParam);
}

void UWidgetEnumSettingEntryBase::OnDespawn_Implementation(EObjectDespawnMode InMode)
{
	if(ComboBox_Value)
	{
		ComboBox_Value->OnSelectionChanged.RemoveDynamic(this, &ThisClass::OnSelectionChanged);
		ComboBox_Value->ClearOptions();
	}
	Super::OnDespawn_Implementation(InMode);
}

void UWidgetEnumSettingEntryBase::SetSettingEntry(USettingEntry* InEntry)
{
	if(ComboBox_Value)
	{
		ComboBox_Value->OnSelectionChanged.RemoveDynamic(this, &ThisClass::OnSelectionChanged);
		ComboBox_Value->OnSelectionChanged.AddDynamic(this, &ThisClass::OnSelectionChanged);
		ComboBox_Value->ClearOptions();
		if(InEntry)
		{
			for(const FSettingOption& Option : InEntry->GetDefinition().Options)
			{
				ComboBox_Value->AddOption(Option.DisplayName.ToString());
			}
		}
	}
	Super::SetSettingEntry(InEntry);
}

void UWidgetEnumSettingEntryBase::ApplyValueToControl_Implementation(const FParameter& InValue)
{
	if(!ComboBox_Value || !SettingEntry)
	{
		return;
	}
	const int32 Index = SettingEntry->GetDefinition().Options.IndexOfByPredicate([&InValue](const FSettingOption& Option)
	{
		return Option.Value == InValue;
	});
	if(SettingEntry->GetDefinition().Options.IsValidIndex(Index))
	{
		ComboBox_Value->SetSelectedOption(SettingEntry->GetDefinition().Options[Index].DisplayName.ToString());
	}
}

void UWidgetEnumSettingEntryBase::OnSelectionChanged(FString InSelectedItem, ESelectInfo::Type InSelectionType)
{
	if(bRefreshingFromModel || !SettingEntry)
	{
		return;
	}
	if(const FSettingOption* Option = SettingEntry->GetDefinition().Options.FindByPredicate([&InSelectedItem](const FSettingOption& Item)
	{
		return Item.DisplayName.ToString() == InSelectedItem;
	}))
	{
		CommitUserValue(Option->Value);
	}
}
