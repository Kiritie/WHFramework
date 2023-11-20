// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "BehaviorTree/BTTaskNode.h"
#include "Common/CommonTypes.h"
#include "AITaskBase.generated.h"

class AAIControllerBase;
class IAIAgentInterface;

/**
 * AI任务基类
 */
UCLASS()
class WHFRAMEWORK_API UAITaskBase : public UBTTaskNode
{
	GENERATED_UCLASS_BODY()
	
protected:
	virtual void InitializeFromAsset(UBehaviorTree& Asset) override;

	virtual bool InitTask(UBehaviorTreeComponent& OwnerComp);

	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

	virtual void OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult) override;

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
	APawn* GetAgent(TSubclassOf<APawn> InClass) const { return GetDeterminesOutputType(Cast<APawn>(Agent), InClass); }
};
