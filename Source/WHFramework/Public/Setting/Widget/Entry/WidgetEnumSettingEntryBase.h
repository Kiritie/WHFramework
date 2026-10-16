#pragma once

#include "Setting/Widget/Entry/WidgetOptionSettingEntryBase.h"
#include "WidgetEnumSettingEntryBase.generated.h"

UCLASS()
class WHFRAMEWORK_API UWidgetEnumSettingEntryBase : public UWidgetOptionSettingEntryBase
{
	GENERATED_BODY()

public:
	UWidgetEnumSettingEntryBase(const FObjectInitializer& ObjectInitializer);

protected:
	virtual FName GetRendererName() const override;
};
