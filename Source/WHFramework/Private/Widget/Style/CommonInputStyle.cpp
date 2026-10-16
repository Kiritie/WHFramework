#include "Widget/Style/CommonInputStyle.h"
#include "Components/EditableTextBox.h"
#include "Components/ComboBoxKey.h"

UCommonInputStyle::UCommonInputStyle()
{
	InputStyle = GetDefault<UEditableTextBox>()->GetWidgetStyle();
	ComboBoxStyle = GetDefault<UComboBoxKey>()->GetWidgetStyle();
	ComboItemStyle = GetDefault<UComboBoxKey>()->GetItemStyle();
}
