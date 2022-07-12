// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Parameter/ParameterModuleTypes.h"

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
struct WHFRAMEWORK_API FSaveGameInfo
{
	GENERATED_BODY()

public:
	FORCEINLINE FSaveGameInfo()
	{
		Index = -1;
		Class = nullptr;
		Array = TMap<int32, USaveGameBase*>();
	}

	UPROPERTY(BlueprintReadOnly)
	int32 Index;

	UPROPERTY(BlueprintReadOnly)
	TSubclassOf<USaveGameBase> Class;

	UPROPERTY(BlueprintReadOnly)
	TMap<int32, USaveGameBase*> Array;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FParameterSaveData : public FSaveData
{
	GENERATED_BODY()

public:
	FORCEINLINE FParameterSaveData()
	{
		Parameters = FParameters();
	}

public:
	UPROPERTY(BlueprintReadOnly)
	FParameters Parameters;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FGeneralSaveData : public FSaveData
{
	GENERATED_BODY()

public:
	FORCEINLINE FGeneralSaveData()
	{
		AllSaveGameInfo = TMap<FName, FSaveGameInfo>();
	}

public:
	UPROPERTY(BlueprintReadOnly)
	TMap<FName, FSaveGameInfo> AllSaveGameInfo;
};