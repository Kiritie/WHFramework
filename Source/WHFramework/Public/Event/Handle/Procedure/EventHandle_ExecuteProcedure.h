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
	UPROPERTY(BlueprintReadOnly)
	class UProcedureBase* Procedure;

public:
	virtual void OnDespawn_Implementation() override;

	virtual void Fill_Implementation(const TArray<FParameter>& InParams) override;
};
