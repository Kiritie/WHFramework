#include "Widget/Theme/WidgetThemePreviewButton.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/Image.h"
#include "Widget/Common/CommonTextBlockN.h"

bool UWidgetThemePreviewButton::Initialize()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("PreviewWidgetTree"), RF_Transient);
		UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Root"));
		Img_Icon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Img_Icon"));
		Txt_Title = WidgetTree->ConstructWidget<UCommonTextBlockN>(UCommonTextBlockN::StaticClass(), TEXT("Txt_Title"));
		Img_Icon->SetVisibility(ESlateVisibility::Collapsed);
		Root->AddChildToOverlay(Img_Icon);
		UOverlaySlot* TitleSlot = Root->AddChildToOverlay(Txt_Title);
		TitleSlot->SetHorizontalAlignment(HAlign_Center);
		TitleSlot->SetVerticalAlignment(VAlign_Center);
		WidgetTree->RootWidget = Root;
	}
	return Super::Initialize();
}
