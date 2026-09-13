// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Widget/World/WorldWidgetBase.h"
#include "WidgetWorldText.generated.h"

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FWorldTextWidgetParameter : public FWidgetSpawnParameter
{
	GENERATED_BODY()

	FWorldTextWidgetParameter()
	{
	}

	FWorldTextWidgetParameter(const FString& InText, const FLinearColor& InTextColor, int32 InTextStyle)
		: Text(InText), TextColor(InTextColor), TextStyle(InTextStyle)
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Text;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FLinearColor TextColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 TextStyle = 0;
};

/**
 * �����ı�
 */
UCLASS()
class WHFRAMEWORK_API UWidgetWorldText : public UWorldWidgetBase
{
	GENERATED_BODY()

public:
	UWidgetWorldText(const FObjectInitializer& ObjectInitializer);
};
