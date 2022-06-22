// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/GameStateBase.h"
#include "WHGameState.generated.h"

/**
 *
 */
UCLASS()
class WHFRAMEWORK_API AWHGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	AWHGameState();

	//////////////////////////////////////////////////////////////////////////
	/// Defaults
public:
	UFUNCTION(BlueprintNativeEvent)
	void OnInitialize();
	
	UFUNCTION(BlueprintNativeEvent)
	void OnPreparatory();

	//////////////////////////////////////////////////////////////////////////
	/// Inherits
protected:
	virtual void BeginPlay() override;
};
