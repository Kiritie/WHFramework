// Fill out your copyright notice in the Description Item of Project Settings.

#include "Setting/Widget/Item/WidgetFloatSettingItemBase.h"

#include "Components/EditableTextBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Kismet/KismetTextLibrary.h"
#include "Widget/WidgetModuleStatics.h"

UWidgetFloatSettingItemBase::UWidgetFloatSettingItemBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	MinValue = 0.f;
	MaxValue = 1.f;

	DecimalNum = 2;
	ScaleFactor = 1.f;
}

void UWidgetFloatSettingItemBase::OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams)
{
	Super::OnSpawn_Implementation(InOwner, InParams);

	Slider_Value->OnValueChanged.AddDynamic(this, &UWidgetFloatSettingItemBase::OnSliderValueChanged);
	if(TxtBox_Value)
	{
		TxtBox_Value->OnTextCommitted.AddDynamic(this, &UWidgetFloatSettingItemBase::OnTextBoxValueCommitted);
	}

	if(InParams.IsValidIndex(1))
	{
		MinValue = InParams[1].GetFloatValue();
	}

	if(InParams.IsValidIndex(2))
	{
		MaxValue = InParams[2].GetFloatValue();
	}

	if(InParams.IsValidIndex(3))
	{
		DecimalNum = InParams[3].GetIntegerValue();
	}

	if(InParams.IsValidIndex(4))
	{
		ScaleFactor = InParams[4].GetFloatValue();
	}

	if(Txt_MinValue)
	{
		Txt_MinValue->SetText(UKismetTextLibrary::Conv_DoubleToText(MinValue * ScaleFactor, ERoundingMode::HalfToEven, false, false, 1, 324, 0, DecimalNum));
	}
	if(Txt_MaxValue)
	{
		Txt_MaxValue->SetText(UKismetTextLibrary::Conv_DoubleToText(MaxValue * ScaleFactor, ERoundingMode::HalfToEven, false, false, 1, 324, 0, DecimalNum));
	}
}

void UWidgetFloatSettingItemBase::OnDespawn_Implementation(bool bRecovery)
{
	Super::OnDespawn_Implementation(bRecovery);

	MinValue = 0.f;
	MaxValue = 1.f;

	DecimalNum = 2;
	ScaleFactor = 1.f;

	Slider_Value->OnValueChanged.RemoveDynamic(this, &UWidgetFloatSettingItemBase::OnSliderValueChanged);
	if(TxtBox_Value)
	{
		TxtBox_Value->OnTextCommitted.RemoveDynamic(this, &UWidgetFloatSettingItemBase::OnTextBoxValueCommitted);
	}
}

void UWidgetFloatSettingItemBase::OnRefresh()
{
	Super::OnRefresh();
}

void UWidgetFloatSettingItemBase::OnSliderValueChanged(float InValue)
{
	if(Txt_Value)
	{
		Txt_Value->SetText(UKismetTextLibrary::Conv_DoubleToText(FMath::Lerp(MinValue * ScaleFactor, MaxValue * ScaleFactor, InValue), ERoundingMode::HalfToEven, false, false, 1, 324, 0, DecimalNum));
	}
	if(TxtBox_Value)
	{
		TxtBox_Value->SetText(UKismetTextLibrary::Conv_DoubleToText(FMath::Lerp(MinValue * ScaleFactor, MaxValue * ScaleFactor, InValue), ERoundingMode::HalfToEven, false, false, 1, 324, 0, DecimalNum));
	}
	if(OnValueChanged.IsBound())
	{
		OnValueChanged.Broadcast(this, GetValue());
	}
}

void UWidgetFloatSettingItemBase::OnTextBoxValueCommitted(const FText& InText, ETextCommit::Type InCommitMethod)
{
	Slider_Value->SetValue(FMath::Clamp((FCString::Atof(*InText.ToString()) - MinValue * ScaleFactor) /  (MaxValue * ScaleFactor - MinValue * ScaleFactor), 0.f, 1.f));
	if(OnValueChanged.IsBound())
	{
		OnValueChanged.Broadcast(this, GetValue());
	}
}

FParameter UWidgetFloatSettingItemBase::GetValue() const
{
	return FMath::Lerp(MinValue, MaxValue, Slider_Value->GetValue());
}

void UWidgetFloatSettingItemBase::SetValue(const FParameter& InValue)
{
	Slider_Value->SetValue((InValue.GetFloatValue() - MinValue) / (MaxValue - MinValue));
	Super::SetValue(InValue);
}
