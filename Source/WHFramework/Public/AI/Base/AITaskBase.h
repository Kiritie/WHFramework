#pragma once
#include "BehaviorTree/BTTaskNode.h"
#include "AI/Base/AIAgentInterface.h"
#include "AI/Base/AIControllerBase.h"
#include "GameFramework/Pawn.h"
#include "AITaskBase.generated.h"

class AAIControllerBase;

UCLASS()
class WHFRAMEWORK_API UAITaskBase : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UAITaskBase(const FObjectInitializer& ObjectInitializer);

	template<class T>
	T* GetController() const
	{
		return Cast<T>(Controller.Get());
	}

	AAIControllerBase* GetController() const { return Controller.Get(); }

	template<class T>
	T* GetAgent() const
	{
		return Cast<T>(Agent.Get());
	}

	IAIAgentInterface* GetAgent() const { return Cast<IAIAgentInterface>(Agent.Get()); }
	APawn* GetAgent(TSubclassOf<APawn> InClass) const;

protected:
	virtual void InitializeFromAsset(UBehaviorTree& Asset) override;
	virtual bool InitTask(UBehaviorTreeComponent& OwnerComp);
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult) override;
	virtual bool FinishTaskOnce(UBehaviorTreeComponent& OwnerComp, EBTNodeResult::Type InResult);
	bool FinishTaskOnce(UBehaviorTreeComponent& OwnerComp, EBTNodeResult::Type InResult, uint64 ExpectedGeneration);
	bool IsTaskActive() const { return bTaskActive; }
	uint64 GetTaskGeneration() const { return TaskGeneration; }

	UPROPERTY(Transient)
	TWeakObjectPtr<AAIControllerBase> Controller;

	UPROPERTY(Transient)
	TWeakObjectPtr<APawn> Agent;

	bool bTaskActive = false;
	uint64 TaskGeneration = 0;
};
