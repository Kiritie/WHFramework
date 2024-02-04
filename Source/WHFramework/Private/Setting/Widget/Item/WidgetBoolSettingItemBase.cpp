// Fill out your copyright notice in the Description Item of Project Settings.

#include "Setting/Widget/Item/WidgetBoolSettingItemBase.h"

#include "Widget/WidgetModuleStatics.h"

UWidgetBoolSettingItemBase::UWidgetBoolSettingItemBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

void UWidgetBoolSettingItemBase::OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams)
{
	Super::OnSpawn_Implementation(InOwner, InParams);

	Btn_Value->OnIsSelectedChanged().AddUObject(this, &UWidgetBoolSettingItemBase::OnCheckBoxStateChanged);
}

void UWidgetBoolSettingItemBase::OnDespawn_Implementation(bool bRecovery)
{
	Super::OnDespawn_Implementation(bRecovery);
	
	Btn_Value->OnIsSelectedChanged().RemoveAll(this);
}

void UWidgetBoolSettingItemBase::OnRefresh()
{
	Super::OnRefresh();
}

void UWidgetBoolSettingItemBase::OnCheckBoxStateChanged(bool bIsChecked)
{
	if(OnValueChanged.IsBound())
	{
		OnValueChanged.Broadcast(this, GetValue());
	}
	Refresh();
}

FParameter UWidgetBoolSettingItemBase::GetValue() const
{
	return Btn_Value->GetSelected();
}

void UWidgetBoolSettingItemBase::SetValue(const FParameter& InValue)
{
	Btn_Value->SetIsSelected(InValue.GetBooleanValue());
	Super::SetValue(InValue);
}
