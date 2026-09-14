#include "Setting/Widget/Entry/WidgetSettingEntryBase.h"

#include "Setting/SettingEntry.h"
#include "GameplayTagContainer.h"

UWidgetSettingEntryBase::UWidgetSettingEntryBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UWidgetSettingEntryBase::OnDespawn_Implementation(EObjectDespawnMode InMode)
{
	SettingEntry = nullptr;
	bRefreshingFromModel = false;

	Super::OnDespawn_Implementation(InMode);
}

void UWidgetSettingEntryBase::NativeOnListItemObjectSet(UObject* InListItemObject)
{
	IUserObjectListEntry::NativeOnListItemObjectSet(InListItemObject);
	SetSettingEntry(Cast<USettingEntry>(InListItemObject));
}

void UWidgetSettingEntryBase::SetSettingEntry(USettingEntry* InEntry)
{
	SettingEntry = InEntry;
	if(SettingEntry)
	{
		SetStyleTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Style.Button.Setting.Entry")), false));
		SetTitle(SettingEntry->GetDefinition().DisplayName);
		SetIsEnabled(SettingEntry->IsEnabled());
		SetVisibility(SettingEntry->IsVisible() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	RefreshFromModel();
}

void UWidgetSettingEntryBase::RefreshFromModel()
{
	if(!SettingEntry)
	{
		return;
	}

	TGuardValue<bool> Guard(bRefreshingFromModel, true);
	SetIsEnabled(SettingEntry->IsEnabled());
	SetVisibility(SettingEntry->IsVisible() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	ApplyValueToControl(SettingEntry->GetPendingValue());
}

void UWidgetSettingEntryBase::ApplyValueToControl_Implementation(const FParameter& InValue)
{
}

bool UWidgetSettingEntryBase::CommitUserValue(const FParameter& InValue)
{
	if(bRefreshingFromModel || !SettingEntry)
	{
		return false;
	}

	const bool bResult = SettingEntry->SetPendingValue(InValue);
	return bResult;
}
