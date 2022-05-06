// Copyright Epic Games, Inc. All Rights Reserved.

#include "Step/StepGraphSchema.h"

#include "EdGraphSchema_K2_Actions.h"
#include "GameplayEffect.h"
#include "Kismet2/BlueprintEditorUtils.h"

UStepGraphSchema::UStepGraphSchema(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer) {}

UK2Node_VariableGet* UStepGraphSchema::SpawnVariableGetNode(const FVector2D GraphPosition, class UEdGraph* ParentGraph, FName VariableName, UStruct* Source) const
{
	return Super::SpawnVariableGetNode(GraphPosition, ParentGraph, VariableName, Source);
}

UK2Node_VariableSet* UStepGraphSchema::SpawnVariableSetNode(const FVector2D GraphPosition, class UEdGraph* ParentGraph, FName VariableName, UStruct* Source) const
{
	return Super::SpawnVariableSetNode(GraphPosition, ParentGraph, VariableName, Source);
}
