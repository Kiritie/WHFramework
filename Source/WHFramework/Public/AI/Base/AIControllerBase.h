// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "AIController.h"
#include "Common/Base/WHActor.h"
#include "Perception/AIPerceptionTypes.h"
#include "AIControllerBase.generated.h"

class UBehaviorTreeComponent;
class ACharacterBase;
class UAIBlackboardBase;
class UFiniteStateBase;

DECLARE_DELEGATE_TwoParams(FAIPerceptionUpdated, ACharacterBase*, bool)

/**
 */
UCLASS()
class WHFRAMEWORK_API AAIControllerBase : public AAIController, public IWHActorInterface
{
	GENERATED_BODY()

public:
    AAIControllerBase();
	
	//////////////////////////////////////////////////////////////////////////
	/// WHActor
public:
	virtual void OnInitialize_Implementation() override;

	virtual void OnPreparatory_Implementation(EPhase InPhase) override;

	virtual void OnRefresh_Implementation(float DeltaSeconds) override;

	virtual void OnTermination_Implementation(EPhase InPhase) override;

protected:
	virtual bool IsDefaultLifecycle_Implementation() const override { return true; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	UBehaviorTree* BehaviorTreeAsset;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	UAIBlackboardBase* BlackboardAsset;

protected:
	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	virtual void OnPossess(APawn* InPawn) override;

	virtual void OnUnPossess() override;

	UFUNCTION()
	virtual void OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

public:
	virtual void Tick(float DeltaSeconds) override;
	
	virtual void InitBehaviorTree(UBehaviorTree* InBehaviorTreeAsset);

	virtual bool RunBehaviorTree(UBehaviorTree* BTAsset) override;

	virtual void StopBehaviorTree();

public:
	UBehaviorTree* GetBehaviorTree() const { return BehaviorTreeAsset; }

	template<class T>
	T* GetBlackboard() const
	{
		return  Cast<T>(BlackboardAsset);
	}

	UAIBlackboardBase* GetBlackboard() const { return BlackboardAsset; }

	UBehaviorTreeComponent* GetBehaviorTreeComponent() const;

	bool IsRunningBehaviorTree() const;
};
