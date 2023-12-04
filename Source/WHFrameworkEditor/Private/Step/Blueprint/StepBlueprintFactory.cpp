// Copyright Epic Games, Inc. All Rights Reserved.

#include "Step/Blueprint/StepBlueprintFactory.h"

#include "Step/Base/StepBase.h"
#include "Step/Base/StepBlueprint.h"
#include "Step/Blueprint/StepGraph.h"
#include "Step/Blueprint/StepGraphSchema.h"

#define LOCTEXT_NAMESPACE "StepBlueprintFactory"

UStepBlueprintFactory::UStepBlueprintFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UStepBlueprint::StaticClass();
	ParentClass = UStepBase::StaticClass();

	GraphClass = UStepGraph::StaticClass();
	GraphSchemaClass = UStepGraphSchema::StaticClass();

	DefaultEventGraph = FName("Step Graph");

	DefaultEventNodes.Add(FName("K2_OnInitialize"));
	DefaultEventNodes.Add(FName("K2_OnRestore"));
	DefaultEventNodes.Add(FName("K2_OnEnter"));
	DefaultEventNodes.Add(FName("K2_OnRefresh"));
	DefaultEventNodes.Add(FName("K2_OnGuide"));
	DefaultEventNodes.Add(FName("K2_OnExecute"));
	DefaultEventNodes.Add(FName("K2_OnComplete"));
	DefaultEventNodes.Add(FName("K2_OnLeave"));
}

#undef LOCTEXT_NAMESPACE
