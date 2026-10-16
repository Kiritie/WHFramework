#pragma once

#include "CoreMinimal.h"
#include "CommonRangeTypes.generated.h"

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FCommonRangeConfig
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bUseSlider = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bShowStepButtons = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bHasMin = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bHasMax = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	double Min = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	double Max = 1.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	double Step = 0.01;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	double DisplayScale = 1.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 DecimalPlaces = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText Suffix;
};
