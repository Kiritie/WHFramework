// Fill out your copyright notice in the Description Item of Project Settings.

#include "Setting/Widget/Item/WidgetTextSettingItemBase.h"

#include "Components/EditableTextBox.h"
#include "Widget/WidgetModuleStatics.h"

UWidgetTextSettingItemBase::UWidgetTextSettingItemBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	ParameterValueType = nullptr;
}

void UWidgetTextSettingItemBase::OnSpawn_Implementation(const FParameter& InParam)
{
	Super::OnSpawn_Implementation(InParam);

	TxtBox_Value->OnTextChanged.AddDynamic(this, &UWidgetTextSettingItemBase::OnTextBoxValueChanged);
}

void UWidgetTextSettingItemBase::OnDespawn_Implementation(EObjectDespawnMode InMode)
{
	TxtBox_Value->KeyboardType = EVirtualKeyboardType::Default;

	ParameterValueType = nullptr;

	TxtBox_Value->OnTextChanged.RemoveDynamic(this, &UWidgetTextSettingItemBase::OnTextBoxValueChanged);

	Super::OnDespawn_Implementation(InMode);
}

void UWidgetTextSettingItemBase::OnRefresh()
{
	Super::OnRefresh();
}

void UWidgetTextSettingItemBase::OnTextBoxValueChanged(const FText& InText)
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
	const FString Text = TxtBox_Value->GetText().ToString();
	if (ParameterValueType == FParameterIntValue::StaticStruct())
	{
		Value = FCString::Atoi(*Text);
	}
	else if (ParameterValueType == FParameterFloatValue::StaticStruct())
	{
		Value = FCString::Atof(*Text);
	}
	else if (ParameterValueType == FParameterStringValue::StaticStruct())
	{
		Value = Text;
	}
	else if (ParameterValueType == FParameterNameValue::StaticStruct())
	{
		Value = FName(*Text);
	}
	else if (ParameterValueType == FParameterTextValue::StaticStruct())
	{
		Value = FText::FromString(Text);
	}
	return Value;
}

void UWidgetTextSettingItemBase::SetValue(const FParameter& InValue)
{
	ParameterValueType = const_cast<UScriptStruct*>(InValue.GetValueStruct());
	FString Text;
	if (InValue.Is<int32>())
	{
		Text = FString::FromInt(InValue.Get<int32>());
		TxtBox_Value->KeyboardType = EVirtualKeyboardType::Number;
	}
	else if (InValue.Is<float>())
	{
		Text = FString::Printf(TEXT("%0.2f"), InValue.Get<float>());
		TxtBox_Value->KeyboardType = EVirtualKeyboardType::Number;
	}
	else if (InValue.Is<FString>())
	{
		Text = InValue.Get<FString>();
		TxtBox_Value->KeyboardType = EVirtualKeyboardType::Default;
	}
	else if (InValue.Is<FName>())
	{
		Text = InValue.Get<FName>().ToString();
		TxtBox_Value->KeyboardType = EVirtualKeyboardType::Default;
	}
	else if (InValue.Is<FText>())
	{
		Text = InValue.Get<FText>().ToString();
		TxtBox_Value->KeyboardType = EVirtualKeyboardType::Default;
	}
	TxtBox_Value->SetText(FText::FromString(Text));
	Super::SetValue(InValue);
}
