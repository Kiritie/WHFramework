// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Base/WebRequestHandleBase.h"
#include "UObject/Object.h"
#include "Dom/JsonObject.h"
#include "WebRequestHandle_GetJson.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UWebRequestHandle_GetJson : public UWebRequestHandleBase
{
	GENERATED_BODY()

public:
	UWebRequestHandle_GetJson();

public:
	virtual void OnDespawn_Implementation() override;

public:
	TSharedPtr<FJsonObject> Json;

public:
	virtual void Fill(const FWebRequestResult& InWebRequestResult) override;
};
