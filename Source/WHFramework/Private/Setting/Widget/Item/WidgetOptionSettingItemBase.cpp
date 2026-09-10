// Fill out your copyright notice in the Description Item of Project Settings.

#include "Setting/Widget/Item/WidgetOptionSettingItemBase.h"

#include "CommonTextBlock.h"
#include "Widget/WidgetModuleStatics.h"

UWidgetOptionSettingItemBase::UWidgetOptionSettingItemBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bEditable = false;
}

void UWidgetOptionSettingItemBase::OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams)
{
	Super::OnSpawn_Implementation(InOwner, InParams);

	TxtBox_Value->OnTextChanged.AddDynamic(this, &UWidgetOptionSettingItemBase::OnTextBoxValueChanged);

	Btn_Last->OnClicked().AddUObject(this, &UWidgetOptionSettingItemBase::OnLastButtonClicked);
	Btn_Next->OnClicked().AddUObject(this, &UWidgetOptionSettingItemBase::OnNextButtonClicked);

	if(InParams.IsValidIndex(1))
	{
		if(const FStringArrayParameterValue* Value = InParams[1].GetStructPtr<FStringArrayParameterValue>())
		{
			OptionNames = Value->Value;
		}
	}
	if(InParams.IsValidIndex(2))
	{
		bEditable = InParams[2].Get<bool>();
	}

	SetOptionNames(OptionNames);

	TxtBox_Value->SetVisibility(bEditable ? ESlateVisibility::Visible : ESlateVisibility::HitTestInvisible);
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

void UWidgetOptionSettingItemBase::NativeOnCurrentTextStyleChanged()
{
	Super::NativeOnCurrentTextStyleChanged();

	if(!TxtBox_Value)
	{
		return;
	}

	const TSubclassOf<UCommonTextStyle> CurrentTextStyleClass = GetCurrentTextStyleClass();
	if(!CurrentTextStyleClass)
	{
		return;
	}

	const UCommonTextStyle* CurrentTextStyle = CurrentTextStyleClass->GetDefaultObject<UCommonTextStyle>();
	FSlateFontInfo Font;
	FLinearColor Color;
	CurrentTextStyle->GetFont(Font);
	CurrentTextStyle->GetColor(Color);

	FEditableTextBoxStyle WidgetStyle = TxtBox_Value->GetWidgetStyle();
	WidgetStyle.TextStyle.Font = Font;
	WidgetStyle.TextStyle.ColorAndOpacity = FSlateColor(Color);

	TxtBox_Value->WidgetStyle = MoveTemp(WidgetStyle);
	TxtBox_Value->SynchronizeProperties();
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
