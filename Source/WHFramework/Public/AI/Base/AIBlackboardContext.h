#pragma once

#include "CoreMinimal.h"
#include "AI/AIModuleTypes.h"
#include "AI/Base/AIBlackboardBase.h"
#include "AI/Base/AIAgentInterface.h"
#include "AI/Base/AIControllerBase.h"
#include "GameFramework/Pawn.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AIBlackboardContext.generated.h"

class AAIControllerBase;
class UAIBlackboardBase;

UCLASS(BlueprintType)
class WHFRAMEWORK_API UAIBlackboardContext : public UObject
{
	GENERATED_BODY()

public:
	UAIBlackboardContext();
	virtual void Initialize(const UAIBlackboardBase* InDefinition, AAIControllerBase* InController, UBlackboardComponent* InComponent);
	virtual void Uninitialize();
	virtual void Reset();
	virtual void ResetValue(FName InValueName);
	virtual void Refresh();

	BLACKBOARD_VALUE_ACCESSORS_BOOL(IsLostTarget);
	BLACKBOARD_VALUE_ACCESSORS_VECTOR(TargetLocation);
	BLACKBOARD_VALUE_ACCESSORS_OBJECT(TargetAgent);

	FOnBlackboardValuePreChange OnBlackboardValuePreChange;
	FOnBlackboardValueChanged OnBlackboardValueChanged;

	UBlackboardComponent* GetComponent() const { return Component; }
	AAIControllerBase* GetController() const { return Controller.Get(); }
	const UAIBlackboardBase* GetDefinition() const { return Definition.Get(); }

	template<class T>
	T* GetController() const
	{
		return Cast<T>(Controller.Get());
	}

	template<class T>
	T* GetAgent() const
	{
		return Cast<T>(Agent.Get());
	}

	IAIAgentInterface* GetAgent() const { return Cast<IAIAgentInterface>(Agent.Get()); }

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"))
	APawn* GetAgent(TSubclassOf<APawn> InClass) const;

protected:
	virtual void OnReset();
	virtual void OnRefresh();
	virtual void OnValueReset(FName InValueName);
	virtual void OnValuePreChange(FName InValueName);
	virtual void OnValueChanged(FName InValueName);

	UPROPERTY(Transient)
	TObjectPtr<UBlackboardComponent> Component;

	UPROPERTY(Transient)
	TWeakObjectPtr<AAIControllerBase> Controller;

	UPROPERTY(Transient)
	TWeakObjectPtr<APawn> Agent;

	UPROPERTY(Transient)
	TWeakObjectPtr<UAIBlackboardBase> Definition;
};
