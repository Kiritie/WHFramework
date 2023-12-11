// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Base/WebRequestHandleBase.h"
#include "UObject/Object.h"
#include "WebRequestHandle_GetText.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UWebRequestHandle_GetText : public UWebRequestHandleBase
{
	GENERATED_BODY()

public:
	UWebRequestHandle_GetText();

public:
	virtual void OnDespawn_Implementation(bool bRecovery) override;

public:
	virtual void Parse(const FWebRequestResult& InResult, const TArray<FParameter>& InParams) override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	FString Text;
	
public:
	UFUNCTION(BlueprintPure)
	FString GetText() const { return Text; }
};
