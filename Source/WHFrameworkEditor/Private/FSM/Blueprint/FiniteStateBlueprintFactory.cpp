// Copyright Epic Games, Inc. All Rights Reserved.

#include "FSM/Blueprint/FiniteStateBlueprintFactory.h"

#include "FSM/Base/FiniteStateBase.h"
#include "FSM/Base/FiniteStateBlueprint.h"
#include "FSM/Blueprint/FiniteStateGraph.h"
#include "FSM/Blueprint/FiniteStateGraphSchema.h"

#define LOCTEXT_NAMESPACE "FiniteStateBlueprintFactory"

UFiniteStateBlueprintFactory::UFiniteStateBlueprintFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UFiniteStateBlueprint::StaticClass();
	ParentClass = UFiniteStateBase::StaticClass();

	GraphClass = UFiniteStateGraph::StaticClass();
	GraphSchemaClass = UFiniteStateGraphSchema::StaticClass();

	DefaultEventGraph = FName("Finite State Graph");

	DefaultEventNodes.Add(FName("K2_OnInitialize"));
	DefaultEventNodes.Add(FName("K2_OnEnterValidate"));
	DefaultEventNodes.Add(FName("K2_OnEnter"));
	DefaultEventNodes.Add(FName("K2_OnRefresh"));
	DefaultEventNodes.Add(FName("K2_OnLeaveValidate"));
	DefaultEventNodes.Add(FName("K2_OnLeave"));
	DefaultEventNodes.Add(FName("K2_OnTermination"));
}

#undef LOCTEXT_NAMESPACE
