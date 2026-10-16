#pragma once

#include "Blueprint/IUserObjectListEntry.h"
#include "Blueprint/UserWidget.h"

#include "WidgetSettingCategoryEntry.generated.h"

class UCommonTextBlockN;

UCLASS()
class WHFRAMEWORK_API UWidgetSettingCategoryEntry : public UUserWidget, public IUserObjectListEntry
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeOnListItemObjectSet(UObject* InListItemObject) override;
	virtual void NativeOnEntryReleased() override;

	UPROPERTY(meta = (BindWidget, OptionalWidget = false))
	TObjectPtr<UCommonTextBlockN> Txt_Title;
};
