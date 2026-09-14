// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "UObject/Interface.h"
#include "ReferencePoolInterface.generated.h"

UINTERFACE(MinimalAPI)
class UReferencePoolInterface : public UInterface
{
	GENERATED_BODY()
};

class WHFRAMEWORK_API IReferencePoolInterface
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void OnReset();
};
