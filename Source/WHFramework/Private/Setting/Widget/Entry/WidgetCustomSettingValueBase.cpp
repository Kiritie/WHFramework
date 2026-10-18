#include "Setting/Widget/Entry/WidgetCustomSettingValueBase.h"

bool UWidgetCustomSettingValueBase::CanEditValue_Implementation(const FParameter& InValue) const
{
	return false;
}

void UWidgetCustomSettingValueBase::SubmitValue(const FParameter& InValue)
{
	if (GetIsEnabled() && CanEditValue(InValue))
	{
		OnValueChanged.Broadcast(this, InValue);
	}
}
