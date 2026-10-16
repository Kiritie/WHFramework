#include "Widget/Common/CommonRange.h"

#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Slider.h"
#include "Components/SpinBox.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Internationalization/Internationalization.h"
#include "Widget/Common/CommonButton.h"
#include "Widget/Common/CommonInput.h"
#include "Widget/Common/CommonTextBlockN.h"
#include "Widget/Common/CommonImageN.h"
#include "Widget/Style/CommonRangeStyle.h"

UCommonRange::UCommonRange(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

EWidgetStyleKind UCommonRange::GetStyleKind() const
{
	return EWidgetStyleKind::Range;
}

void UCommonRange::BuildNativeLayout()
{
	UHorizontalBox* Root = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("RangeLayout"));
	WidgetTree->RootWidget = Root;
	Slider_Value = WidgetTree->ConstructWidget<USlider>(USlider::StaticClass(), TEXT("Slider_Value"));
	Root->AddChildToHorizontalBox(Slider_Value)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	NativeValueArea = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("ValueArea"));
	Root->AddChildToHorizontalBox(NativeValueArea);
	NativeValueBackground = WidgetTree->ConstructWidget<UCommonImageN>(UCommonImageN::StaticClass(), TEXT("ValueBackground"));
	UOverlaySlot* BackgroundSlot = NativeValueArea->AddChildToOverlay(NativeValueBackground);
	BackgroundSlot->SetHorizontalAlignment(HAlign_Fill);
	BackgroundSlot->SetVerticalAlignment(VAlign_Fill);
	NativeValueBackground->SetVisibility(ESlateVisibility::Collapsed);
	UHorizontalBox* Controls = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ValueControls"));
	UOverlaySlot* ControlsSlot = NativeValueArea->AddChildToOverlay(Controls);
	ControlsSlot->SetHorizontalAlignment(HAlign_Fill);
	ControlsSlot->SetVerticalAlignment(VAlign_Fill);
	Btn_Previous = WidgetTree->ConstructWidget<UCommonButton>(UCommonButton::StaticClass(), TEXT("Btn_Previous"));
	Btn_Previous->SetTitle(FText::FromString(TEXT("−")));
	Controls->AddChildToHorizontalBox(Btn_Previous)->SetVerticalAlignment(VAlign_Center);
	Input_Value = WidgetTree->ConstructWidget<UCommonInput>(UCommonInput::StaticClass(), TEXT("Input_Value"));
	Input_Value->SetValue(Value);
	Controls->AddChildToHorizontalBox(Input_Value)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	Txt_Suffix = WidgetTree->ConstructWidget<UCommonTextBlockN>(UCommonTextBlockN::StaticClass(), TEXT("Txt_Suffix"));
	Controls->AddChildToHorizontalBox(Txt_Suffix)->SetVerticalAlignment(VAlign_Center);
	Btn_Next = WidgetTree->ConstructWidget<UCommonButton>(UCommonButton::StaticClass(), TEXT("Btn_Next"));
	Btn_Next->SetTitle(FText::FromString(TEXT("+")));
	Controls->AddChildToHorizontalBox(Btn_Next)->SetVerticalAlignment(VAlign_Center);
}

void UCommonRange::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	RefreshControls();
}

