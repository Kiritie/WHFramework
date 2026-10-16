#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateTypes.h"
#include "Widget/Style/CommonStyleTypes.h"
#include "CommonRangeStyle.generated.h"

UCLASS(BlueprintType, Blueprintable)
class WHFRAMEWORK_API UCommonRangeStyle : public UObject
{
	GENERATED_BODY()

public:
	UCommonRangeStyle();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FSliderStyle SliderStyle;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FSpinBoxStyle SpinBoxStyle;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (Categories = "Style.Input"))
	FGameplayTag ValueInputStyleTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (Categories = "Style.Brush"))
	FGameplayTag ValueBackgroundBrushStyleTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (Categories = "Style.Text"))
	FGameplayTag ValueTextStyleTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (Categories = "Style.Text"))
	FGameplayTag LimitTextStyleTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (Categories = "Style.Button"))
	FGameplayTag PreviousButtonStyleTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (Categories = "Style.Button"))
	FGameplayTag NextButtonStyleTag;

};
