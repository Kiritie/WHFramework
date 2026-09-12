// Fill out your copyright notice in the Description Item of Project Settings.

#include "Setting/Widget/Item/WidgetFloatSettingItemBase.h"

#include "Setting/SettingModuleTypes.h"

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

void UWidgetFloatSettingItemBase::OnSpawn_Implementation(const FParameter& InParam)
{
	Super::OnSpawn_Implementation(InParam);

	Slider_Value->OnValueChanged.AddDynamic(this, &UWidgetFloatSettingItemBase::OnSliderValueChanged);
	if(TxtBox_Value)
	{
		TxtBox_Value->OnTextCommitted.AddDynamic(this, &UWidgetFloatSettingItemBase::OnTextBoxValueCommitted);
	}

	const FWidgetFloatSettingItemSpawnParameter& Parameter = InParam.GetRef<FWidgetFloatSettingItemSpawnParameter>();
	MinValue = Parameter.MinValue;
	MaxValue = Parameter.MaxValue;
	DecimalNum = Parameter.DecimalNum;
	ScaleFactor = Parameter.ScaleFactor;

	if(Txt_MinValue)
	{
		Txt_MinValue->SetText(UKismetTextLibrary::Conv_DoubleToText(MinValue * ScaleFactor, ERoundingMode::HalfToEven, false, false, 1, 324, 0, DecimalNum));
	}
	if(Txt_MaxValue)
	{
		Txt_MaxValue->SetText(UKismetTextLibrary::Conv_DoubleToText(MaxValue * ScaleFactor, ERoundingMode::HalfToEven, false, false, 1, 324, 0, DecimalNum));
	}
}

void UWidgetFloatSettingItemBase::OnDespawn_Implementation(EObjectDespawnMode InMode)
{
	Super::OnDespawn_Implementation(InMode);

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
	const float Value = (InValue.Get<float>() - MinValue) / (MaxValue - MinValue);
	if(Slider_Value->GetValue() == Value)
	{
		OnSliderValueChanged(Value);
	}
	Slider_Value->SetValue(Value);
	Super::SetValue(InValue);
}
