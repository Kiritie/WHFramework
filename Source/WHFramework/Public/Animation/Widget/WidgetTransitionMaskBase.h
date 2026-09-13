// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "Widget/Screen/UserWidgetBase.h"

#include "WidgetTransitionMaskBase.generated.h"

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FTransitionMaskWidgetOpenParameter : public FWidgetOpenParameter
{
	GENERATED_BODY()

public:
	FTransitionMaskWidgetOpenParameter() = default;

	explicit FTransitionMaskWidgetOpenParameter(float InDuration)
		: Duration(InDuration)
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Duration = 0.f;
};

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UWidgetTransitionMaskBase : public UUserWidgetBase
{
	GENERATED_BODY()
	
public:
	UWidgetTransitionMaskBase(const FObjectInitializer& ObjectInitializer);
	
public:
	virtual void OnCreate(const FParameter& InParams) override;
	
	virtual void OnInitialize(const FParameter& InParams) override;

	virtual void OnOpen(const FParameter& InParams, bool bInstant) override;

	virtual void OnClose(bool bInstant) override;
};
