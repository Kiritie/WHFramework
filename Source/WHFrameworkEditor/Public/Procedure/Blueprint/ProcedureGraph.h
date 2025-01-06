// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "EdGraph/EdGraph.h"
#include "ProcedureGraph.generated.h"

UCLASS(MinimalAPI)
class UProcedureGraph : public UEdGraph
{
	GENERATED_BODY()

public:
	UProcedureGraph(const FObjectInitializer& ObjectInitializer);
};
