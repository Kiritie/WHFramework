#include "AI/Base/AIDecoratorBase.h"

#include "AI/Base/AIControllerBase.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"

UAIDecoratorBase::UAIDecoratorBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNodeInstance = true;
	bNotifyBecomeRelevant = true;
	bNotifyCeaseRelevant = true;
	bNotifyTick = true;
	FlowAbortMode = EBTFlowAbortMode::Both;
}

void UAIDecoratorBase::BeginDestroy()
{
	ClearObservers();
	Super::BeginDestroy();
}

bool UAIDecoratorBase::InitDecorator(UBehaviorTreeComponent& OwnerComp)
{
	Controller = Cast<AAIControllerBase>(OwnerComp.GetAIOwner());
	Agent = Controller.IsValid() ? Controller->GetPawn() : nullptr;
	return Agent.IsValid() && Agent->HasAuthority() && GetAgent() != nullptr;
}

bool UAIDecoratorBase::InitDecorator(UBehaviorTreeComponent& OwnerComp) const
{
	return const_cast<ThisClass*>(this)->InitDecorator(OwnerComp);
}

bool UAIDecoratorBase::CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const
{
	return InitDecorator(OwnerComp);
}

void UAIDecoratorBase::GetObservedKeys(TArray<FName>& OutKeys) const
{
}

AActor* UAIDecoratorBase::GetObservedTarget(UBehaviorTreeComponent& OwnerComp) const
{
	return nullptr;
}

void UAIDecoratorBase::OnBecomeRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	ClearObservers();
	Super::OnBecomeRelevant(OwnerComp, NodeMemory);
	Behavior = &OwnerComp;
	ObservedBlackboard = OwnerComp.GetBlackboardComponent();
	bObserving = true;
	ObservationRemaining = 0.f;
	InitDecorator(OwnerComp);
	if (UBlackboardComponent* Blackboard = ObservedBlackboard.Get())
	{
		TArray<FName> Keys;
		GetObservedKeys(Keys);
		for (const FName Name : Keys)
		{
			const FBlackboard::FKey Key = Blackboard->GetKeyID(Name);
			if (Key != FBlackboard::InvalidKey)
			{
				Blackboard->RegisterObserver(Key, this, FOnBlackboardChangeNotification::CreateUObject(this, &ThisClass::OnBlackboardChanged));
			}
		}
	}
	RefreshObservedSources(OwnerComp);
	bLastCondition = CalculateRawConditionValue(OwnerComp, NodeMemory);
}

void UAIDecoratorBase::OnCeaseRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	ClearObservers();
	Super::OnCeaseRelevant(OwnerComp, NodeMemory);
}

void UAIDecoratorBase::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	if (!bObserving)
	{
		return;
	}
	ObservationRemaining -= DeltaSeconds;
	if (ObservationRemaining > 0.f)
	{
		return;
	}
	ObservationRemaining = FMath::Max(0.05f, ContinuousObservationInterval);
	const bool bSourcesChanged = RefreshObservedSources(OwnerComp);
	if (bObserveContinuousInputs || bSourcesChanged)
	{
		EvaluateAndRequest(OwnerComp);
	}
}

