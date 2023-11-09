// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Common/Base/WHObject.h"
#include "UObject/Object.h"
#include "WebRequest/WebRequestModuleStatics.h"
#include "WebRequest/WebRequestModuleTypes.h"
#include "WebInterfaceBase.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UWebInterfaceBase : public UWHObject
{
	GENERATED_BODY()

public:
	UWebInterfaceBase();

public:
	virtual void OnDespawn_Implementation(bool bRecovery) override;

protected:
	UFUNCTION(BlueprintNativeEvent)
	void OnRequestComplete(UWebRequestHandleBase* InWebRequestHandle);

public:
	void RequestComplete(FWebRequestResult InResult, const TArray<FParameter>& InParams);

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName Name;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString Url;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString FullUrl;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSubclassOf<UWebRequestHandleBase> HandleClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TMap<FString, UWebRequestHandleBase*> HandleMap;

	FOnWebRequestCompleteMulti OnWebRequestComplete;
	
public:
	FName GetName() const { return Name; }
	
	FString GetUrl() const { return Url; }
		
	FString GetFullUrl() const;

	TSubclassOf<UWebRequestHandleBase> GetHandleClass() const { return HandleClass; }

	FOnWebRequestCompleteMulti& GetOnWebRequestComplete() { return OnWebRequestComplete; }
};
