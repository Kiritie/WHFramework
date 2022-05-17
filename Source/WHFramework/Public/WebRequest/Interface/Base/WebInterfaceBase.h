// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ObjectPool/ObjectPoolInterface.h"
#include "UObject/Object.h"
#include "WebRequest/WebRequestModuleTypes.h"
#include "WebInterfaceBase.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UWebInterfaceBase : public UObject, public IObjectPoolInterface
{
	GENERATED_BODY()

public:
	UWebInterfaceBase();

public:
	virtual void OnSpawn_Implementation(const TArray<FParameter>& InParams) override;

	virtual void OnDespawn_Implementation() override;

public:
	UFUNCTION(BlueprintNativeEvent)
	void OnRequestComplete(FWebRequestInfo InWebRequestInfo);

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName Name;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString Url;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSubclassOf<UWebRequestHandleBase> Handle;

public:
	FOnWebRequestCompleteMulti OnWebRequestComplete;
};
