// Fill out your copyright notice in the Description Item of Project Settings.

#include "Setting/Widget/Item/WidgetBoolSettingItemBase.h"

#include "Components/CheckBox.h"
#include "Widget/WidgetModuleStatics.h"

UWidgetBoolSettingItemBase::UWidgetBoolSettingItemBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

void UWidgetBoolSettingItemBase::OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams)
{
	Super::OnSpawn_Implementation(InOwner, InParams);
}

void UWidgetBoolSettingItemBase::OnDespawn_Implementation(bool bRecovery)
{
	Super::OnDespawn_Implementation(bRecovery);
}

void UWidgetBoolSettingItemBase::OnCreate(UUserWidgetBase* InOwner, const TArray<FParameter>& InParams)
{
	Super::OnCreate(InOwner, InParams);
	
	CheckBox_Value->OnCheckStateChanged.AddDynamic(this, &UWidgetBoolSettingItemBase::OnCheckBoxStateChanged);
}

void UWidgetBoolSettingItemBase::OnInitialize(const TArray<FParameter>& InParams)
{
	Super::OnInitialize(InParams);
}

void UWidgetBoolSettingItemBase::OnRefresh()
{
	Super::OnRefresh();
}

void UWidgetBoolSettingItemBase::OnDestroy()
{
	Super::OnDestroy();
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
	return CheckBox_Value->IsChecked();
}

void UWidgetBoolSettingItemBase::SetValue(const FParameter& InValue)
{
	CheckBox_Value->SetIsChecked(InValue.GetBooleanValue());
	Super::SetValue(InValue);
}
