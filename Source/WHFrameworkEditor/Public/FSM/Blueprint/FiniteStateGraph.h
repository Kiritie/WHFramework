// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraph.h"
#include "FiniteStateGraph.generated.h"

UCLASS(MinimalAPI)
class UFiniteStateGraph : public UEdGraph
{
	GENERATED_BODY()

public:
	UFiniteStateGraph(const FObjectInitializer& ObjectInitializer);
};
