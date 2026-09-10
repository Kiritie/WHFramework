#include "Scene/Widget/WidgetSceneMarkerItem.h"

#include "Components/Image.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "Blueprint/WidgetTree.h"
#include "Styling/CoreStyle.h"

UWidgetSceneMarkerItem::UWidgetSceneMarkerItem(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ImgIcon = nullptr;
	TxtSymbol = nullptr;
	TxtName = nullptr;
	TxtDistance = nullptr;
}

TSharedRef<SWidget> UWidgetSceneMarkerItem::RebuildWidget()
{
	if(WidgetTree && !WidgetTree->RootWidget)
	{
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("MarkerRow"));
		USizeBox* IconSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("IconSize"));
		UOverlay* IconLayer = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("IconLayer"));
		ImgIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("ImgIcon"));
		TxtSymbol = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TxtSymbol"));
		TxtName = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TxtName"));
		TxtDistance = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TxtDistance"));

		WidgetTree->RootWidget = Row;
		IconSizeBox->SetWidthOverride(UWidgetSceneMarkerItem::IconSize);
		IconSizeBox->SetHeightOverride(UWidgetSceneMarkerItem::IconSize);
		IconSizeBox->AddChild(IconLayer);
		IconLayer->AddChild(ImgIcon);
		IconLayer->AddChild(TxtSymbol);
		TxtSymbol->SetText(NSLOCTEXT("SceneMarker", "FallbackSymbol", "◆"));
		TxtSymbol->SetJustification(ETextJustify::Center);
		TxtSymbol->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 18));
		TxtSymbol->SetRenderTransformPivot(FVector2D(0.5f));
		TxtName->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 13));
		TxtDistance->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 11));
		for(UTextBlock* Text : {TxtSymbol, TxtName, TxtDistance})
		{
			Text->SetColorAndOpacity(FSlateColor(FLinearColor::White));
			Text->SetShadowOffset(FVector2D(1.f));
			Text->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.85f));
		}

		if(UHorizontalBoxSlot* IconSlot = Row->AddChildToHorizontalBox(IconSizeBox))
		{
			IconSlot->SetVerticalAlignment(VAlign_Center);
			IconSlot->SetPadding(FMargin(0.f, 0.f, 5.f, 0.f));
		}
		UVerticalBox* Labels = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Labels"));
		if(UHorizontalBoxSlot* LabelSlot = Row->AddChildToHorizontalBox(Labels)) LabelSlot->SetVerticalAlignment(VAlign_Center);
		if(UVerticalBoxSlot* NameSlot = Labels->AddChildToVerticalBox(TxtName)) NameSlot->SetHorizontalAlignment(HAlign_Left);
		if(UVerticalBoxSlot* DistanceSlot = Labels->AddChildToVerticalBox(TxtDistance)) DistanceSlot->SetHorizontalAlignment(HAlign_Left);
	}
	const TSharedRef<SWidget> Result = Super::RebuildWidget();
	SetMarkerView(MarkerView, bShowName, bShowDistance);
	return Result;
}

void UWidgetSceneMarkerItem::SetMarkerView(const FSceneMarkerView& InMarkerView, bool bInShowName, bool bInShowDistance)
{
	MarkerView = InMarkerView;
	bShowName = bInShowName;
	bShowDistance = bInShowDistance;
	if(ImgIcon)
	{
		const TSoftObjectPtr<UTexture2D> DesiredIcon = MarkerView.bPlayer && MarkerView.Marker.Icon.IsNull() ? PlayerIcon : MarkerView.Marker.Icon;
		UTexture2D* DesiredTexture = DesiredIcon.LoadSynchronous();
		const bool bUsePlayerIcon = MarkerView.bPlayer && DesiredTexture;
		ImgIcon->SetColorAndOpacity(bUsePlayerIcon ? FLinearColor::White : MarkerView.Marker.Color);
		if(ImgIcon->GetBrush().GetResourceObject() != DesiredTexture)
		{
			ImgIcon->SetBrushResourceObject(DesiredTexture);
		}
		ImgIcon->SetRenderTransformPivot(FVector2D(0.5f));
		ImgIcon->SetRenderScale(bUsePlayerIcon ? FVector2D(0.9f) : FVector2D(1.f));
		if(!bUsePlayerIcon) ImgIcon->SetRenderTransformAngle(0.f);
		ImgIcon->SetVisibility(DesiredTexture ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
	if(TxtSymbol)
	{
		TxtSymbol->SetText(MarkerView.bPlayer ? NSLOCTEXT("SceneMarker", "PlayerSymbol", "▲") : NSLOCTEXT("SceneMarker", "FallbackSymbol", "◆"));
		TxtSymbol->SetColorAndOpacity(FSlateColor(MarkerView.Marker.Color));
		TxtSymbol->SetRenderScale(MarkerView.bPlayer ? FVector2D(4.f / 3.f) : FVector2D(1.f));
		if(!MarkerView.bPlayer) TxtSymbol->SetRenderTransformAngle(0.f);
		TxtSymbol->SetVisibility(ImgIcon && ImgIcon->GetBrush().GetResourceObject() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}
	if(TxtName)
	{
		TxtName->SetText(MarkerView.Marker.DisplayName);
		TxtName->SetVisibility(bInShowName && !MarkerView.bPlayer ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
	if(TxtDistance)
	{
		TxtDistance->SetText(FText::Format(NSLOCTEXT("SceneMarker", "DistanceMeters", "{0}m"), FText::AsNumber(FMath::RoundToInt(MarkerView.Distance / 100.f))));
		TxtDistance->SetVisibility(bInShowDistance ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UWidgetSceneMarkerItem::UpdateMarkerState(const FSceneMarkerView& InMarkerView)
{
	MarkerView.Location = InMarkerView.Location;
	MarkerView.Distance = InMarkerView.Distance;
	MarkerView.Bearing = InMarkerView.Bearing;
	MarkerView.bTargetLoaded = InMarkerView.bTargetLoaded;
	MarkerView.bTracked = InMarkerView.bTracked;
	if(TxtDistance)
	{
		TxtDistance->SetText(FText::Format(NSLOCTEXT("SceneMarker", "DistanceMeters", "{0}m"), FText::AsNumber(FMath::RoundToInt(MarkerView.Distance / 100.f))));
	}
}

void UWidgetSceneMarkerItem::SetPlayerRotation(float InAngle)
{
	if(MarkerView.bPlayer)
	{
		if(ImgIcon) ImgIcon->SetRenderTransformAngle(InAngle);
		if(TxtSymbol) TxtSymbol->SetRenderTransformAngle(InAngle);
	}
}
