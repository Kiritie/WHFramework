#pragma once

#include "Blueprint/IUserObjectListEntry.h"
#include "Widget/Screen/UserWidgetBase.h"

#include "WidgetSettingCategoryEntry.generated.h"

class UTextBlock;

UCLASS()
class WHFRAMEWORK_API UWidgetSettingCategoryEntry : public UUserWidget, public IUserObjectListEntry
{
	GENERATED_BODY()

protected:
	virtual void NativeOnListItemObjectSet(UObject* InListItemObject) override;

	UPROPERTY(meta = (BindWidget, OptionalWidget = false))
	TObjectPtr<UTextBlock> Txt_Title;
};
