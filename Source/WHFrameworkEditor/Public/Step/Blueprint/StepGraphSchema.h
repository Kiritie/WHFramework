// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "EdGraphSchema_K2.h"
#include "StepGraphSchema.generated.h"

UCLASS(MinimalAPI)
class UStepGraphSchema : public UEdGraphSchema_K2
{
	GENERATED_BODY()

public:
	UStepGraphSchema(const FObjectInitializer& ObjectInitializer);

public:
	virtual bool ShouldAlwaysPurgeOnModification() const override { return true; }
};
