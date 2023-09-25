// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "BehaviorTree/BTService.h"
#include "AIServiceBase.generated.h"

class ACharacterBase;
class AAIControllerBase;

/**
 */
UCLASS()
class WHFRAMEWORK_API UAIServiceBase : public UBTService
{
	GENERATED_UCLASS_BODY()

protected:
	virtual bool InitService(UBehaviorTreeComponent& OwnerComp);

	virtual void OnBecomeRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

	virtual void OnCeaseRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

protected:
	UPROPERTY()
	AAIControllerBase* Controller;
	
	IAIAgentInterface* Agent;
	
public:
	template<class T>
	T* GetController() const
	{
		return Cast<T>(Controller);
	}

	UFUNCTION(BlueprintPure)
	AAIControllerBase* GetController() const { return Controller; }

	template<class T>
	T* GetAgent() const
	{
		return Cast<T>(Agent);
	}

	IAIAgentInterface* GetAgent() const { return Agent; }

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"))
	APawn* GetAgent(TSubclassOf<APawn> InClass) const { return Cast<APawn>(Agent); }
};
