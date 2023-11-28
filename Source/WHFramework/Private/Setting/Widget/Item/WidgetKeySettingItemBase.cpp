// Fill out your copyright notice in the Description Item of Project Settings.

#include "Setting/Widget/Item/WidgetKeySettingItemBase.h"

#include "Setting/Widget/Misc/WidgetPressAnyKeyPanelBase.h"
#include "Widget/WidgetModuleStatics.h"
#include "Widget/Common/CommonButton.h"

UWidgetKeySettingItemBase::UWidgetKeySettingItemBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

void UWidgetKeySettingItemBase::OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams)
{
	Super::OnSpawn_Implementation(InOwner, InParams);
}

void UWidgetKeySettingItemBase::OnDespawn_Implementation(bool bRecovery)
{
	Super::OnDespawn_Implementation(bRecovery);
}

void UWidgetKeySettingItemBase::OnCreate(UUserWidgetBase* InOwner, const TArray<FParameter>& InParams)
{
	Super::OnCreate(InOwner, InParams);

	for(int32 i = 0; i < Btn_Values.Num(); i++)
	{
		Btn_Values[i]->OnClicked().AddUObject(this, &UWidgetKeySettingItemBase::OnValueButtonClicked, i);
	}
}

void UWidgetKeySettingItemBase::OnInitialize(const TArray<FParameter>& InParams)
{
	Super::OnInitialize(InParams);
}

void UWidgetKeySettingItemBase::OnRefresh()
{
	Super::OnRefresh();
}

void UWidgetKeySettingItemBase::OnDestroy()
{
	Super::OnDestroy();
}

void UWidgetKeySettingItemBase::OnValueButtonClicked(int32 InIndex)
{
	UWidgetPressAnyKeyPanelBase* PressAnyKeyPanel = UWidgetModuleStatics::CreateUserWidget<UWidgetPressAnyKeyPanelBase>();
	PressAnyKeyPanel->OnKeySelected.AddUObject(this, &ThisClass::OnKeySelected, PressAnyKeyPanel);
	PressAnyKeyPanel->OnKeySelectionCanceled.AddUObject(this, &ThisClass::OnKeySelectionCanceled, PressAnyKeyPanel);
	PressAnyKeyPanel->Open({ InIndex });
}

void UWidgetKeySettingItemBase::OnKeySelected(FKey InKey, UWidgetPressAnyKeyPanelBase* InPressAnyKeyPanel)
{
	InPressAnyKeyPanel->OnKeySelected.RemoveAll(this);
	Btn_Values[InPressAnyKeyPanel->GetWidgetParams()[0].GetIntegerValue()]->SetTitle(FText::FromString(InKey.ToString()));
	if(OnValuesChanged.IsBound())
	{
		OnValuesChanged.Broadcast(this, GetValues());
	}
	Refresh();
}

void UWidgetKeySettingItemBase::OnKeySelectionCanceled(UWidgetPressAnyKeyPanelBase* InPressAnyKeyPanel)
{
	InPressAnyKeyPanel->OnKeySelectionCanceled.RemoveAll(this);
}

TArray<FParameter> UWidgetKeySettingItemBase::GetValues() const
{
	TArray<FParameter> Values;
	for(int32 i = 0; i < Btn_Values.Num(); i++)
	{
		Values.Add(Btn_Values[i]->GetTitle().ToString());
	}
	return Values;
}

void UWidgetKeySettingItemBase::SetValues(const TArray<FParameter>& InValues)
{
	for(int32 i = 0; i < InValues.Num(); i++)
	{
		Btn_Values[i]->SetTitle(FText::FromString(InValues[i].GetStringValue()));
	}
	Super::SetValues(InValues);
}
