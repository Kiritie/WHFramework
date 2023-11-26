// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BlackboardData.h"
#include "AI/AIModuleTypes.h"
#include "Common/CommonTypes.h"
#include "AIBlackboardBase.generated.h"

class IAIAgentInterface;
/**
 * AI黑板基类
 */
UCLASS()
class WHFRAMEWORK_API UAIBlackboardBase : public UBlackboardData
{
	GENERATED_BODY()

public:
	virtual void PostLoad() override;

	virtual void Initialize(UBlackboardComponent* InComponent, IAIAgentInterface* InAgent);

	virtual void Refresh();

protected:
	virtual void OnRefresh();

	virtual void OnValuePreChange(FName InValueName);

	virtual void OnValueChanged(FName InValueName);

public:
	BLACKBOARD_VALUE_ACCESSORS_BOOL(IsLostTarget);
	BLACKBOARD_VALUE_ACCESSORS_VECTOR(TargetLocation);
	BLACKBOARD_VALUE_ACCESSORS_OBJECT(TargetAgent);

public:
	FOnBlackboardValuePreChange OnBlackboardValuePreChange;
	FOnBlackboardValueChanged OnBlackboardValueChanged;

protected:
	UPROPERTY()
	UBlackboardComponent* Component;
	
	IAIAgentInterface* Agent;
	
public:
	UFUNCTION(BlueprintPure)
	UBlackboardComponent* GetComponent() const { return Component; }

	template<class T>
	T* GetAgent() const
	{
		return Cast<T>(Agent);
	}

	IAIAgentInterface* GetAgent() const { return Agent; }

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"))
	APawn* GetAgent(TSubclassOf<APawn> InClass) const { return GetDeterminesOutputObject(Cast<APawn>(Agent), InClass); }
};
