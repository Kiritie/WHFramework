// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "BehaviorTree/BTDecorator.h"
#include "Common/CommonTypes.h"
#include "AIDecoratorBase.generated.h"

class IAIAgentInterface;
class AAIControllerBase;
/**
 */
UCLASS()
class WHFRAMEWORK_API UAIDecoratorBase : public UBTDecorator
{
	GENERATED_UCLASS_BODY()
	
protected:
	virtual bool InitDecorator(UBehaviorTreeComponent& OwnerComp);

	virtual bool InitDecorator(UBehaviorTreeComponent& OwnerComp) const;

	virtual bool CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const override;

	virtual void OnNodeActivation(FBehaviorTreeSearchData& SearchData) override;

	virtual void OnNodeDeactivation(FBehaviorTreeSearchData& SearchData, EBTNodeResult::Type NodeResult) override;

	virtual void OnNodeProcessed(FBehaviorTreeSearchData& SearchData, EBTNodeResult::Type& NodeResult) override;

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
	APawn* GetAgent(TSubclassOf<APawn> InClass) const { return GetDeterminesOutputObject(Cast<APawn>(Agent), InClass); }
};
