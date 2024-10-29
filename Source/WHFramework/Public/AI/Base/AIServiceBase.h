// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "BehaviorTree/BTService.h"
#include "Common/CommonTypes.h"
#include "AIServiceBase.generated.h"

class IAIAgentInterface;
class AAIControllerBase;

/**
 */
UCLASS()
class WHFRAMEWORK_API UAIServiceBase : public UBTService
{
	GENERATED_BODY()

public:
	UAIServiceBase(const FObjectInitializer& ObjectInitializer);

protected:
	virtual bool InitService(UBehaviorTreeComponent& OwnerComp);

	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

	virtual void OnSearchStart(FBehaviorTreeSearchData& SearchData) override;

	virtual void OnBecomeRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

	virtual void OnCeaseRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

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
	APawn* GetAgent(TSubclassOf<APawn> InClass) const;
};
