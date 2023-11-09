// Copyright Epic Games, Inc. All Rights Reserved.

#include "Main/ModuleGraphSchema.h"

UModuleGraphSchema::UModuleGraphSchema(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer) {}

UK2Node_VariableGet* UModuleGraphSchema::SpawnVariableGetNode(const FVector2D GraphPosition, class UEdGraph* ParentGraph, FName VariableName, UStruct* Source) const
{
	return Super::SpawnVariableGetNode(GraphPosition, ParentGraph, VariableName, Source);
}

UK2Node_VariableSet* UModuleGraphSchema::SpawnVariableSetNode(const FVector2D GraphPosition, class UEdGraph* ParentGraph, FName VariableName, UStruct* Source) const
{
	return Super::SpawnVariableSetNode(GraphPosition, ParentGraph, VariableName, Source);
}
