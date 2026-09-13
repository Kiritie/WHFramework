#include "Setting/Widget/Entry/WidgetOptionSettingEntryBase.h"

#include "Components/EditableTextBox.h"
#include "Setting/SettingEntry.h"
#include "Widget/Common/CommonButton.h"

UWidgetOptionSettingEntryBase::UWidgetOptionSettingEntryBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UWidgetOptionSettingEntryBase::OnSpawn_Implementation(const FParameter& InParam)
{
	Super::OnSpawn_Implementation(InParam);
}

void UWidgetOptionSettingEntryBase::OnDespawn_Implementation(EObjectDespawnMode InMode)
{
	if(Btn_Last)
	{
		Btn_Last->SetStyleTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Style.Button.Setting.Previous")), false));
		Btn_Last->OnClicked().RemoveAll(this);
	}
	if(Btn_Next)
	{
		Btn_Next->SetStyleTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Style.Button.Setting.Next")), false));
		Btn_Next->OnClicked().RemoveAll(this);
	}
	Super::OnDespawn_Implementation(InMode);
}

void UWidgetOptionSettingEntryBase::ApplyValueToControl_Implementation(const FParameter& InValue)
{
	if(!SettingEntry)
	{
		return;
	}
	const int32 Index = GetCurrentOptionIndex();
	const TArray<FSettingOption>& Options = SettingEntry->GetDefinition().Options;
	if(Btn_Last)
	{
		Btn_Last->OnClicked().RemoveAll(this);
		Btn_Last->OnClicked().AddUObject(this, &ThisClass::OnPreviousClicked);
	}
	if(Btn_Next)
	{
		Btn_Next->OnClicked().RemoveAll(this);
		Btn_Next->OnClicked().AddUObject(this, &ThisClass::OnNextClicked);
	}
	if(TxtBox_Value)
	{
		TxtBox_Value->SetIsReadOnly(true);
		TxtBox_Value->SetText(Options.IsValidIndex(Index) ? Options[Index].DisplayName : FText::GetEmpty());
	}
	if(Btn_Last)
	{
		Btn_Last->SetIsEnabled(Index > 0);
	}
	if(Btn_Next)
	{
		Btn_Next->SetIsEnabled(Index != INDEX_NONE && Index < Options.Num() - 1);
	}
}

void UWidgetOptionSettingEntryBase::OnPreviousClicked()
{
	if(!SettingEntry)
	{
		return;
	}
	const int32 Index = GetCurrentOptionIndex();
	if(SettingEntry->GetDefinition().Options.IsValidIndex(Index - 1))
	{
		CommitUserValue(SettingEntry->GetDefinition().Options[Index - 1].Value);
	}
}

void UWidgetOptionSettingEntryBase::OnNextClicked()
{
	if(!SettingEntry)
	{
		return;
	}
	const int32 Index = GetCurrentOptionIndex();
	if(SettingEntry->GetDefinition().Options.IsValidIndex(Index + 1))
	{
		CommitUserValue(SettingEntry->GetDefinition().Options[Index + 1].Value);
	}
}

int32 UWidgetOptionSettingEntryBase::GetCurrentOptionIndex() const
{
	if(!SettingEntry)
	{
		return INDEX_NONE;
	}
	const FParameter CurrentValue = SettingEntry->GetPendingValue();
	return SettingEntry->GetDefinition().Options.IndexOfByPredicate([&CurrentValue](const FSettingOption& Option)
	{
		return Option.Value == CurrentValue;
	});
}
