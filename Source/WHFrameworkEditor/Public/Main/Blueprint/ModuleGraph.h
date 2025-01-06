// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "EdGraph/EdGraph.h"
#include "ModuleGraph.generated.h"

UCLASS(MinimalAPI)
class UModuleGraph : public UEdGraph
{
	GENERATED_BODY()

public:
	UModuleGraph(const FObjectInitializer& ObjectInitializer);
};
