#pragma once
#include "BehaviorTree/BTService.h"
#include "AI/Base/AIAgentInterface.h"
#include "AI/Base/AIControllerBase.h"
#include "GameFramework/Pawn.h"
#include "AIServiceBase.generated.h"

class AAIControllerBase;

UCLASS()
class WHFRAMEWORK_API UAIServiceBase : public UBTService
{
	GENERATED_BODY()

public:
	UAIServiceBase(const FObjectInitializer& ObjectInitializer);

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
	virtual bool InitService(UBehaviorTreeComponent& OwnerComp);
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual void OnSearchStart(FBehaviorTreeSearchData& SearchData) override;
	virtual void OnBecomeRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void OnCeaseRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

	UPROPERTY(Transient)
	TWeakObjectPtr<AAIControllerBase> Controller;

	UPROPERTY(Transient)
	TWeakObjectPtr<APawn> Agent;
};
