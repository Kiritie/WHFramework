#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateTypes.h"
#include "Widget/Style/CommonStyleTypes.h"
#include "CommonInputStyle.generated.h"

UCLASS(BlueprintType, Blueprintable)
class WHFRAMEWORK_API UCommonInputStyle : public UObject
{
	GENERATED_BODY()

public:
	UCommonInputStyle();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FEditableTextBoxStyle InputStyle;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FCommonOptionStyleTags OptionStyles;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FComboBoxStyle ComboBoxStyle;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FTableRowStyle ComboItemStyle;

};
