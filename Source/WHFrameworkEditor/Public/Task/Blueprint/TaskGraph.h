// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "EdGraph/EdGraph.h"
#include "TaskGraph.generated.h"

UCLASS(MinimalAPI)
class UTaskGraph : public UEdGraph
{
	GENERATED_BODY()

public:
	UTaskGraph(const FObjectInitializer& ObjectInitializer);
};
