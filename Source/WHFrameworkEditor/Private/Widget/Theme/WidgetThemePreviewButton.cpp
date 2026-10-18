#include "Widget/Theme/WidgetThemePreviewButton.h"
#include "Widget/Common/CommonTextBlockN.h"
#include "Widget/Style/CommonButtonStyleN.h"

bool UWidgetThemePreviewButton::Initialize()
{
	return Super::Initialize();
}

void UWidgetThemePreviewButton::SetPreviewTextVisible(bool bVisible)
{
	const UCommonButtonStyleN* ButtonStyle = Cast<UCommonButtonStyleN>(GetStyle());
	Txt_Title->SetVisibility(bVisible && (!ButtonStyle || !ButtonStyle->bIconOnly)
		? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
}
