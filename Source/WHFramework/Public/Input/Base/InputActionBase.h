// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "GameplayTagContainer.h"
#include "InputAction.h"
#include "InputActionBase.generated.h"

UCLASS(BlueprintType)
class WHFRAMEWORK_API UInputActionBase : public UInputAction
{
	GENERATED_BODY()

public:
	UInputActionBase();

public:
	// A localized descriptor of this input action
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tag")
	FGameplayTag ActionTag;
};