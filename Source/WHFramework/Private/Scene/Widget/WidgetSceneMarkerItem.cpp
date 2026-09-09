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
	return Super::RebuildWidget();
}

void UWidgetSceneMarkerItem::SetMarkerView(const FSceneMarkerView& InMarkerView, bool bInShowName, bool bInShowDistance)
{
	const TSoftObjectPtr<UTexture2D> PreviousIcon = MarkerView.Marker.Icon;
	MarkerView = InMarkerView;
	if(ImgIcon)
	{
		ImgIcon->SetColorAndOpacity(MarkerView.Marker.Color);
		if(PreviousIcon != MarkerView.Marker.Icon || ImgIcon->GetBrush().GetResourceObject() == nullptr)
		{
			ImgIcon->SetBrushResourceObject(MarkerView.Marker.Icon.LoadSynchronous());
		}
		ImgIcon->SetVisibility(MarkerView.Marker.Icon.IsNull() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}
	if(TxtSymbol)
	{
		TxtSymbol->SetColorAndOpacity(FSlateColor(MarkerView.Marker.Color));
		TxtSymbol->SetVisibility(MarkerView.Marker.Icon.IsNull() ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
	if(TxtName)
	{
		TxtName->SetText(MarkerView.Marker.DisplayName);
		TxtName->SetVisibility(bInShowName ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
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
