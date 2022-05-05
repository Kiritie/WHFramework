// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "SaveGameModuleTypes.generated.h"

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FSaveData
{
	GENERATED_BODY()

public:
	FORCEINLINE FSaveData()
	{
		bSaved = false;
		Datas = TArray<uint8>();
	}

	virtual ~FSaveData() { }

	UPROPERTY()
	bool bSaved;

	UPROPERTY()
	TArray<uint8> Datas;
};