bool UAIDecoratorBase::RefreshObservedSources(UBehaviorTreeComponent& OwnerComp)
{
	InitDecorator(OwnerComp);
	APawn* Pawn = Agent.Get();
	AActor* Target = GetObservedTarget(OwnerComp);
	bool bChanged = Pawn != ObservedPawn.Get() || Target != ObservedTarget.Get() ||
		bHadPawn != IsValid(Pawn) || bHadTarget != IsValid(Target);
	ObservedPawn = Pawn;
	ObservedTarget = Target;
	bHadPawn = IsValid(Pawn);
	bHadTarget = IsValid(Target);

	UAbilitySystemComponent* AgentASC = IsValid(Pawn) ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn) : nullptr;
	UAbilitySystemComponent* TargetASC = IsValid(Target) ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target) : nullptr;
	if (TargetASC == AgentASC)
	{
		TargetASC = nullptr;
	}
	if (AgentASC != AgentAbilitySystem.Get() || (AgentASC && !AgentTagHandle.IsValid()) ||
		(!AgentASC && AgentTagHandle.IsValid()))
	{
		bChanged = true;
		if (AgentAbilitySystem.IsValid() && AgentTagHandle.IsValid())
		{
			AgentAbilitySystem->RegisterGenericGameplayTagEvent().Remove(AgentTagHandle);
		}
		AgentTagHandle.Reset();
		AgentAbilitySystem = AgentASC;
		if (AgentASC)
		{
			AgentTagHandle = AgentASC->RegisterGenericGameplayTagEvent().AddUObject(this, &ThisClass::OnGameplayTagChanged);
		}
	}
	if (TargetASC != TargetAbilitySystem.Get() || (TargetASC && !TargetTagHandle.IsValid()) ||
		(!TargetASC && TargetTagHandle.IsValid()))
	{
		bChanged = true;
		if (TargetAbilitySystem.IsValid() && TargetTagHandle.IsValid())
		{
			TargetAbilitySystem->RegisterGenericGameplayTagEvent().Remove(TargetTagHandle);
		}
		TargetTagHandle.Reset();
		TargetAbilitySystem = TargetASC;
		if (TargetASC)
		{
			TargetTagHandle = TargetASC->RegisterGenericGameplayTagEvent().AddUObject(this, &ThisClass::OnGameplayTagChanged);
		}
	}
	return bChanged;
}

EBlackboardNotificationResult UAIDecoratorBase::OnBlackboardChanged(const UBlackboardComponent& Blackboard, FBlackboard::FKey ChangedKeyID)
{
	UBehaviorTreeComponent* OwnerComp = Behavior.Get();
	if (!bObserving || !OwnerComp || ObservedBlackboard.Get() != &Blackboard)
	{
		return EBlackboardNotificationResult::RemoveObserver;
	}
	RefreshObservedSources(*OwnerComp);
	EvaluateAndRequest(*OwnerComp);
	return bObserving ? EBlackboardNotificationResult::ContinueObserving : EBlackboardNotificationResult::RemoveObserver;
}

void UAIDecoratorBase::OnGameplayTagChanged(const FGameplayTag InTag, int32 NewCount)
{
	if (bObserving)
	{
		if (UBehaviorTreeComponent* OwnerComp = Behavior.Get())
		{
			EvaluateAndRequest(*OwnerComp);
		}
	}
}

void UAIDecoratorBase::EvaluateAndRequest(UBehaviorTreeComponent& OwnerComp)
{
	const bool bCondition = CalculateRawConditionValue(OwnerComp, nullptr);
	if (bCondition != bLastCondition)
	{
		bLastCondition = bCondition;
		OwnerComp.RequestExecution(this);
	}
}

void UAIDecoratorBase::ClearObservers()
{
	bObserving = false;
	if (ObservedBlackboard.IsValid())
	{
		ObservedBlackboard->UnregisterObserversFrom(this);
	}
	if (AgentAbilitySystem.IsValid() && AgentTagHandle.IsValid())
	{
		AgentAbilitySystem->RegisterGenericGameplayTagEvent().Remove(AgentTagHandle);
	}
	if (TargetAbilitySystem.IsValid() && TargetTagHandle.IsValid())
	{
		TargetAbilitySystem->RegisterGenericGameplayTagEvent().Remove(TargetTagHandle);
	}
	AgentTagHandle.Reset();
	TargetTagHandle.Reset();
	AgentAbilitySystem.Reset();
	TargetAbilitySystem.Reset();
	ObservedBlackboard.Reset();
	ObservedTarget.Reset();
	ObservedPawn.Reset();
	Behavior.Reset();
	Controller.Reset();
	Agent.Reset();
	bHadTarget = false;
	bHadPawn = false;
}

APawn* UAIDecoratorBase::GetAgent(TSubclassOf<APawn> InClass) const
{
	APawn* Pawn = Agent.Get();
	return Pawn && (!InClass || Pawn->IsA(InClass)) ? Pawn : nullptr;
}
