// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Global/Base/WHObject.h"
#include "ObjectPool/ObjectPoolInterface.h"
#include "UObject/Object.h"
#include "WebRequest/WebRequestModuleTypes.h"
#include "WebRequestHandleBase.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UWebRequestHandleBase : public UWHObject, public IObjectPoolInterface
{
	GENERATED_BODY()

public:
	UWebRequestHandleBase();

public:
	virtual void OnDespawn_Implementation() override;

public:
	virtual void Fill(const FWebRequestResult& InWebRequestResult);

protected:
	UPROPERTY(VisibleAnywhere)
	bool bSucceeded;
	
public:
	UFUNCTION(BlueprintPure)
	bool IsSucceeded() const { return bSucceeded; }

	UFUNCTION(BlueprintCallable)
	void SetSucceeded(bool bInSucceeded) { this->bSucceeded = bInSucceeded; }
};
