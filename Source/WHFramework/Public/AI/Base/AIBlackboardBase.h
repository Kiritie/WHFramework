// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BlackboardData.h"
#include "AI/AIModuleTypes.h"
#include "Common/CommonTypes.h"
#include "AI/Base/AIAgentInterface.h"
#include "AIBlackboardBase.generated.h"

class AAIControllerBase;
class IAIAgentInterface;
/**
 * AI黑板基类
 */
UCLASS()
class WHFRAMEWORK_API UAIBlackboardBase : public UBlackboardData
{
	GENERATED_BODY()

protected:
	virtual void PostLoad() override;

protected:
	virtual void OnInitialize();
	
	virtual void OnReset();

	virtual void OnRefresh();

	virtual void OnValueReset(FName InValueName);

	virtual void OnValuePreChange(FName InValueName);

	virtual void OnValueChanged(FName InValueName);

public:
	virtual void Initialize(UBlackboardComponent* InComponent);

	virtual void Reset();

	virtual void ResetValue(FName InValueName);

	virtual void Refresh();

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
	
	UPROPERTY()
	AAIControllerBase* Controller;

	IAIAgentInterface* Agent;
	
public:
	UFUNCTION(BlueprintPure)
	UBlackboardComponent* GetComponent() const { return Component; }

	template<class T>
	T* GetController() const
	{
		return Cast<T>(Controller);
	}

	UFUNCTION(BlueprintPure)
	AAIControllerBase* GetController() const { return Controller; }

	template<class T>
	T* GetAgent() const
	{
		return Cast<T>(Agent);
	}

	IAIAgentInterface* GetAgent() const { return Agent; }

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"))
	APawn* GetAgent(TSubclassOf<APawn> InClass) const;
};
