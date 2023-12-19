// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "EventHandleBase.h"

#include "InstancedEventHandleBase.generated.h"

/**
 * 
 */
UCLASS(Abstract, Blueprintable, BlueprintType, EditInlineNew)
class WHFRAMEWORK_API UInstancedEventHandleBase : public UEventHandleBase
{
	GENERATED_BODY()
	
public:
	UInstancedEventHandleBase();
};
