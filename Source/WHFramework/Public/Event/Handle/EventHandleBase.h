// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "Event/EventModuleTypes.h"
#include "ObjectPool/ObjectPoolInterface.h"
#include "Parameter/ParameterModuleTypes.h"

#include "EventHandleBase.generated.h"

/**
 * 
 */
UCLASS(Blueprintable)
class WHFRAMEWORK_API UEventHandleBase : public UObject, public IObjectPoolInterface
{
	GENERATED_BODY()
	
public:
	UEventHandleBase();

public:
	UPROPERTY(BlueprintReadOnly)
	EEventType EventType;

public:
	virtual void Reset_Implementation() override;
	
	UFUNCTION(BlueprintNativeEvent, BlueprintPure)
	bool Filter(UObject* InOwner, const FName InFuncName);

	UFUNCTION(BlueprintNativeEvent)
	void Fill(const TArray<FParameter>& InParameters);
};
