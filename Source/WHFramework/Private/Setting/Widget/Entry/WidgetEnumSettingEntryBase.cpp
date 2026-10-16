#include "Setting/Widget/Entry/WidgetEnumSettingEntryBase.h"

UWidgetEnumSettingEntryBase::UWidgetEnumSettingEntryBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bUseDropdown = true;
}

FName UWidgetEnumSettingEntryBase::GetRendererName() const
{
	return TEXT("Enum");
}
