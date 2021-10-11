// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Event/Handle/EventHandleBase.h"

#include "EventHandle_ExecuteProcedure.generated.h"

/**
 * 
 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UEventHandle_ExecuteProcedure : public UEventHandleBase
{
	GENERATED_BODY()

public:
	UEventHandle_ExecuteProcedure();

public:
	UPROPERTY(BlueprintReadOnly, EditAnywhere, meta = (InstanceEditable, ExposeOnSpawn))
	class AProcedureBase* Procedure;
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere, meta = (InstanceEditable, ExposeOnSpawn))
	FName FuncName;

public:
	virtual void OnDespawn_Implementation() override;

	virtual bool Filter_Implementation(UObject* InOwner, const FName InFuncName) override;

	virtual void Fill_Implementation(const TArray<FParameter>& InParameters) override;
};
