// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/GameStateBase.h"
#include "WHGameState.generated.h"

/**
 * ��Ϸ״̬����
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
	void Initialize();
	
	//////////////////////////////////////////////////////////////////////////
	/// Inherits
protected:
	virtual void BeginPlay() override;
};
