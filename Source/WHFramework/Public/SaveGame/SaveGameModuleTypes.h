// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "SaveGameModuleTypes.generated.h"

class USaveGameBase;
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

public:
	template<class T>
	T* To()
	{
		return static_cast<T*>(this);
	}

	template<class T>
	T& ToRef()
	{
		return *To<T>();
	}
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FSaveGames
{
	GENERATED_BODY()

public:
	FORCEINLINE FSaveGames()
	{
		Array = TArray<USaveGameBase*>();
	}

	UPROPERTY()
	TArray<USaveGameBase*> Array;
};
