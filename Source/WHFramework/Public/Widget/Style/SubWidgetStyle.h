// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonButtonBase.h"
#include "Widget/Style/CommonStyleTypes.h"
#include "SubWidgetStyle.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API USubWidgetStyle : public UCommonButtonStyle
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (Categories = "Style.Brush"))
	FGameplayTag IconStyleTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	bool bIconOnly = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (Categories = "Style.Brush"))
	FGameplayTag SelectionMarkerStyleTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0"))
	FVector2D SelectionMarkerSize = FVector2D(74.f, 6.f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FCommonControlStyleTags ControlStyles;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (Categories = "Style.Text"))
	FGameplayTag NormalTextStyleTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (Categories = "Style.Text"))
	FGameplayTag NormalHoveredTextStyleTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (Categories = "Style.Text"))
	FGameplayTag SelectedTextStyleTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (Categories = "Style.Text"))
	FGameplayTag SelectedHoveredTextStyleTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (Categories = "Style.Text"))
	FGameplayTag DisabledTextStyleTag;
};
