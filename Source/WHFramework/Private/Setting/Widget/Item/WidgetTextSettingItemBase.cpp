// Fill out your copyright notice in the Description Item of Project Settings.

#include "Setting/Widget/Item/WidgetTextSettingItemBase.h"

#include "Components/EditableTextBox.h"
#include "Widget/WidgetModuleStatics.h"

UWidgetTextSettingItemBase::UWidgetTextSettingItemBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	ParameterType = EParameterType::None;
}

void UWidgetTextSettingItemBase::OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams)
{
	Super::OnSpawn_Implementation(InOwner, InParams);
}

void UWidgetTextSettingItemBase::OnDespawn_Implementation(bool bRecovery)
{
	TxtBox_Value->KeyboardType = EVirtualKeyboardType::Default;

	ParameterType = EParameterType::None;

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
}

void UWidgetTextSettingItemBase::OnRefresh()
{
	Super::OnRefresh();
}

void UWidgetTextSettingItemBase::OnDestroy(bool bRecovery)
{
	Super::OnDestroy(bRecovery);
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
	switch (ParameterType)
	{
		case EParameterType::Integer:
		case EParameterType::Float:
		{
			Value = FCString::Atof(*TxtBox_Value->GetText().ToString());
			break;
		}
		case EParameterType::String:
		case EParameterType::Name:
		case EParameterType::Text:
		{
			Value = TxtBox_Value->GetText().ToString();
			break;
		}
		default: break;
	}
	return Value;
}

void UWidgetTextSettingItemBase::SetValue(const FParameter& InValue)
{
	ParameterType = InValue.GetParameterType();
	FString Text;
	switch (ParameterType)
	{
		case EParameterType::Integer:
		{
			Text = FString::FromInt(InValue.GetIntegerValue());
			TxtBox_Value->KeyboardType = EVirtualKeyboardType::Number;
			break;
		}
		case EParameterType::Float:
		{
			Text = FString::Printf(TEXT("%0.2f"), InValue.GetFloatValue());
			TxtBox_Value->KeyboardType = EVirtualKeyboardType::Number;
			break;
		}
		case EParameterType::String:
		{
			Text = InValue.GetStringValue();
			TxtBox_Value->KeyboardType = EVirtualKeyboardType::Default;
			break;
		}
		case EParameterType::Name:
		{
			Text = InValue.GetNameValue().ToString();
			TxtBox_Value->KeyboardType = EVirtualKeyboardType::Default;
			break;
		}
		case EParameterType::Text:
		{
			Text = InValue.GetTextValue().ToString();
			TxtBox_Value->KeyboardType = EVirtualKeyboardType::Default;
			break;
		}
		default: break;
	}
	TxtBox_Value->SetText(FText::FromString(Text));
	Super::SetValue(InValue);
}
