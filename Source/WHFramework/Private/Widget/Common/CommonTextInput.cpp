#include "Widget/Common/CommonTextInput.h"

#include "Components/EditableTextBox.h"

UCommonTextInput::UCommonTextInput(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UCommonTextInput::OnSpawn_Implementation(const FParameter& InParam)
{
	Super::OnSpawn_Implementation(InParam);

	if(TxtBox_Value)
	{
		TxtBox_Value->OnTextChanged.RemoveDynamic(this, &ThisClass::OnTextValueChanged);
		TxtBox_Value->OnTextChanged.AddDynamic(this, &ThisClass::OnTextValueChanged);
	}
}

void UCommonTextInput::OnDespawn_Implementation(EObjectDespawnMode InMode)
{
	if(TxtBox_Value)
	{
		TxtBox_Value->KeyboardType = EVirtualKeyboardType::Default;
		TxtBox_Value->OnTextChanged.RemoveDynamic(this, &ThisClass::OnTextValueChanged);
	}
	ParameterValueType = nullptr;
	OnValueChanged.Clear();

	Super::OnDespawn_Implementation(InMode);
}

void UCommonTextInput::OnTextValueChanged(const FText& InText)
{
	OnValueChanged.Broadcast(this, GetValue());
}

FParameter UCommonTextInput::GetValue() const
{
	FParameter Value;
	if(!TxtBox_Value)
	{
		return Value;
	}
	const FString Text = TxtBox_Value->GetText().ToString();
	if(ParameterValueType == FParameterIntValue::StaticStruct())
	{
		Value = FCString::Atoi(*Text);
	}
	else if(ParameterValueType == FParameterFloatValue::StaticStruct())
	{
		Value = FCString::Atof(*Text);
	}
	else if(ParameterValueType == FParameterNameValue::StaticStruct())
	{
		Value = FName(*Text);
	}
	else if(ParameterValueType == FParameterTextValue::StaticStruct())
	{
		Value = FText::FromString(Text);
	}
	else
	{
		Value = Text;
	}
	return Value;
}

void UCommonTextInput::SetValue(const FParameter& InValue)
{
	ParameterValueType = const_cast<UScriptStruct*>(InValue.GetValueStruct());
	FString Text;
	EVirtualKeyboardType::Type KeyboardType = EVirtualKeyboardType::Default;
	if(const int32* IntValue = InValue.GetPtr<int32>())
	{
		Text = FString::FromInt(*IntValue);
		KeyboardType = EVirtualKeyboardType::Number;
	}
	else if(const float* FloatValue = InValue.GetPtr<float>())
	{
		Text = FString::Printf(TEXT("%0.2f"), *FloatValue);
		KeyboardType = EVirtualKeyboardType::Number;
	}
	else if(const FString* StringValue = InValue.GetPtr<FString>())
	{
		Text = *StringValue;
	}
	else if(const FName* NameValue = InValue.GetPtr<FName>())
	{
		Text = NameValue->ToString();
	}
	else if(const FText* TextValue = InValue.GetPtr<FText>())
	{
		Text = TextValue->ToString();
	}
	if(TxtBox_Value)
	{
		TxtBox_Value->KeyboardType = KeyboardType;
		TxtBox_Value->SetText(FText::FromString(Text));
	}
}
