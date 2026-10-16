#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateTypes.h"
#include "Widget/Style/CommonStyleTypes.h"
#include "CommonScrollStyle.generated.h"

UCLASS(BlueprintType, Blueprintable)
class WHFRAMEWORK_API UCommonScrollStyle : public UObject
{
	GENERATED_BODY()

public:
	UCommonScrollStyle();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FScrollBoxStyle ScrollBoxStyle;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FScrollBarStyle ScrollBarStyle;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FVector2D ScrollBarThickness = FVector2D(5.f, 5.f);

};
