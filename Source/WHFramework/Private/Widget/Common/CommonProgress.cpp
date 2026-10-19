#include "Widget/Common/CommonProgress.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Widget/Style/CommonProgressStyle.h"

UCommonProgress::UCommonProgress(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

void UCommonProgress::OnDespawn_Implementation(EObjectDespawnMode InMode)
{
	Super::OnDespawn_Implementation(InMode);
	SetPercent(CastChecked<UCommonProgress>(GetArchetype())->Percent);
}

EWidgetStyleKind UCommonProgress::GetStyleKind() const
{
	return EWidgetStyleKind::Progress;
}

void UCommonProgress::BuildNativeLayout()
{
	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("ProgressLayout"));
	WidgetTree->RootWidget = Root;
	Img_Background = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Img_Background"));
	UOverlaySlot* BackgroundSlot = Root->AddChildToOverlay(Img_Background);
	BackgroundSlot->SetHorizontalAlignment(HAlign_Fill);
	BackgroundSlot->SetVerticalAlignment(VAlign_Fill);
	Img_Background->SetVisibility(ESlateVisibility::Collapsed);
	Bar_Value = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("Bar_Value"));
	UOverlaySlot* BarSlot = Root->AddChildToOverlay(Bar_Value);
	BarSlot->SetHorizontalAlignment(HAlign_Fill);
	BarSlot->SetVerticalAlignment(VAlign_Fill);
	Img_Radial = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Img_Radial"));
	UOverlaySlot* ImageSlot = Root->AddChildToOverlay(Img_Radial);
	ImageSlot->SetHorizontalAlignment(HAlign_Fill);
	ImageSlot->SetVerticalAlignment(VAlign_Fill);
	Img_Radial->SetVisibility(ESlateVisibility::Collapsed);
}

void UCommonProgress::ApplyResolvedStyle(const UObject* InStyle)
{
	const UCommonProgressStyle* Style = Cast<UCommonProgressStyle>(InStyle);
	if (!Style)
	{
		return;
	}
	const bool bRadial = Style->RadialMaterial != nullptr;
	ProgressParameter = Style->ProgressParameter;
	if (Img_Background)
	{
		Img_Background->SetBrush(Style->RadialBackground);
		Img_Background->SetVisibility(bRadial && Style->RadialBackground.DrawAs != ESlateBrushDrawType::NoDrawType ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (Bar_Value)
	{
		Bar_Value->SetWidgetStyle(Style->ProgressBarStyle);
		Bar_Value->SetBarFillType(Style->FillType);
		Bar_Value->SetBarFillStyle(Style->FillStyle);
		Bar_Value->SetFillColorAndOpacity(Style->FillColor);
		Bar_Value->SetVisibility(bRadial ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}
	if (Img_Radial)
	{
		if (UOverlaySlot* RadialSlot = Cast<UOverlaySlot>(Img_Radial->Slot)) RadialSlot->SetPadding(Style->RadialPadding);
		Img_Radial->SetVisibility(bRadial ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		if (bRadial)
		{
			if (!RadialMaterial || RadialMaterial->Parent != Style->RadialMaterial)
			{
				RadialMaterial = UMaterialInstanceDynamic::Create(Style->RadialMaterial, this);
			}
			Img_Radial->SetBrushFromMaterial(RadialMaterial);
			Img_Radial->SetColorAndOpacity(Style->FillColor);
		}
		else
		{
			RadialMaterial = nullptr;
			Img_Radial->SetBrush(FSlateBrush());
		}
	}
	SetPercent(Percent);
}

void UCommonProgress::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	SetPercent(Percent);
}

void UCommonProgress::SetPercent(float InPercent)
{
	Percent = FMath::IsFinite(InPercent) ? FMath::Clamp(InPercent, 0.f, 1.f) : 0.f;
	if (Bar_Value)
	{
		Bar_Value->SetPercent(Percent);
	}
	if (RadialMaterial)
	{
		RadialMaterial->SetScalarParameterValue(ProgressParameter, Percent);
	}
}

float UCommonProgress::GetPercent() const
{
	return Percent;
}
