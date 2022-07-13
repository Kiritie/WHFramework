// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "AIController.h"
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
class WHFRAMEWORK_API AAIControllerBase : public AAIController
{
	GENERATED_BODY()

public:
    AAIControllerBase();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UAIPerceptionComponent* AIPerception;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Default")
	UBehaviorTree* BehaviorTreeAsset;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Default")
	UAIBlackboardBase* BlackboardAsset;

protected:
	virtual void OnPossess(APawn* InPawn) override;

	virtual void OnUnPossess() override;

	UFUNCTION()
	virtual void OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

public:
	virtual void Tick(float DeltaTime) override;
	
	virtual void InitBehaviorTree(UBehaviorTree* InBehaviorTreeAsset, ACharacterBase* InCharacter);

	virtual bool RunBehaviorTree();

	virtual bool RunBehaviorTree(UBehaviorTree* BTAsset) override;

	virtual void StopBehaviorTree();

public:
	UAIPerceptionComponent* GetAIPerception() const { return AIPerception; }

	UBehaviorTree* GetBehaviorTree() const { return BehaviorTreeAsset; }

	template<class T>
	T* GetBlackboard() const
	{
		return  Cast<T>(BlackboardAsset);
	}

	UAIBlackboardBase* GetBlackboard() const { return BlackboardAsset; }

	UBehaviorTreeComponent* GetBehaviorTreeComponent() const;
};
