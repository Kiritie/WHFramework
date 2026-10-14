#pragma once
#include "UObject/Object.h"
#include "Styling/SlateBrush.h"
#include "WidgetBrushStyle.generated.h"

UCLASS(BlueprintType, Blueprintable)
class WHFRAMEWORK_API UWidgetBrushStyle : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Style")
	FSlateBrush Brush;
};
