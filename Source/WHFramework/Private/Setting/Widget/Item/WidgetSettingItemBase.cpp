// Fill out your copyright notice in the Description Item of Project Settings.


#include "Setting/Widget/Item/WidgetSettingItemBase.h"
#include "Widget/WidgetModuleStatics.h"

UWidgetSettingItemBase::UWidgetSettingItemBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

void UWidgetSettingItemBase::OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams)
{
	Super::OnSpawn_Implementation(InOwner, InParams);;

	if(InParams.IsValidIndex(0))
	{
		SetTitle(InParams[0]);
	}
}

void UWidgetSettingItemBase::OnDespawn_Implementation(bool bRecovery)
{
	Super::OnDespawn_Implementation(bRecovery);
	
	SettingName = NAME_None;
	SetValue(FParameter());
	OnValueChanged.Clear();
	OnValuesChanged.Clear();
}

void UWidgetSettingItemBase::OnRefresh()
{
}

void UWidgetSettingItemBase::Refresh()
{
	OnRefresh();
}

FParameter UWidgetSettingItemBase::GetValue() const
{
	return FParameter();
}

void UWidgetSettingItemBase::SetValue(const FParameter& InValue)
{
	if(OnValueChanged.IsBound())
	{
		OnValueChanged.Broadcast(this, InValue);
	}
	Refresh();
}

TArray<FParameter> UWidgetSettingItemBase::GetValues() const
{
	return TArray<FParameter>();
}

void UWidgetSettingItemBase::SetValues(const TArray<FParameter>& InValues)
{
	if(OnValuesChanged.IsBound())
	{
		OnValuesChanged.Broadcast(this, InValues);
	}
	Refresh();
}
