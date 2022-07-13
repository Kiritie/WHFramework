// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "BehaviorTree/BTService.h"
#include "AIServiceBase.generated.h"

class ACharacterBase;
class AAIControllerBase;

/**
 */
UCLASS()
class WHFRAMEWORK_API UAIServiceBase : public UBTService
{
	GENERATED_UCLASS_BODY()
	
protected:
	UPROPERTY(VisibleAnywhere, Category = "Default")
	AAIControllerBase* OwnerController;

	UPROPERTY(VisibleAnywhere, Category = "Default")
	ACharacterBase* OwnerCharacter;

protected:
	virtual bool InitService(UBehaviorTreeComponent& OwnerComp);

	virtual void OnBecomeRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

	virtual void OnCeaseRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

public:
	template<class T>
	T* GetOwnerController() const
	{
		return Cast<T>(OwnerController);
	}

	AAIControllerBase* GetOwnerController() const { return OwnerController; }

	template<class T>
	T* GetOwnerCharacter() const
	{
		return Cast<T>(OwnerCharacter);
	}

	ACharacterBase* GetOwnerCharacter() const { return OwnerCharacter; }
};
