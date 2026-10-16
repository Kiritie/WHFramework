#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateTypes.h"
#include "Widget/Style/CommonStyleTypes.h"
#include "Materials/MaterialInterface.h"
#include "CommonProgressStyle.generated.h"

UCLASS(BlueprintType, Blueprintable)
class WHFRAMEWORK_API UCommonProgressStyle : public UObject
{
	GENERATED_BODY()

public:
	UCommonProgressStyle();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FProgressBarStyle ProgressBarStyle;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<UMaterialInterface> RadialMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FName ProgressParameter = TEXT("Progress");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FLinearColor FillColor = FLinearColor::White;

};
