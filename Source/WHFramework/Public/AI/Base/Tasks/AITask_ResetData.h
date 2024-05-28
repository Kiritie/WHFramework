// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AI/Base/AITaskBase.h"
#include "AITask_ResetData.generated.h"

/**
 * AI任务_重置数据
 */
UCLASS()
class WHFRAMEWORK_API UAITask_ResetData : public UAITaskBase
{
	GENERATED_UCLASS_BODY()

protected:
	virtual void InitializeFromAsset(UBehaviorTree& Asset) override;
	
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

protected:
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	TArray<FBlackboardKeySelector> ResetKeys;
};
