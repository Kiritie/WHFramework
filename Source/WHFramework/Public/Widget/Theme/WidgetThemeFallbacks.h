#pragma once
#include "CommonButtonBase.h"
#include "CommonTextBlock.h"
#include "Widget/Style/WidgetBrushStyle.h"
#include "WidgetThemeFallbacks.generated.h"

UCLASS()
class WHFRAMEWORK_API UWidgetFallbackTextStyle : public UCommonTextStyle
{
	GENERATED_BODY()
};
UCLASS()
class WHFRAMEWORK_API UWidgetFallbackButtonStyle : public UCommonButtonStyle
{
	GENERATED_BODY()
public:
	UWidgetFallbackButtonStyle();
};
UCLASS()
class WHFRAMEWORK_API UWidgetFallbackBrushStyle : public UWidgetBrushStyle
{
	GENERATED_BODY()
public:
	UWidgetFallbackBrushStyle()
	{
		Brush.DrawAs = ESlateBrushDrawType::NoDrawType;
	}
};
