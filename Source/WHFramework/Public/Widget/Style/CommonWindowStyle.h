#pragma once

#include "CoreMinimal.h"
#include "CommonTextBlock.h"
#include "Styling/SlateTypes.h"
#include "GameplayTagContainer.h"
#include "CommonWindowStyle.generated.h"

/** 基础窗口的纯色默认样式 */
UCLASS(BlueprintType, Blueprintable)
class WHFRAMEWORK_API UCommonWindowStyle : public UObject
{
	GENERATED_BODY()

public:
	UCommonWindowStyle();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FSlateBrush Background;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FMargin Padding = FMargin(24.f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0"))
	float TitleSpacing = 16.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FSlateBrush TitleSeparator;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0"))
	float TitleSeparatorHeight = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0"))
	float TitleBarHeight = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0"))
	float TextWidth = 420.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TSubclassOf<UCommonTextStyle> TitleTextStyle;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (Categories = "UI.Style.Button"))
	FGameplayTag CloseButtonStyleTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FText CloseButtonText;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FVector2D CloseButtonSize = FVector2D(64.f, 32.f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FMargin CloseButtonPadding = FMargin(12.f, 0.f, 0.f, 0.f);
};
