#include "Widget/Style/CommonToggleStyle.h"
#include "Components/CheckBox.h"

UCommonToggleStyle::UCommonToggleStyle()
{
	CheckBoxStyle = GetDefault<UCheckBox>()->GetWidgetStyle();
}
