#include "Setting/SettingCategoryEntry.h"

void USettingCategoryEntry::Initialize(FName InCategory, const FText& InDisplayName, int32 InOrder)
{
	Category = InCategory;
	DisplayName = InDisplayName;
	Order = InOrder;
}
