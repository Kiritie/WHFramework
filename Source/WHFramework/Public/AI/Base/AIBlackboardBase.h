// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BlackboardData.h"
#include "AI/AIModuleTypes.h"
#include "AIBlackboardBase.generated.h"

class ACharacterBase;
/**
 * AI黑板基类
 */
UCLASS()
class WHFRAMEWORK_API UAIBlackboardBase : public UBlackboardData
{

private:
	GENERATED_BODY()

public:
	virtual void PostLoad() override;

	virtual void OnInitialize(UBlackboardComponent* InComponent, ACharacterBase* InOwnerCharacter);

	virtual void OnRefresh();

	virtual void OnValuePreChange(FName InValueName);

	virtual void OnValueChanged(FName InValueName);

public:
	FOnBlackboardValuePreChange OnBlackboardValuePreChange;
	FOnBlackboardValueChanged OnBlackboardValueChanged;

public:
	BLACKBOARD_VALUE_ACCESSORS_BOOL(IsLostTarget);
	BLACKBOARD_VALUE_ACCESSORS_VECTOR(TargetLocation);
	BLACKBOARD_VALUE_ACCESSORS_OBJECT(TargetCharacter);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	UBlackboardComponent* Component;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	ACharacterBase* Character;
	
public:
	UBlackboardComponent* GetComponent() const { return Component; }

	template<class T>
	T* GetCharacter() const
	{
		return Cast<T>(Character);
	}

	ACharacterBase* GetCharacter() const { return Character; }
};
