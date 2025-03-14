// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Common/Base/WHActor.h"
#include "GameFramework/PlayerState.h"
#include "WHPlayerState.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API AWHPlayerState : public APlayerState, public IWHActorInterface
{
	GENERATED_BODY()

public:
	AWHPlayerState();

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
