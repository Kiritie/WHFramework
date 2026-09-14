#include "Setting/Widget/Entry/WidgetNumberSettingEntryBase.h"

#include "Components/EditableTextBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Setting/SettingEntry.h"

UWidgetNumberSettingEntryBase::UWidgetNumberSettingEntryBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UWidgetNumberSettingEntryBase::OnSpawn_Implementation(const FParameter& InParam)
{
	Super::OnSpawn_Implementation(InParam);
}

void UWidgetNumberSettingEntryBase::OnDespawn_Implementation(EObjectDespawnMode InMode)
{
	if(Slider_Value)
	{
		Slider_Value->OnValueChanged.RemoveDynamic(this, &ThisClass::OnSliderValueChanged);
	}
	if(TxtBox_Value)
	{
		TxtBox_Value->OnTextCommitted.RemoveDynamic(this, &ThisClass::OnTextValueCommitted);
	}
	Super::OnDespawn_Implementation(InMode);
}

void UWidgetNumberSettingEntryBase::ApplyValueToControl_Implementation(const FParameter& InValue)
{
	if(!SettingEntry)
	{
		return;
	}

	const FSettingNumberDisplay& Display = SettingEntry->GetDefinition().NumberDisplay;
	const double Value = GetNumericValue(InValue);
	const double DisplayValue = Value * Display.Scale;
	const bool bHasRange = Display.bHasMin && Display.bHasMax && Display.Max > Display.Min;
	if(Slider_Value)
	{
		Slider_Value->OnValueChanged.RemoveDynamic(this, &ThisClass::OnSliderValueChanged);
		Slider_Value->OnValueChanged.AddDynamic(this, &ThisClass::OnSliderValueChanged);
		Slider_Value->SetVisibility(bHasRange ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		Slider_Value->SetIsEnabled(SettingEntry->IsEnabled());
		if(bHasRange)
		{
			Slider_Value->SetMinValue(static_cast<float>(Display.Min));
			Slider_Value->SetMaxValue(static_cast<float>(Display.Max));
			Slider_Value->SetStepSize(static_cast<float>(Display.Step));
			Slider_Value->SetValue(static_cast<float>(Value));
		}
	}
	if(Txt_MinValue)
	{
		Txt_MinValue->SetText(Display.bHasMin ? FormatValue(Display.Min * Display.Scale) : FText::GetEmpty());
	}
	if(Txt_MaxValue)
	{
		Txt_MaxValue->SetText(Display.bHasMax ? FormatValue(Display.Max * Display.Scale) : FText::GetEmpty());
	}
	if(Txt_Value)
	{
		Txt_Value->SetText(FormatValue(DisplayValue));
	}
	if(TxtBox_Value)
	{
		TxtBox_Value->OnTextCommitted.RemoveDynamic(this, &ThisClass::OnTextValueCommitted);
		TxtBox_Value->OnTextCommitted.AddDynamic(this, &ThisClass::OnTextValueCommitted);
		TxtBox_Value->SetText(FormatValue(DisplayValue));
	}
}

void UWidgetNumberSettingEntryBase::OnSliderValueChanged(float InValue)
{
	if(bRefreshingFromModel || !SettingEntry)
	{
		return;
	}

	const FSettingNumberDisplay& Display = SettingEntry->GetDefinition().NumberDisplay;
	if(Display.bHasMin && Display.bHasMax && Display.Max > Display.Min)
	{
		if(CommitUserValue(MakeTypedValue(InValue)))
		{
			const double DisplayValue = static_cast<double>(InValue) * Display.Scale;
			if(Txt_Value)
			{
				Txt_Value->SetText(FormatValue(DisplayValue));
			}
			if(TxtBox_Value)
			{
				TxtBox_Value->SetText(FormatValue(DisplayValue));
			}
		}
	}
}

void UWidgetNumberSettingEntryBase::OnTextValueCommitted(const FText& InText, ETextCommit::Type InCommitMethod)
{
	if(bRefreshingFromModel || !SettingEntry)
	{
		return;
	}

	const FSettingNumberDisplay& Display = SettingEntry->GetDefinition().NumberDisplay;
	const double Scale = FMath::IsNearlyZero(Display.Scale) ? 1.0 : Display.Scale;
	CommitUserValue(MakeTypedValue(FCString::Atod(*InText.ToString()) / Scale));
}

FParameter UWidgetNumberSettingEntryBase::MakeTypedValue(double InValue) const
{
	const FParameter CurrentValue = SettingEntry ? SettingEntry->GetPendingValue() : FParameter();
	if(CurrentValue.Is<int32>()) return FParameter(FMath::RoundToInt(InValue));
	if(CurrentValue.Is<int64>()) return FParameter(static_cast<int64>(FMath::RoundToDouble(InValue)));
	if(CurrentValue.Is<float>()) return FParameter(static_cast<float>(InValue));
	return FParameter(InValue);
}

double UWidgetNumberSettingEntryBase::GetNumericValue(const FParameter& InValue) const
{
	if(const int32* Value = InValue.GetPtr<int32>()) return *Value;
	if(const int64* Value = InValue.GetPtr<int64>()) return static_cast<double>(*Value);
	if(const float* Value = InValue.GetPtr<float>()) return *Value;
	if(const double* Value = InValue.GetPtr<double>()) return *Value;
	return 0.0;
}

FText UWidgetNumberSettingEntryBase::FormatValue(double InValue) const
{
	FNumberFormattingOptions Options;
	const int32 DecimalPlaces = SettingEntry ? SettingEntry->GetDefinition().NumberDisplay.DecimalPlaces : 2;
	Options.MinimumFractionalDigits = DecimalPlaces;
	Options.MaximumFractionalDigits = DecimalPlaces;
	FText Result = FText::AsNumber(InValue, &Options);
	if(SettingEntry && !SettingEntry->GetDefinition().NumberDisplay.Suffix.IsEmpty())
	{
		Result = FText::Format(FText::FromString(TEXT("{0}{1}")), Result, SettingEntry->GetDefinition().NumberDisplay.Suffix);
	}
	return Result;
}
