#include "Setting/Widget/Entry/WidgetSettingEntryBase.h"

#include "Setting/SettingEntry.h"
#include "GameplayTagContainer.h"
#include "Widget/Common/CommonInput.h"
#include "Widget/Common/CommonOptionSelector.h"
#include "Widget/Common/CommonToggle.h"
#include "Widget/Common/CommonRange.h"
#include "Widget/Common/CommonTextBlockN.h"
#include "Widget/Style/CommonButtonStyleN.h"

UWidgetSettingEntryBase::UWidgetSettingEntryBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UWidgetSettingEntryBase::OnDespawn_Implementation(EObjectDespawnMode InMode)
{
	SetSettingEntry(nullptr);

	Super::OnDespawn_Implementation(InMode);
}

void UWidgetSettingEntryBase::NativeConstruct()
{
	Super::NativeConstruct();
	BindValueControl();
	RefreshFromModel();
}

void UWidgetSettingEntryBase::NativeDestruct()
{
	UnbindValueControl();
	Super::NativeDestruct();
}

void UWidgetSettingEntryBase::NativeOnEntryReleased()
{
	SetSettingEntry(nullptr);
	IUserListEntry::NativeOnEntryReleased();
}

void UWidgetSettingEntryBase::NativeOnListItemObjectSet(UObject* InListItemObject)
{
	SetSettingEntry(Cast<USettingEntry>(InListItemObject));
	IUserObjectListEntry::NativeOnListItemObjectSet(InListItemObject);
}

void UWidgetSettingEntryBase::SetSettingEntry(USettingEntry* InEntry)
{
	if (SettingEntry == InEntry) return;
	UnbindValueControl();
	SettingEntry = InEntry;
	if(SettingEntry)
	{
		SetTitle(SettingEntry->GetDefinition().DisplayName);
		ConfigureValueControl();
		BindValueControl();
	}
	else
	{
		SetTitle(FText::GetEmpty());
		SetIsEnabledN(false);
		SetVisibility(ESlateVisibility::Collapsed);
		if (Txt_Reason)
		{
			Txt_Reason->SetText(FText::GetEmpty());
		}
	}
	RefreshFromModel();
}

void UWidgetSettingEntryBase::RefreshFromModel()
{
	RefreshModelState();
	if (SettingEntry)
	{
		TGuardValue<bool> Guard(bRefreshingFromModel, true);
		ApplyValueToControl(SettingEntry->GetPendingValue());
	}
}

void UWidgetSettingEntryBase::RefreshModelState()
{
	if(!SettingEntry)
	{
		return;
	}

	TGuardValue<bool> Guard(bRefreshingFromModel, true);
	SetIsEnabledN(SettingEntry->IsEnabled());
	SetVisibility(SettingEntry->IsVisible() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	const FSettingValidationResult Validation = SettingEntry->GetValidationResult();
	SetToolTipText(Validation.bValid ? SettingEntry->GetDefinition().Description : Validation.Error);
	if (Txt_Reason)
	{
		Txt_Reason->SetText(Validation.bValid ? FText::GetEmpty() : Validation.Error);
		Txt_Reason->SetVisibility(Validation.bValid ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}
	OnModelStateChanged();
}

void UWidgetSettingEntryBase::ApplyValueToControl_Implementation(const FParameter& InValue)
{
}

bool UWidgetSettingEntryBase::CommitUserValue(const FParameter& InValue)
{
	if(bRefreshingFromModel || !SettingEntry || !SettingEntry->IsEnabled() || !SettingEntry->IsVisible())
	{
		return false;
	}

	USettingEntry* Model = SettingEntry;
	const bool bResult = Model->SetPendingValue(InValue);
	if (SettingEntry == Model)
	{
		RefreshFromModel();
	}
	return bResult;
}

void UWidgetSettingEntryBase::OnResolvedStyleChanged(const UCommonButtonStyleN* InStyle)
{
	Super::OnResolvedStyleChanged(InStyle);
	if (!InStyle)
	{
		return;
	}
	UWidget* Control = GetValueControl();
	const FCommonControlStyleTags& Styles = InStyle->ControlStyles;
	if (UCommonOptionSelector* Option = Cast<UCommonOptionSelector>(Control))
	{
		Option->SetInheritedStyleTag(Styles.ValueOptionStyleTag);
	}
	else if (UCommonInput* Input = Cast<UCommonInput>(Control))
	{
		Input->SetInheritedStyleTag(Styles.ValueInputStyleTag);
	}
	else if (UCommonToggle* Toggle = Cast<UCommonToggle>(Control))
	{
		Toggle->SetInheritedStyleTag(Styles.ValueToggleStyleTag);
	}
	else if (UCommonRange* Range = Cast<UCommonRange>(Control))
	{
		Range->SetInheritedStyleTag(Styles.ValueRangeStyleTag);
	}
	else if (UCommonButton* Button = Cast<UCommonButton>(Control))
	{
		Button->SetInheritedStyleTag(Styles.ValueButtonStyleTag);
	}
	if (Txt_Reason)
	{
		Txt_Reason->SetStyleTag(Styles.ReasonTextStyleTag);
	}
}

UWidget* UWidgetSettingEntryBase::GetValueControl() const
{
	return nullptr;
}

FName UWidgetSettingEntryBase::GetRendererName() const
{
	return TEXT("Custom");
}

void UWidgetSettingEntryBase::BindValueControl()
{
}

void UWidgetSettingEntryBase::UnbindValueControl()
{
}

void UWidgetSettingEntryBase::ConfigureValueControl()
{
}

void UWidgetSettingEntryBase::OnModelStateChanged()
{
}
