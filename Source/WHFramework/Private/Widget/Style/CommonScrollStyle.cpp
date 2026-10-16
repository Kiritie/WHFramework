#include "Widget/Style/CommonScrollStyle.h"
#include "Components/ScrollBox.h"

UCommonScrollStyle::UCommonScrollStyle()
{
	ScrollBoxStyle = GetDefault<UScrollBox>()->GetWidgetStyle();
	ScrollBarStyle = GetDefault<UScrollBox>()->GetWidgetBarStyle();
}
