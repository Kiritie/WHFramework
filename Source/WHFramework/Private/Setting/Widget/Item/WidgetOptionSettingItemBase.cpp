// Fill out your copyright notice in the Description Item of Project Settings.

#include "Setting/Widget/Item/WidgetOptionSettingItemBase.h"

#include "Components/EditableTextBox.h"
#include "Setting/SettingModuleTypes.h"
#include "Widget/WidgetModuleStatics.h"

UWidgetOptionSettingItemBase::UWidgetOptionSettingItemBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bEditable = false;
}

void UWidgetOptionSettingItemBase::OnSpawn_Implementation(const FParameter& InParam)
{
	Super::OnSpawn_Implementation(InParam);

	TxtBox_Value->OnTextChanged.AddDynamic(this, &UWidgetOptionSettingItemBase::OnTextBoxValueChanged);

	Btn_Last->OnClicked().AddUObject(this, &UWidgetOptionSettingItemBase::OnLastButtonClicked);
	Btn_Next->OnClicked().AddUObject(this, &UWidgetOptionSettingItemBase::OnNextButtonClicked);

	const FWidgetOptionSettingItemSpawnParameter& Parameter = InParam.GetRef<FWidgetOptionSettingItemSpawnParameter>();
	OptionNames = Parameter.Options;
	bEditable = Parameter.bEditable;

	SetOptionNames(OptionNames);

	TxtBox_Value->SetVisibility(bEditable ? ESlateVisibility::Visible : ESlateVisibility::HitTestInvisible);
}

void UWidgetOptionSettingItemBase::OnDespawn_Implementation(EObjectDespawnMode InMode)
{
	Super::OnDespawn_Implementation(InMode);

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
	const int32 Index = OptionNames.Find(GetValue().Get<FString>());
	if(Index > 0 && OptionNames.IsValidIndex(Index - 1))
	{
		SetValue(OptionNames[Index - 1]);
	}
}

void UWidgetOptionSettingItemBase::OnNextButtonClicked()
{
	const int32 Index = OptionNames.Find(GetValue().Get<FString>());
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
	FString Value = InValue.Get<FString>();
	if(!OptionNames.Contains(Value) && OptionNames.IsValidIndex(0))
	{
		Value = OptionNames[0];
	}
	TxtBox_Value->SetText(FText::FromString(Value));
	
	const int32 Index = OptionNames.Find(Value);
	Btn_Last->SetIsEnabledN(Index > 0);
	Btn_Next->SetIsEnabledN(Index < OptionNames.Num() - 1);

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

int32 UWidgetOptionSettingItemBase::GetOptionIndex() const
{
	return OptionNames.Find(GetValue().Get<FString>());
}

void UWidgetOptionSettingItemBase::SetOptionIndex(int32 InOptionIndex)
{
	if(OptionNames.IsValidIndex(InOptionIndex))
	{
		SetValue(OptionNames[InOptionIndex]);
	}
}
