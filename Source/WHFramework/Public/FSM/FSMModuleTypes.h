// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "FSMModuleTypes.generated.h"

class UFSMComponent;
class UFiniteStateBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnFiniteStateChanged, UFiniteStateBase*, InCurrentState, UFiniteStateBase*, InLastState);

USTRUCT(BlueprintType)
struct FFSMGroup
{
	GENERATED_BODY()

public:
	FFSMGroup()
	{
		FSMArray = TArray<UFSMComponent*>();
	}

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TArray<UFSMComponent*> FSMArray;
};
