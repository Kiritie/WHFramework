#pragma once
#include "UObject/Object.h"
#include "Styling/SlateBrush.h"
#include "CommonBrushStyle.generated.h"

UCLASS(BlueprintType, Blueprintable, meta = (DisplayName = "Common Brush Style"))
class WHFRAMEWORK_API UCommonBrushStyle : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Style")
	FSlateBrush Brush;
};
