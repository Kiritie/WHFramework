// Copyright Epic Games, Inc. All Rights Reserved.

#include "Main/Blueprint/ModuleBlueprintFactory.h"

#include "Main/Base/ModuleBase.h"
#include "Main/Base/ModuleBlueprint.h"
#include "Main/Blueprint/ModuleGraph.h"
#include "Main/Blueprint/ModuleGraphSchema.h"

#define LOCTEXT_NAMESPACE "ModuleBlueprintFactory"

UModuleBlueprintFactory::UModuleBlueprintFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UModuleBlueprint::StaticClass();
	ParentClass = UModuleBase::StaticClass();

	GraphClass = UModuleGraph::StaticClass();
	GraphSchemaClass = UModuleGraphSchema::StaticClass();

	DefaultEventGraph = FName("Module Graph");

	DefaultEventNodes.Add(FName("K2_OnInitialize"));
	DefaultEventNodes.Add(FName("K2_OnPreparatory"));
	DefaultEventNodes.Add(FName("K2_OnReset"));
	DefaultEventNodes.Add(FName("K2_OnPause"));
	DefaultEventNodes.Add(FName("K2_OnUnPause"));
	DefaultEventNodes.Add(FName("K2_OnRefresh"));
	DefaultEventNodes.Add(FName("K2_OnTermination"));
}

#undef LOCTEXT_NAMESPACE
