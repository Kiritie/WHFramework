// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "AI/Base/AITaskBase.h"
#include "AITask_Duration.generated.h"

/**
 * AI任务基类
 */
UCLASS()
class WHFRAMEWORK_API UAITask_Duration : public UAITaskBase
{
	GENERATED_BODY()
	
public:
	UAITask_Duration(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
		
protected:
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	float DurationTime;

	UPROPERTY(EditAnywhere, meta = (EditConditionHides, EditCondition = "EDC_RandomDeviation"), Category = "Blackboard")
	float RandomDeviation;

private:
	UFUNCTION()
	bool EDC_RandomDeviation() const;

private:
	float LocalRemainTime;
};