void UCommonRange::ApplyResolvedStyle(const UObject* InStyle)
{
	const UCommonRangeStyle* Style = Cast<UCommonRangeStyle>(InStyle);
	if (!Style)
	{
		return;
	}
	if (Slider_Value)
	{
		Slider_Value->SetWidgetStyle(Style->SliderStyle);
	}
	if (SpinBox_Value)
	{
		SpinBox_Value->SetWidgetStyle(Style->SpinBoxStyle);
	}
	if (Input_Value)
	{
		Input_Value->SetInheritedStyleTag(Style->ValueInputStyleTag);
		Input_Value->SetUseStyleBackground(!Style->ValueBackgroundBrushStyleTag.IsValid());
	}
	if (NativeValueBackground)
	{
		NativeValueBackground->SetStyleTag(Style->ValueBackgroundBrushStyleTag);
		NativeValueBackground->SetVisibility(Style->ValueBackgroundBrushStyleTag.IsValid() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (Txt_Value)
	{
		Txt_Value->SetStyleTag(Style->ValueTextStyleTag);
	}
	if (Txt_MinValue)
	{
		Txt_MinValue->SetStyleTag(Style->LimitTextStyleTag);
	}
	if (Txt_MaxValue)
	{
		Txt_MaxValue->SetStyleTag(Style->LimitTextStyleTag);
	}
	if (Txt_Suffix)
	{
		Txt_Suffix->SetStyleTag(Style->ValueTextStyleTag);
	}
	if (Btn_Previous)
	{
		Btn_Previous->SetInheritedStyleTag(Style->PreviousButtonStyleTag);
	}
	if (Btn_Next)
	{
		Btn_Next->SetInheritedStyleTag(Style->NextButtonStyleTag);
	}
}

void UCommonRange::BindControlEvents()
{
	if (Slider_Value)
	{
		Slider_Value->OnValueChanged.AddUniqueDynamic(this, &ThisClass::HandleSliderChanged);
		Slider_Value->OnMouseCaptureEnd.AddUniqueDynamic(this, &ThisClass::HandleCaptureEnd);
		Slider_Value->OnControllerCaptureEnd.AddUniqueDynamic(this, &ThisClass::HandleCaptureEnd);
	}
	if (SpinBox_Value)
	{
		SpinBox_Value->OnValueChanged.AddUniqueDynamic(this, &ThisClass::HandleSpinChanged);
		SpinBox_Value->OnValueCommitted.AddUniqueDynamic(this, &ThisClass::HandleSpinCommitted);
	}
	if (Input_Value)
	{
		Input_Value->OnTextCommitted.AddUniqueDynamic(this, &ThisClass::HandleInputCommitted);
	}
	if (Btn_Previous)
	{
		Btn_Previous->OnClicked().RemoveAll(this);
		Btn_Previous->OnClicked().AddUObject(this, &ThisClass::OnPreviousClicked);
	}
	if (Btn_Next)
	{
		Btn_Next->OnClicked().RemoveAll(this);
		Btn_Next->OnClicked().AddUObject(this, &ThisClass::OnNextClicked);
	}
}

void UCommonRange::UnbindControlEvents()
{
	if (Slider_Value)
	{
		Slider_Value->OnValueChanged.RemoveDynamic(this, &ThisClass::HandleSliderChanged);
		Slider_Value->OnMouseCaptureEnd.RemoveDynamic(this, &ThisClass::HandleCaptureEnd);
		Slider_Value->OnControllerCaptureEnd.RemoveDynamic(this, &ThisClass::HandleCaptureEnd);
	}
	if (SpinBox_Value)
	{
		SpinBox_Value->OnValueChanged.RemoveDynamic(this, &ThisClass::HandleSpinChanged);
		SpinBox_Value->OnValueCommitted.RemoveDynamic(this, &ThisClass::HandleSpinCommitted);
	}
	if (Input_Value)
	{
		Input_Value->OnTextCommitted.RemoveDynamic(this, &ThisClass::HandleInputCommitted);
	}
	if (Btn_Previous)
	{
		Btn_Previous->OnClicked().RemoveAll(this);
	}
	if (Btn_Next)
	{
		Btn_Next->OnClicked().RemoveAll(this);
	}
}

void UCommonRange::OnDespawn_Implementation(EObjectDespawnMode InMode)
{
	Super::OnDespawn_Implementation(InMode);
	OnValueChanged.Clear();
	OnValueCommitted.Clear();
	const UCommonRange* Defaults = CastChecked<UCommonRange>(GetArchetype());
	Config = Defaults->Config;
	Value = Defaults->Value;
	RefreshControls();
}

void UCommonRange::Configure(const FCommonRangeConfig& InConfig)
{
	Config = InConfig;
	if (!FMath::IsFinite(Config.DisplayScale) || Config.DisplayScale <= 0.0)
	{
		Config.DisplayScale = 1.0;
	}
	Config.DecimalPlaces = FMath::Clamp(Config.DecimalPlaces, 0, 12);
	RefreshControls();
}

void UCommonRange::SetValue(const FParameter& InValue, bool bNotify)
{
	double Numeric;
	if (!NumericValue(InValue, Numeric))
	{
		return;
	}
	const bool bChanged = Value != InValue;
	Value = InValue;
	RefreshControls();
	if (bNotify && bChanged)
	{
		const FParameter Snapshot = Value;
		OnValueChanged.Broadcast(this, Snapshot);
	}
}

FParameter UCommonRange::GetValue() const
{
	return Value;
}

FText UCommonRange::FormatValue(double InValue) const
{
	FNumberFormattingOptions Options;
	Options.UseGrouping = false;
	Options.MinimumFractionalDigits = Config.DecimalPlaces;
	Options.MaximumFractionalDigits = Config.DecimalPlaces;
	return FText::AsNumber(InValue, &Options);
}

void UCommonRange::RefreshControls()
{
	TGuardValue<bool> Guard(bSynchronizingValue, true);
	double Numeric = 0.0;
	NumericValue(Value, Numeric);
	const bool bInteger = Value.Is<int32>() || Value.Is<int64>();
	// Native slider/spin values are floats; large integers remain in the exact typed input.
	const bool bPreciseInput = bInteger && (FMath::Abs(Numeric) > 16777216.0 ||
		(Config.bHasMin && FMath::Abs(Config.Min) > 16777216.0) || (Config.bHasMax && FMath::Abs(Config.Max) > 16777216.0));
	const bool bHasRange = Config.bUseSlider && !bPreciseInput && Config.bHasMin && Config.bHasMax && Config.Max > Config.Min;
	if (NativeValueArea)
	{
		if (UHorizontalBoxSlot* ValueSlot = Cast<UHorizontalBoxSlot>(NativeValueArea->Slot))
		{
			ValueSlot->SetSize(FSlateChildSize(bHasRange ? ESlateSizeRule::Automatic : ESlateSizeRule::Fill));
			ValueSlot->SetPadding(bHasRange ? FMargin(12.f, 0.f, 0.f, 0.f) : FMargin(0.f));
		}
	}
	if (Slider_Value)
	{
		Slider_Value->SetVisibility(bHasRange ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		if (bHasRange)
		{
			Slider_Value->SetMinValue(static_cast<float>(Config.Min));
			Slider_Value->SetMaxValue(static_cast<float>(Config.Max));
			Slider_Value->SetStepSize(static_cast<float>(FMath::Max(0.0, Config.Step)));
			Slider_Value->SetValue(static_cast<float>(Numeric));
		}
	}
	if (SpinBox_Value)
	{
		SpinBox_Value->SetVisibility(bPreciseInput ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
		SpinBox_Value->SetMinValue(Config.bHasMin ? static_cast<float>(Config.Min * Config.DisplayScale) : -TNumericLimits<float>::Max());
		SpinBox_Value->SetMaxValue(Config.bHasMax ? static_cast<float>(Config.Max * Config.DisplayScale) : TNumericLimits<float>::Max());
		SpinBox_Value->SetDelta(static_cast<float>(FMath::Abs(Config.Step * Config.DisplayScale)));
		SpinBox_Value->SetValue(static_cast<float>(Numeric * Config.DisplayScale));
	}
	if (Input_Value)
	{
		Input_Value->SetValue(Config.DisplayScale == 1.0 ? Value : FParameter(Numeric * Config.DisplayScale));
		if (!bInteger || Config.DisplayScale != 1.0)
		{
			FNumberFormattingOptions Options;
			Options.UseGrouping = false;
			Options.MinimumFractionalDigits = Config.DecimalPlaces;
			Options.MaximumFractionalDigits = Config.DecimalPlaces;
			Input_Value->SetText(FText::AsNumber(Numeric * Config.DisplayScale, &Options, FInternationalization::Get().GetInvariantCulture()));
		}
	}
	if (Txt_Value)
	{
		FText Display = FormatValue(Numeric * Config.DisplayScale);
		if (const int64* Exact = Config.DisplayScale == 1.0 ? Value.GetPtr<int64>() : nullptr)
		{
			Display = FText::AsNumber(*Exact);
		}
		Txt_Value->SetText(FText::Format(NSLOCTEXT("CommonRange", "ValueSuffix", "{0}{1}"), Display, Config.Suffix));
	}
	if (Txt_Suffix)
	{
		Txt_Suffix->SetText(Config.Suffix);
	}
	if (Txt_MinValue)
	{
		Txt_MinValue->SetText(Config.bHasMin ? FormatValue(Config.Min * Config.DisplayScale) : FText::GetEmpty());
	}
	if (Txt_MaxValue)
	{
		Txt_MaxValue->SetText(Config.bHasMax ? FormatValue(Config.Max * Config.DisplayScale) : FText::GetEmpty());
	}
	if (Btn_Previous)
	{
		Btn_Previous->SetVisibility(Config.bShowStepButtons ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		Btn_Previous->SetIsEnabled(!Config.bHasMin || Numeric > Config.Min);
	}
	if (Btn_Next)
	{
		Btn_Next->SetVisibility(Config.bShowStepButtons ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		Btn_Next->SetIsEnabled(!Config.bHasMax || Numeric < Config.Max);
	}
}

void UCommonRange::AcceptUserValue(FParameter InValue, bool bCommit)
{
	double Numeric;
	if (bSynchronizingValue || !NumericValue(InValue, Numeric))
	{
		return;
	}
	if (Config.bHasMin && Numeric < Config.Min)
	{
		InValue = MakeTypedValue(Config.Min);
	}
	else if (Config.bHasMax && Numeric > Config.Max)
	{
		InValue = MakeTypedValue(Config.Max);
	}
	if (!InValue.HasValue())
	{
		RefreshControls();
		return;
	}
	SetValue(InValue, true);
	if (bCommit)
	{
		const FParameter Snapshot = Value;
		OnValueCommitted.Broadcast(this, Snapshot);
	}
}

void UCommonRange::HandleSliderChanged(float InValue)
{
	AcceptUserValue(MakeTypedValue(InValue), false);
}

void UCommonRange::HandleSpinChanged(float InValue)
{
	AcceptUserValue(MakeTypedValue(InValue / Config.DisplayScale), false);
}

void UCommonRange::HandleSpinCommitted(float InValue, ETextCommit::Type InCommitMethod)
{
	if (InCommitMethod != ETextCommit::OnCleared && !bSynchronizingValue)
	{
		AcceptUserValue(MakeTypedValue(InValue / Config.DisplayScale), true);
	}
}

void UCommonRange::HandleInputCommitted(UCommonInput* InInput, const FText& InText, ETextCommit::Type InCommitMethod)
{
	FParameter InputValue;
	if (InCommitMethod == ETextCommit::OnCleared || !InInput || !InInput->TryGetValue(InputValue))
	{
		RefreshControls();
		return;
	}
	if (Config.DisplayScale == 1.0)
	{
		AcceptUserValue(InputValue, true);
	}
	else
	{
		double Numeric;
		if (NumericValue(InputValue, Numeric))
		{
			AcceptUserValue(MakeTypedValue(Numeric / Config.DisplayScale), true);
		}
	}
}

void UCommonRange::HandleCaptureEnd()
{
	if (!bSynchronizingValue)
	{
		const FParameter Snapshot = Value;
		OnValueCommitted.Broadcast(this, Snapshot);
	}
}

bool UCommonRange::NumericValue(const FParameter& InValue, double& OutValue)
{
	if (const int32* IntValue = InValue.GetPtr<int32>())
	{
		OutValue = *IntValue;
	}
	else if (const int64* Int64Value = InValue.GetPtr<int64>())
	{
		OutValue = static_cast<double>(*Int64Value);
	}
	else if (const float* FloatValue = InValue.GetPtr<float>())
	{
		OutValue = *FloatValue;
	}
	else if (const double* DoubleValue = InValue.GetPtr<double>())
	{
		OutValue = *DoubleValue;
	}
	else
	{
		return false;
	}
	return FMath::IsFinite(OutValue);
}

FParameter UCommonRange::MakeTypedValue(double InValue) const
{
	if (!FMath::IsFinite(InValue))
	{
		return FParameter();
	}
	if (Value.Is<int32>())
	{
		const double Rounded = FMath::RoundToDouble(InValue);
		return Rounded >= -2147483648.0 && Rounded < 2147483648.0 ? FParameter(static_cast<int32>(Rounded)) : FParameter();
	}
	if (Value.Is<int64>())
	{
		const double Rounded = FMath::RoundToDouble(InValue);
		return Rounded >= -9223372036854775808.0 && Rounded < 9223372036854775808.0 ? FParameter(static_cast<int64>(Rounded)) : FParameter();
	}
	return Value.Is<float>() ? FParameter(static_cast<float>(InValue)) : FParameter(InValue);
}

void UCommonRange::SetInputFocus()
{
	if (Input_Value)
	{
		Input_Value->SetInputFocus();
	}
	else if (Slider_Value)
	{
		Slider_Value->SetKeyboardFocus();
	}
}

void UCommonRange::StepValue(int32 InDirection)
{
	if (const int64* Exact = Value.GetPtr<int64>())
	{
		const double Step = FMath::Max(1.0, FMath::RoundToDouble(Config.Step));
		if (!FMath::IsFinite(Step) || Step >= 9223372036854775808.0)
		{
			return;
		}
		const int64 Delta = static_cast<int64>(Step);
		if ((InDirection > 0 && *Exact > TNumericLimits<int64>::Max() - Delta) ||
			(InDirection < 0 && *Exact < TNumericLimits<int64>::Lowest() + Delta))
		{
			return;
		}
		AcceptUserValue(FParameter(*Exact + InDirection * Delta), true);
	}
	else
	{
		double Numeric;
		if (NumericValue(Value, Numeric))
		{
			AcceptUserValue(MakeTypedValue(Numeric + InDirection * FMath::Max(Config.Step, Value.Is<int32>() ? 1.0 : 0.01)), true);
		}
	}
}

void UCommonRange::OnPreviousClicked()
{
	StepValue(-1);
}

void UCommonRange::OnNextClicked()
{
	StepValue(1);
}
