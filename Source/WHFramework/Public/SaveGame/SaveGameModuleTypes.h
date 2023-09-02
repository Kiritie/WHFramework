// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

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

protected:
	UPROPERTY(Transient)
	bool bSaved;

	UPROPERTY(BlueprintReadWrite)
	TArray<uint8> Datas;

public:
	virtual bool IsSaved() const { return bSaved; }

	virtual void MakeSaved() { bSaved = true; }

	virtual TArray<uint8>& GetDatas() { return Datas; }

	virtual void SetDatas(const TArray<uint8>& InDatas) { Datas = InDatas; }

public:
	template<class T>
	T* Cast()
	{
		return static_cast<T*>(this);
	}

	template<class T>
	T& CastRef()
	{
		return *Cast<T>();
	}
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FSaveGameInfo
{
	GENERATED_BODY()

public:
	FORCEINLINE FSaveGameInfo()
	{
		ActiveIndex = -1;
		SaveGameClass = nullptr;
		SaveGames = TArray<USaveGameBase*>();
	}

	FORCEINLINE FSaveGameInfo(UClass* InClass, int32 InActiveIndex = -1)
	{
		ActiveIndex = InActiveIndex;
		SaveGameClass = InClass;
	}

	UPROPERTY(BlueprintReadOnly)
	int32 ActiveIndex;

	UPROPERTY(BlueprintReadOnly)
	UClass* SaveGameClass;

	UPROPERTY(BlueprintReadOnly, Transient)
	TArray<USaveGameBase*> SaveGames;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FSaveGameData : public FSaveData
{
	GENERATED_BODY()

public:
	FORCEINLINE FSaveGameData()
	{
		ActiveIndex = -1;
		SaveGameClass = nullptr;
		SaveIndexs = TArray<int32>();
	}

	UPROPERTY(BlueprintReadOnly)
	int32 ActiveIndex;

	UPROPERTY(BlueprintReadOnly)
	UClass* SaveGameClass;

	UPROPERTY()
	TArray<int32> SaveIndexs;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FGeneralSaveData : public FSaveData
{
	GENERATED_BODY()

public:
	FORCEINLINE FGeneralSaveData()
	{
		SaveGameDatas = TArray<FSaveGameData>();
	}

public:
	UPROPERTY(BlueprintReadOnly)
	TArray<FSaveGameData> SaveGameDatas;

public:
	virtual void MakeSaved() override
	{
		Super::MakeSaved();

		for(auto& Iter : SaveGameDatas)
		{
			Iter.MakeSaved();
		}
	}
};
