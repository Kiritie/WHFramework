// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Base/WebRequestHandleBase.h"
#include "UObject/Object.h"
#include "WebRequestHandle_GetTexture.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UWebRequestHandle_GetTexture : public UWebRequestHandleBase
{
	GENERATED_BODY()

public:
	UWebRequestHandle_GetTexture();

public:
	virtual void OnDespawn_Implementation(bool bRecovery) override;

public:
	virtual void Fill(const FWebRequestResult& InResult, const TArray<FParameter>& InParams) override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	UTexture2D* Texture;
	
public:
	UFUNCTION(BlueprintPure)
	UTexture2D* GetTexture() const { return Texture; }
};
