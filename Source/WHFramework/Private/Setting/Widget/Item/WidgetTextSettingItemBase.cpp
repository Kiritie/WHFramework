// Fill out your copyright notice in the Description Item of Project Settings.

#include "Setting/Widget/Item/WidgetTextSettingItemBase.h"

#include "Components/EditableTextBox.h"
#include "Widget/WidgetModuleStatics.h"

UWidgetTextSettingItemBase::UWidgetTextSettingItemBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

void UWidgetTextSettingItemBase::OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams)
{
	Super::OnSpawn_Implementation(InOwner, InParams);
}

void UWidgetTextSettingItemBase::OnDespawn_Implementation(bool bRecovery)
{
	TxtBox_Value->KeyboardType = EVirtualKeyboardType::Default;

	Super::OnDespawn_Implementation(bRecovery);
}

void UWidgetTextSettingItemBase::OnCreate(UUserWidgetBase* InOwner, const TArray<FParameter>& InParams)
{
	Super::OnCreate(InOwner, InParams);

	TxtBox_Value->OnTextChanged.AddDynamic(this, &UWidgetTextSettingItemBase::OnTextBoxContentChanged);
}

void UWidgetTextSettingItemBase::OnInitialize(const TArray<FParameter>& InParams)
{
	Super::OnInitialize(InParams);

	if(InParams.IsValidIndex(1))
	{
		TxtBox_Value->KeyboardType = InParams[1].GetPointerValueRef<EVirtualKeyboardType::Type>();
	}
}

void UWidgetTextSettingItemBase::OnRefresh()
{
	Super::OnRefresh();
}

void UWidgetTextSettingItemBase::OnDestroy()
{
	Super::OnDestroy();
}

void UWidgetTextSettingItemBase::OnTextBoxContentChanged(const FText& InText)
{
	if(OnValueChanged.IsBound())
	{
		OnValueChanged.Broadcast(this, GetValue());
	}
	Refresh();
}

FParameter UWidgetTextSettingItemBase::GetValue() const
{
	FParameter Value;
	switch (TxtBox_Value->KeyboardType)
	{
		case EVirtualKeyboardType::Default:
		{
			Value = TxtBox_Value->GetText().ToString();
			break;
		}
		case EVirtualKeyboardType::Number:
		{
			Value = FCString::Atof(*TxtBox_Value->GetText().ToString());
			break;
		}
		default: break;
	}
	return Value;
}

void UWidgetTextSettingItemBase::SetValue(const FParameter& InValue)
{
	FText Text;
	switch (TxtBox_Value->KeyboardType)
	{
		case EVirtualKeyboardType::Default:
		{
			Text = FText::FromString(InValue.GetStringValue());
			break;
		}
		case EVirtualKeyboardType::Number:
		{
			Text = FText::FromString(FString::Printf(TEXT("%0.2f"), InValue.GetFloatValue()));
			break;
		}
		default: break;
	}
	TxtBox_Value->SetText(Text);
	Super::SetValue(InValue);
}
