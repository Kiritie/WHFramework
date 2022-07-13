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

	virtual void Initialize(UBlackboardComponent* InComponent, ACharacterBase* InOwnerCharacter);

	virtual void Refresh();

protected:
	UPROPERTY()
	UBlackboardComponent* Component;
	
	UPROPERTY()
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
