// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "EdGraph/EdGraph.h"
#include "StepGraph.generated.h"

UCLASS(MinimalAPI)
class UStepGraph : public UEdGraph
{
	GENERATED_BODY()

public:
	UStepGraph(const FObjectInitializer& ObjectInitializer);
};
