// Copyright Epic Games, Inc. All Rights Reserved.

#include "Procedure/ProcedureGraphSchema.h"

#include "EdGraphSchema_K2_Actions.h"
#include "Kismet2/BlueprintEditorUtils.h"

UProcedureGraphSchema::UProcedureGraphSchema(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer) {}

UK2Node_VariableGet* UProcedureGraphSchema::SpawnVariableGetNode(const FVector2D GraphPosition, class UEdGraph* ParentGraph, FName VariableName, UStruct* Source) const
{
	return Super::SpawnVariableGetNode(GraphPosition, ParentGraph, VariableName, Source);
}

UK2Node_VariableSet* UProcedureGraphSchema::SpawnVariableSetNode(const FVector2D GraphPosition, class UEdGraph* ParentGraph, FName VariableName, UStruct* Source) const
{
	return Super::SpawnVariableSetNode(GraphPosition, ParentGraph, VariableName, Source);
}
