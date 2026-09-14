#include "Setting/Widget/Entry/WidgetSettingCategoryEntry.h"

#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Setting/SettingCategoryEntry.h"

void UWidgetSettingCategoryEntry::NativeOnListItemObjectSet(UObject* InListItemObject)
{
	IUserObjectListEntry::NativeOnListItemObjectSet(InListItemObject);
	if(!Txt_Title && WidgetTree)
	{
		Txt_Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Txt_Title"));
		if(!WidgetTree->RootWidget)
		{
			WidgetTree->RootWidget = Txt_Title;
		}
	}

	const USettingCategoryEntry* CategoryEntry = Cast<USettingCategoryEntry>(InListItemObject);
	if(CategoryEntry && Txt_Title)
	{
		Txt_Title->SetText(CategoryEntry->GetDisplayName());
	}
}
