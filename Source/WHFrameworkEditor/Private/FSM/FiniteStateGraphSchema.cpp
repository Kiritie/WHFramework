// Copyright Epic Games, Inc. All Rights Reserved.

#include "FSM/FiniteStateGraphSchema.h"


UFiniteStateGraphSchema::UFiniteStateGraphSchema(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer) {}

UK2Node_VariableGet* UFiniteStateGraphSchema::SpawnVariableGetNode(const FVector2D GraphPosition, class UEdGraph* ParentGraph, FName VariableName, UStruct* Source) const
{
	return Super::SpawnVariableGetNode(GraphPosition, ParentGraph, VariableName, Source);
}

UK2Node_VariableSet* UFiniteStateGraphSchema::SpawnVariableSetNode(const FVector2D GraphPosition, class UEdGraph* ParentGraph, FName VariableName, UStruct* Source) const
{
	return Super::SpawnVariableSetNode(GraphPosition, ParentGraph, VariableName, Source);
}
