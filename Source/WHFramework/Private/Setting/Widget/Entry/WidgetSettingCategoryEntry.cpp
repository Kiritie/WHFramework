#include "Setting/Widget/Entry/WidgetSettingCategoryEntry.h"

#include "Blueprint/WidgetTree.h"
#include "Widget/Common/CommonTextBlockN.h"
#include "Setting/SettingCategoryEntry.h"

TSharedRef<SWidget> UWidgetSettingCategoryEntry::RebuildWidget()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"), RF_Transient);
	}
	if (!WidgetTree->RootWidget)
	{
		Txt_Title = WidgetTree->ConstructWidget<UCommonTextBlockN>(UCommonTextBlockN::StaticClass(), TEXT("Txt_Title"));
		WidgetTree->RootWidget = Txt_Title;
	}
	return Super::RebuildWidget();
}

void UWidgetSettingCategoryEntry::NativeOnListItemObjectSet(UObject* InListItemObject)
{
	const USettingCategoryEntry* CategoryEntry = Cast<USettingCategoryEntry>(InListItemObject);
	if(CategoryEntry && Txt_Title)
	{
		Txt_Title->SetText(CategoryEntry->GetDisplayName());
	}
	IUserObjectListEntry::NativeOnListItemObjectSet(InListItemObject);
}

void UWidgetSettingCategoryEntry::NativeOnEntryReleased()
{
	if (Txt_Title)
	{
		Txt_Title->SetText(FText::GetEmpty());
	}
	IUserListEntry::NativeOnEntryReleased();
}
