// Copyright Epic Games, Inc. All Rights Reserved.

#include "Procedure/Blueprint/ProcedureBlueprintFactory.h"

#include "Procedure/Base/ProcedureBase.h"
#include "Procedure/Base/ProcedureBlueprint.h"
#include "Procedure/Blueprint/ProcedureGraph.h"
#include "Procedure/Blueprint/ProcedureGraphSchema.h"

#define LOCTEXT_NAMESPACE "ProcedureBlueprintFactory"

UProcedureBlueprintFactory::UProcedureBlueprintFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UProcedureBlueprint::StaticClass();
	ParentClass = UProcedureBase::StaticClass();

	GraphClass = UProcedureGraph::StaticClass();
	GraphSchemaClass = UProcedureGraphSchema::StaticClass();

	DefaultEventGraph = FName("Procedure Graph");

	DefaultEventNodes.Add(FName("K2_OnInitialize"));
	DefaultEventNodes.Add(FName("K2_OnEnter"));
	DefaultEventNodes.Add(FName("K2_OnRefresh"));
	DefaultEventNodes.Add(FName("K2_OnGuide"));
	DefaultEventNodes.Add(FName("K2_OnLeave"));
}

#undef LOCTEXT_NAMESPACE
