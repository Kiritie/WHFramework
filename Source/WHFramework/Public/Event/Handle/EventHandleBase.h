// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "Event/EventModuleTypes.h"
#include "Global/Base/WHObject.h"
#include "Parameter/ParameterModuleTypes.h"

#include "EventHandleBase.generated.h"

/**
 * 
 */
UCLASS(Blueprintable)
class WHFRAMEWORK_API UEventHandleBase : public UWHObject
{
	GENERATED_BODY()
	
public:
	UEventHandleBase();

public:
	UPROPERTY(BlueprintReadOnly)
	EEventType EventType;

public:
	virtual int32 GetLimit_Implementation() const override { return -1; }

	virtual void OnSpawn_Implementation(const TArray<FParameter>& InParams) override;
	
	virtual void OnDespawn_Implementation() override;

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintPure)
	bool Filter(UObject* InOwner, const FName InFuncName);

	UFUNCTION(BlueprintNativeEvent)
	void Fill(const TArray<FParameter>& InParams);
};
