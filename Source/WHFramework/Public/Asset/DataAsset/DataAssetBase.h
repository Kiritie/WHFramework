// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DataAssetBase.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UDataAssetBase : public UDataAsset
{
	GENERATED_BODY()

public:
	UDataAssetBase();

public:
	UFUNCTION(BlueprintCallable)
	virtual void Fill(const FString& InJsonData);

	UFUNCTION(BlueprintPure)
	virtual FString Pack();

protected:
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly)
	FName DataAssetName;
	
public:
	UFUNCTION(BlueprintPure)
	virtual FName GetDataAssetName() const { return DataAssetName; }
};
