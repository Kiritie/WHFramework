// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/GameStateBase.h"
#include "Common/Base/WHActor.h"
#include "WHGameState.generated.h"

/**
 *
 */
UCLASS()
class WHFRAMEWORK_API AWHGameState : public AGameStateBase, public IWHActorInterface
{
	GENERATED_BODY()

public:
	AWHGameState();

	//////////////////////////////////////////////////////////////////////////
	/// WHActor
public:
	virtual void OnInitialize_Implementation() override;

	virtual void OnPreparatory_Implementation() override;

	virtual void OnRefresh_Implementation(float DeltaSeconds) override;

	virtual void OnTermination_Implementation() override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "WHActor")
	bool bInitialized;
	
protected:
	virtual bool IsInitialized_Implementation() const override { return bInitialized; }
	
	virtual bool IsUseDefaultLifecycle_Implementation() const override { return true; }

protected:
	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	virtual void Tick(float DeltaSeconds) override;
};
