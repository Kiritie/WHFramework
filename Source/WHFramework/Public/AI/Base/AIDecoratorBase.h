#pragma once

#include "AI/Base/AIAgentInterface.h"
#include "AI/Base/AIControllerBase.h"
#include "BehaviorTree/BTDecorator.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/Pawn.h"
#include "GameplayTagContainer.h"
#include "AIDecoratorBase.generated.h"

class AAIControllerBase;
class UAbilitySystemComponent;
class UBlackboardComponent;

/** Observes the data that defines the condition; it never starts or stops gameplay. */
UCLASS()
class WHFRAMEWORK_API UAIDecoratorBase : public UBTDecorator
{
	GENERATED_BODY()

public:
	UAIDecoratorBase(const FObjectInitializer& ObjectInitializer);
	virtual void BeginDestroy() override;

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
	virtual bool InitDecorator(UBehaviorTreeComponent& OwnerComp);
	virtual bool InitDecorator(UBehaviorTreeComponent& OwnerComp) const;
	virtual bool CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const override;
	virtual void OnBecomeRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void OnCeaseRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual void GetObservedKeys(TArray<FName>& OutKeys) const;
	virtual AActor* GetObservedTarget(UBehaviorTreeComponent& OwnerComp) const;

	UPROPERTY(EditDefaultsOnly, Category = "Observation", meta = (ClampMin = "0.05"))
	float ContinuousObservationInterval = 0.2f;

	/** Distance, team and selected item checks require sampling in addition to events. */
	UPROPERTY(EditDefaultsOnly, Category = "Observation")
	bool bObserveContinuousInputs = false;

	UPROPERTY(Transient)
	TWeakObjectPtr<AAIControllerBase> Controller;

	UPROPERTY(Transient)
	TWeakObjectPtr<APawn> Agent;

private:
	TWeakObjectPtr<UBehaviorTreeComponent> Behavior;
	TWeakObjectPtr<UBlackboardComponent> ObservedBlackboard;
	TWeakObjectPtr<AActor> ObservedTarget;
	TWeakObjectPtr<APawn> ObservedPawn;
	TWeakObjectPtr<UAbilitySystemComponent> AgentAbilitySystem;
	TWeakObjectPtr<UAbilitySystemComponent> TargetAbilitySystem;
	FDelegateHandle AgentTagHandle;
	FDelegateHandle TargetTagHandle;
	float ObservationRemaining = 0.f;
	bool bObserving = false;
	bool bLastCondition = false;
	bool bHadTarget = false;
	bool bHadPawn = false;

	EBlackboardNotificationResult OnBlackboardChanged(const UBlackboardComponent& Blackboard, FBlackboard::FKey ChangedKeyID);
	void OnGameplayTagChanged(const FGameplayTag InTag, int32 NewCount);
	bool RefreshObservedSources(UBehaviorTreeComponent& OwnerComp);
	void EvaluateAndRequest(UBehaviorTreeComponent& OwnerComp);
	void ClearObservers();
};
