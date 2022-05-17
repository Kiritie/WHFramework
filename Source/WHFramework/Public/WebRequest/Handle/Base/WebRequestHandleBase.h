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
	virtual void OnSpawn_Implementation(const TArray<FParameter>& InParams) override;

	virtual void OnDespawn_Implementation() override;

public:
	void Fill(FWebRequestInfo InWebRequestInfo);

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	bool bSucceeded;
};
