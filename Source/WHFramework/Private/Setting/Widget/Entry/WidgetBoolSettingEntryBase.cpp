#include "Setting/Widget/Entry/WidgetBoolSettingEntryBase.h"

#include "Widget/Common/CommonButton.h"

UWidgetBoolSettingEntryBase::UWidgetBoolSettingEntryBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UWidgetBoolSettingEntryBase::OnSpawn_Implementation(const FParameter& InParam)
{
	Super::OnSpawn_Implementation(InParam);
}

void UWidgetBoolSettingEntryBase::OnDespawn_Implementation(EObjectDespawnMode InMode)
{
	if(Btn_Value)
	{
		Btn_Value->SetStyleTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Style.Button.Setting.Toggle")), false));
		Btn_Value->OnIsSelectedChanged().RemoveAll(this);
	}
	Super::OnDespawn_Implementation(InMode);
}

void UWidgetBoolSettingEntryBase::ApplyValueToControl_Implementation(const FParameter& InValue)
{
	if(Btn_Value)
	{
		Btn_Value->OnIsSelectedChanged().RemoveAll(this);
		Btn_Value->OnIsSelectedChanged().AddUObject(this, &ThisClass::OnValueChanged);
		Btn_Value->SetIsSelected(InValue.Get<bool>());
	}
}

void UWidgetBoolSettingEntryBase::OnValueChanged(bool bInSelected)
{
	CommitUserValue(FParameter(bInSelected));
}
