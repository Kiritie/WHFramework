﻿// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Base/WebRequestHandleBase.h"
#include "UObject/Object.h"
#include "WebRequestHandle_DownloadFile.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UWebRequestHandle_DownloadFile : public UWebRequestHandleBase
{
	GENERATED_BODY()

public:
	UWebRequestHandle_DownloadFile();

public:
	virtual void OnDespawn_Implementation(bool bRecovery) override;

public:
	virtual void Parse(const FWebRequestResult& InResult, const TArray<FParameter>& InParams) override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	FString FilePath;
	
public:
	UFUNCTION(BlueprintPure)
	FString GetFilePath() const { return FilePath; }
};
