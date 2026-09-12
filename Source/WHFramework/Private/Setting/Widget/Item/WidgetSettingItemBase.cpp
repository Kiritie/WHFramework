// Fill out your copyright notice in the Description Item of Project Settings.


#include "Setting/Widget/Item/WidgetSettingItemBase.h"
#include "Setting/SettingModuleTypes.h"
#include "Widget/WidgetModuleStatics.h"

UWidgetSettingItemBase::UWidgetSettingItemBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	FParameter TitleParameter(FText::GetEmpty());
	TitleParameter.SetDescription(NSLOCTEXT("WH.WidgetSettingItemBase", "Title", "标题"));
	WidgetParams.Add(MoveTemp(TitleParameter));
}

void UWidgetSettingItemBase::OnSpawn_Implementation(const FParameter& InParam)
{
	Super::OnSpawn_Implementation(InParam);

	const FWidgetSettingItemSpawnParameter& Parameter = InParam.GetRef<FWidgetSettingItemSpawnParameter>();
	SetTitle(Parameter.Title);
}

void UWidgetSettingItemBase::OnDespawn_Implementation(EObjectDespawnMode InMode)
{
	Super::OnDespawn_Implementation(InMode);
	
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

void UWidgetSettingItemBase::SetVisible(bool bVisible)
{
	SetVisibility(bVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
}
