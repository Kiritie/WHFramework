// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonButtonBase.h"
#include "Widget/Style/CommonStyleTypes.h"
#include "CommonButtonStyleN.generated.h"

/**
 * 
 */
UCLASS(BlueprintType, Blueprintable, meta = (DisplayName = "Common Button Style N"))
class WHFRAMEWORK_API UCommonButtonStyleN : public UCommonButtonStyle
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (Categories = "UI.Style.Brush"))
	FGameplayTag IconStyleTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ToolTip = "Use the state tint colors for the icon and draw no button background. When unchecked, state brushes style the button background."))
	bool bIconOnly = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (Categories = "UI.Style.Brush"))
	FGameplayTag SelectionMarkerStyleTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0"))
	FVector2D SelectionMarkerSize = FVector2D(74.f, 6.f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FCommonControlStyleTags ControlStyles;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (Categories = "UI.Style.Text"))
	FGameplayTag NormalTextStyleTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (Categories = "UI.Style.Text"))
	FGameplayTag NormalHoveredTextStyleTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (Categories = "UI.Style.Text"))
	FGameplayTag SelectedTextStyleTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (Categories = "UI.Style.Text"))
	FGameplayTag SelectedHoveredTextStyleTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (Categories = "UI.Style.Text"))
	FGameplayTag DisabledTextStyleTag;
};
