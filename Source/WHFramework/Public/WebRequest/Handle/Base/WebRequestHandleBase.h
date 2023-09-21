// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Common/Base/WHObject.h"
#include "UObject/Object.h"
#include "WebRequest/WebRequestModuleTypes.h"
#include "WebRequestHandleBase.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UWebRequestHandleBase : public UWHObject
{
	GENERATED_BODY()

public:
	UWebRequestHandleBase();

public:
	virtual void OnDespawn_Implementation(bool bRecovery) override;

public:
	virtual void Fill(const FWebRequestResult& InResult, const TArray<FParameter>& InParams);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	bool bSucceeded;
	
public:
	UFUNCTION(BlueprintPure)
	bool IsSucceeded() const { return bSucceeded; }
};
