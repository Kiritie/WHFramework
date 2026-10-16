#include "Widget/Common/CommonInput.h"

#include "Blueprint/WidgetTree.h"
#include "Widget/Style/CommonInputStyle.h"
#include "CommonNumericText.h"

UCommonInput::UCommonInput(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

EWidgetStyleKind UCommonInput::GetStyleKind() const
{
	return EWidgetStyleKind::Input;
}

void UCommonInput::BuildNativeLayout()
{
	TxtBox_Value = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("TxtBox_Value"));
	const bool bNumeric = ParameterValueType == FParameterIntValue::StaticStruct() || ParameterValueType == FParameterInt64Value::StaticStruct() ||
		ParameterValueType == FParameterFloatValue::StaticStruct() || ParameterValueType == FParameterDoubleValue::StaticStruct();
	TxtBox_Value->KeyboardType = bNumeric ? EVirtualKeyboardType::Number : EVirtualKeyboardType::Default;
	WidgetTree->RootWidget = TxtBox_Value;
}

void UCommonInput::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	if (TxtBox_Value)
	{
		TGuardValue<bool> Guard(bSynchronizingValue, true);
		TxtBox_Value->SetText(Text);
		TxtBox_Value->SetHintText(HintText);
		TxtBox_Value->SetIsReadOnly(bReadOnly);
		TxtBox_Value->SetIsPassword(bPassword);
		const bool bNumeric = ParameterValueType == FParameterIntValue::StaticStruct() || ParameterValueType == FParameterInt64Value::StaticStruct() ||
			ParameterValueType == FParameterFloatValue::StaticStruct() || ParameterValueType == FParameterDoubleValue::StaticStruct();
		TxtBox_Value->KeyboardType = bNumeric ? EVirtualKeyboardType::Number : EVirtualKeyboardType::Default;
	}
}

void UCommonInput::ApplyResolvedStyle(const UObject* InStyle)
{
	const UCommonInputStyle* Style = Cast<UCommonInputStyle>(InStyle);
	if (Style && TxtBox_Value)
	{
		TxtBox_Value->WidgetStyle = Style->InputStyle;
		if (!bUseStyleBackground)
		{
			TxtBox_Value->WidgetStyle.BackgroundImageNormal.DrawAs = ESlateBrushDrawType::NoDrawType;
			TxtBox_Value->WidgetStyle.BackgroundImageHovered.DrawAs = ESlateBrushDrawType::NoDrawType;
			TxtBox_Value->WidgetStyle.BackgroundImageFocused.DrawAs = ESlateBrushDrawType::NoDrawType;
			TxtBox_Value->WidgetStyle.BackgroundImageReadOnly.DrawAs = ESlateBrushDrawType::NoDrawType;
		}
		TxtBox_Value->SynchronizeProperties();
	}
}

void UCommonInput::BindControlEvents()
{
	if (TxtBox_Value)
	{
		TxtBox_Value->OnTextChanged.AddUniqueDynamic(this, &ThisClass::HandleTextChanged);
		TxtBox_Value->OnTextCommitted.AddUniqueDynamic(this, &ThisClass::HandleTextCommitted);
	}
}

void UCommonInput::UnbindControlEvents()
{
	if (TxtBox_Value)
	{
		TxtBox_Value->OnTextChanged.RemoveDynamic(this, &ThisClass::HandleTextChanged);
		TxtBox_Value->OnTextCommitted.RemoveDynamic(this, &ThisClass::HandleTextCommitted);
	}
}

void UCommonInput::OnDespawn_Implementation(EObjectDespawnMode InMode)
{
	Super::OnDespawn_Implementation(InMode);
	OnValueChanged.Clear();
	OnTextCommitted.Clear();
	ParameterValueType = nullptr;
	const UCommonInput* Defaults = CastChecked<UCommonInput>(GetArchetype());
	SetIsReadOnly(Defaults->bReadOnly);
	SetIsPassword(Defaults->bPassword);
	SetHintText(Defaults->HintText);
	SetText(Defaults->Text);
	SetUseStyleBackground(Defaults->bUseStyleBackground);
}

void UCommonInput::SetUseStyleBackground(bool bInUseStyleBackground)
{
	if (bUseStyleBackground != bInUseStyleBackground)
	{
		bUseStyleBackground = bInUseStyleBackground;
		RefreshStyle();
	}
}

void UCommonInput::HandleTextChanged(const FText& InText)
{
	Text = InText;
	if (!bSynchronizingValue)
	{
		FParameter Value;
		if (TryGetValue(Value))
		{
			OnValueChanged.Broadcast(this, Value);
		}
	}
}

