#pragma once

#include "GameplayTagContainer.h"
#include "CommonStyleTypes.generated.h"

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FCommonControlStyleTags
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (Categories = "Style.Input"))
	FGameplayTag ValueInputStyleTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (Categories = "Style.Toggle"))
	FGameplayTag ValueToggleStyleTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (Categories = "Style.Range"))
	FGameplayTag ValueRangeStyleTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (Categories = "Style.Input"))
	FGameplayTag ValueOptionStyleTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (Categories = "Style.Button"))
	FGameplayTag ValueButtonStyleTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (Categories = "Style.Text"))
	FGameplayTag ReasonTextStyleTag;

};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FCommonOptionStyleTags
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (Categories = "Style.Text"))
	FGameplayTag LabelTextStyleTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (Categories = "Style.Brush"))
	FGameplayTag ValueBackgroundBrushStyleTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (Categories = "Style.Button"))
	FGameplayTag PreviousButtonStyleTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (Categories = "Style.Button"))
	FGameplayTag NextButtonStyleTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (Categories = "Style.Button"))
	FGameplayTag ExpandButtonStyleTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (Categories = "Style.Brush"))
	FGameplayTag ArrowBrushStyleTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (Categories = "Style.Border"))
	FGameplayTag PopupBorderStyleTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (Categories = "Style.Scroll"))
	FGameplayTag PopupScrollStyleTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (Categories = "Style.Button"))
	FGameplayTag ItemButtonStyleTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (Categories = "Style.Text"))
	FGameplayTag ItemTextStyleTag;

};
