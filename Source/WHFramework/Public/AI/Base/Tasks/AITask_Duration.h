#pragma once
#include "AI/Base/AITaskBase.h"
#include "AITask_Duration.generated.h"

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
	virtual void OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult) override;

	UPROPERTY(EditAnywhere)
	float DurationTime = 1.f;

	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0"))
	float RandomDeviation = 0.f;

	float LocalRemainTime = 0.f;
};
