// Copyright Epic Games, Inc. All Rights Reserved.

#include "Task/Blueprint/TaskBlueprintFactory.h"

#include "Task/Base/TaskBase.h"
#include "Task/Base/TaskBlueprint.h"
#include "Task/Blueprint/TaskGraph.h"
#include "Task/Blueprint/TaskGraphSchema.h"

#define LOCTEXT_NAMESPACE "TaskBlueprintFactory"

UTaskBlueprintFactory::UTaskBlueprintFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UTaskBlueprint::StaticClass();
	ParentClass = UTaskBase::StaticClass();

	GraphClass = UTaskGraph::StaticClass();
	GraphSchemaClass = UTaskGraphSchema::StaticClass();

	DefaultEventGraph = FName("Task Graph");

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
