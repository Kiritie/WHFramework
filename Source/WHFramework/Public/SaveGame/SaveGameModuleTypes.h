// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FSaveData
{
	GENERATED_BODY()

public:
	UPROPERTY()
	bool bSaved;

	UPROPERTY()
	TArray<uint8> Datas;

	FORCEINLINE FSaveData()
	{
		bSaved = false;
		Datas = TArray<uint8>();
	}
};
