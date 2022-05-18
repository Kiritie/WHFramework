// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ObjectPool/ObjectPoolInterface.h"
#include "UObject/Object.h"
#include "WebRequest/WebRequestModuleTypes.h"
#include "WebRequestHandleBase.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UWebRequestHandleBase : public UObject, public IObjectPoolInterface
{
	GENERATED_BODY()

public:
	UWebRequestHandleBase();

public:
	virtual void OnDespawn_Implementation() override;

public:
	virtual void Fill(const FWebRequestResult& InWebRequestResult);

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	bool bSucceeded;
};
