// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
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
	virtual void OnDespawn_Implementation() override;

public:
	virtual void Fill(const FWebRequestResult& InWebRequestResult) override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	FString Text;
	
public:
	UFUNCTION(BlueprintPure)
	FString GetText() const { return Text; }
};
