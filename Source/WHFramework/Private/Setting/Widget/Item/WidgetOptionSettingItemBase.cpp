// Fill out your copyright notice in the Description Item of Project Settings.

#include "Setting/Widget/Item/WidgetOptionSettingItemBase.h"

#include "Widget/WidgetModuleStatics.h"

UWidgetOptionSettingItemBase::UWidgetOptionSettingItemBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

void UWidgetOptionSettingItemBase::OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams)
{
	Super::OnSpawn_Implementation(InOwner, InParams);

	TxtBox_Value->OnTextChanged.AddDynamic(this, &UWidgetOptionSettingItemBase::OnTextBoxValueChanged);

	Btn_Last->OnClicked().AddUObject(this, &UWidgetOptionSettingItemBase::OnLastButtonClicked);
	Btn_Next->OnClicked().AddUObject(this, &UWidgetOptionSettingItemBase::OnNextButtonClicked);

	if(InParams.IsValidIndex(1))
	{
		OptionNames = InParams[1].GetPointerValueRef<TArray<FString>>();
	}
	SetOptionNames(OptionNames);
}

void UWidgetOptionSettingItemBase::OnDespawn_Implementation(bool bRecovery)
{
	Super::OnDespawn_Implementation(bRecovery);

	TxtBox_Value->OnTextChanged.RemoveDynamic(this, &UWidgetOptionSettingItemBase::OnTextBoxValueChanged);
	Btn_Last->OnClicked().RemoveAll(this);
	Btn_Next->OnClicked().RemoveAll(this);

	OptionNames.Empty();
}

void UWidgetOptionSettingItemBase::OnRefresh()
{
	Super::OnRefresh();
}

void UWidgetOptionSettingItemBase::OnTextBoxValueChanged(const FText& InText)
{
	if(!OptionNames.Contains(InText.ToString()) && OptionNames.IsValidIndex(0))
	{
		TxtBox_Value->SetText(FText::FromString(OptionNames[0]));
	}
	if(OnValueChanged.IsBound())
	{
		OnValueChanged.Broadcast(this, GetValue());
	}
	Refresh();
}

void UWidgetOptionSettingItemBase::OnLastButtonClicked()
{
	const int32 Index = OptionNames.Find(GetValue());
	if(Index > 0 && OptionNames.IsValidIndex(Index - 1))
	{
		SetValue(OptionNames[Index - 1]);
	}
}

void UWidgetOptionSettingItemBase::OnNextButtonClicked()
{
	const int32 Index = OptionNames.Find(GetValue());
	if(Index < OptionNames.Num() - 1 && OptionNames.IsValidIndex(Index + 1))
	{
		SetValue(OptionNames[Index + 1]);
	}
}

FParameter UWidgetOptionSettingItemBase::GetValue() const
{
	return TxtBox_Value->GetText().ToString();
}

void UWidgetOptionSettingItemBase::SetValue(const FParameter& InValue)
{
	FString Value = InValue;
	if(!OptionNames.Contains(Value) && OptionNames.IsValidIndex(0))
	{
		Value = OptionNames[0];
	}
	TxtBox_Value->SetText(FText::FromString(Value));
	
	const int32 Index = OptionNames.Find(GetValue());
	Btn_Last->SetIsEnabled(Index > 0);
	Btn_Next->SetIsEnabled(Index < OptionNames.Num() - 1);

	Super::SetValue(InValue);
}

void UWidgetOptionSettingItemBase::SetOptionNames(const TArray<FString>& InOptionNames)
{
	OptionNames = InOptionNames;
	if(OptionNames.IsValidIndex(0))
	{
		SetValue(OptionNames[0]);
	}
}
