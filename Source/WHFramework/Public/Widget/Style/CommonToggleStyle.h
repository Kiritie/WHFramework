#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateTypes.h"
#include "Widget/Style/CommonStyleTypes.h"
#include "CommonToggleStyle.generated.h"

UCLASS(BlueprintType, Blueprintable)
class WHFRAMEWORK_API UCommonToggleStyle : public UObject
{
	GENERATED_BODY()

public:
	UCommonToggleStyle();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FCheckBoxStyle CheckBoxStyle;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (Categories = "Style.Text"))
	FGameplayTag LabelTextStyleTag;

};