void UCommonInput::HandleTextCommitted(const FText& InText, ETextCommit::Type InCommitMethod)
{
	Text = InText;
	if (!bSynchronizingValue)
	{
		OnTextCommitted.Broadcast(this, InText, InCommitMethod);
	}
}

FText UCommonInput::GetText() const
{
	return TxtBox_Value ? TxtBox_Value->GetText() : Text;
}

void UCommonInput::SetText(const FText& InText, bool bNotify)
{
	TGuardValue<bool> Guard(bSynchronizingValue, true);
	Text = InText;
	if (TxtBox_Value)
	{
		TxtBox_Value->SetText(InText);
	}
	if (bNotify)
	{
		FParameter Value;
		if (TryGetValue(Value))
		{
			OnValueChanged.Broadcast(this, Value);
		}
	}
}

FParameter UCommonInput::GetValue() const
{
	FParameter Value;
	TryGetValue(Value);
	return Value;
}

bool UCommonInput::TryGetValue(FParameter& OutValue) const
{
	OutValue.Reset();
	const FText Value = GetText();
	if (ParameterValueType == FParameterIntValue::StaticStruct())
	{
		int32 Number;
		if (!CommonNumericText::Parse(Value.ToString(), Number))
		{
			return false;
		}
		OutValue = Number;
	}
	else if (ParameterValueType == FParameterInt64Value::StaticStruct())
	{
		int64 Number;
		if (!CommonNumericText::Parse(Value.ToString(), Number))
		{
			return false;
		}
		OutValue = Number;
	}
	else if (ParameterValueType == FParameterFloatValue::StaticStruct())
	{
		float Number;
		if (!CommonNumericText::Parse(Value.ToString(), Number))
		{
			return false;
		}
		OutValue = Number;
	}
	else if (ParameterValueType == FParameterDoubleValue::StaticStruct())
	{
		double Number;
		if (!CommonNumericText::Parse(Value.ToString(), Number))
		{
			return false;
		}
		OutValue = Number;
	}
	else if (ParameterValueType == FParameterNameValue::StaticStruct())
	{
		OutValue = FName(*Value.ToString());
	}
	else if (ParameterValueType == FParameterTextValue::StaticStruct())
	{
		OutValue = Value;
	}
	else
	{
		OutValue = Value.ToString();
	}
	return true;
}

void UCommonInput::SetValue(const FParameter& InValue)
{
	ParameterValueType = const_cast<UScriptStruct*>(InValue.GetValueStruct());
	FText Value;
	bool bNumeric = true;
	if (const int32* IntValue = InValue.GetPtr<int32>())
	{
		Value = FText::FromString(FString::FromInt(*IntValue));
	}
	else if (const int64* Int64Value = InValue.GetPtr<int64>())
	{
		Value = FText::FromString(LexToString(*Int64Value));
	}
	else if (const float* FloatValue = InValue.GetPtr<float>())
	{
		Value = FText::FromString(LexToString(*FloatValue));
	}
	else if (const double* DoubleValue = InValue.GetPtr<double>())
	{
		Value = FText::FromString(LexToString(*DoubleValue));
	}
	else
	{
		bNumeric = false;
		if (const FText* ValueText = InValue.GetPtr<FText>())
		{
			Value = *ValueText;
		}
		else if (const FName* ValueName = InValue.GetPtr<FName>())
		{
			Value = FText::FromName(*ValueName);
		}
		else if (const FString* ValueString = InValue.GetPtr<FString>())
		{
			Value = FText::FromString(*ValueString);
		}
	}
	if (TxtBox_Value)
	{
		TxtBox_Value->KeyboardType = bNumeric ? EVirtualKeyboardType::Number : EVirtualKeyboardType::Default;
	}
	SetText(Value);
}

void UCommonInput::SetIsReadOnly(bool bInReadOnly)
{
	bReadOnly = bInReadOnly;
	if (TxtBox_Value)
	{
		TxtBox_Value->SetIsReadOnly(bReadOnly);
	}
}

void UCommonInput::SetIsPassword(bool bInPassword)
{
	bPassword = bInPassword;
	if (TxtBox_Value)
	{
		TxtBox_Value->SetIsPassword(bPassword);
	}
}

void UCommonInput::SetHintText(const FText& InHintText)
{
	HintText = InHintText;
	if (TxtBox_Value)
	{
		TxtBox_Value->SetHintText(HintText);
	}
}

void UCommonInput::SetInputFocus()
{
	if (TxtBox_Value)
	{
		TxtBox_Value->SetKeyboardFocus();
	}
}
