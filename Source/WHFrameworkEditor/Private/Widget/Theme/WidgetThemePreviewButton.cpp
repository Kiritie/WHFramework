#include "Widget/Theme/WidgetThemePreviewButton.h"
#include "Widget/Common/CommonTextBlockN.h"

bool UWidgetThemePreviewButton::Initialize()
{
	return Super::Initialize();
}

void UWidgetThemePreviewButton::SetPreviewTextVisible(bool bVisible)
{
	Txt_Title->SetVisibility(bVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
}
