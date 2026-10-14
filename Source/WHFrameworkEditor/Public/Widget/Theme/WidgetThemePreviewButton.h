#pragma once
#include "Widget/Common/CommonButton.h"
#include "WidgetThemePreviewButton.generated.h"

UCLASS()
class UWidgetThemePreviewButton : public UCommonButton
{
	GENERATED_BODY()
public:
	virtual bool Initialize() override;
	void SetPreviewTextVisible(bool bVisible);
};
