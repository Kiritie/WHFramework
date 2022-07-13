// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "BehaviorTree/BTTaskNode.h"
#include "AITaskBase.generated.h"

class AAIControllerBase;
class ACharacterBase;

/**
 * AI任务基类
 */
UCLASS()
class WHFRAMEWORK_API UAITaskBase : public UBTTaskNode
{
	GENERATED_UCLASS_BODY()

protected:
	UPROPERTY(VisibleAnywhere, Category = "Default")
	AAIControllerBase* OwnerController;

	UPROPERTY(VisibleAnywhere, Category = "Default")
	ACharacterBase* OwnerCharacter;
	
protected:
	virtual bool InitTask(UBehaviorTreeComponent& OwnerComp);

	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

	virtual void OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult) override;

public:
	template<class T>
	T* GetOwnerController() const
	{
		return Cast<T>(OwnerController);
	}

	AAIControllerBase* GetOwnerController() const { return OwnerController; }

	template<class T>
	T* GetOwnerCharacter() const
	{
		return Cast<T>(OwnerCharacter);
	}

	ACharacterBase* GetOwnerCharacter() const { return OwnerCharacter; }
};
