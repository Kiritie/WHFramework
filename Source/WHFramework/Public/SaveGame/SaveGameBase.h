// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "SaveGameModuleTypes.h"
#include "GameFramework/SaveGame.h"
#include "SaveGameBase.generated.h"

class USaveGameDataBase;
/**
* 数据存取基类
*/
UCLASS()
class WHFRAMEWORK_API USaveGameBase : public USaveGame
{
	GENERATED_BODY()

public:
	USaveGameBase();

public:
	UFUNCTION(BlueprintNativeEvent)
	void OnCreate(USaveGameDataBase* InSaveGameData);
	
	UFUNCTION(BlueprintNativeEvent)
	void OnSave();

	UFUNCTION(BlueprintNativeEvent)
	void OnLoad();
	
	UFUNCTION(BlueprintNativeEvent)
	void OnUnload();

	UFUNCTION(BlueprintNativeEvent)
	void OnReset();
	
	UFUNCTION(BlueprintNativeEvent)
	void OnRefresh();

	UFUNCTION(BlueprintNativeEvent)
	void OnDestroy();

protected:
	UPROPERTY(BlueprintReadOnly)
	USaveGameDataBase* SaveGameData;
public:
	template<class T>
	T* GetSaveGameData() const
	{
		return Cast<T>(SaveGameData);
	}

	UFUNCTION(BlueprintPure, meta = (DisplayName = "GetSaveGameData", DeterminesOutputType = "InSaveGameDataClass"))
	USaveGameDataBase* K2_GetSaveGameData(TSubclassOf<USaveGameDataBase> InSaveGameDataClass) const;

protected:
	UPROPERTY(BlueprintReadOnly)
	FName SaveName;
public:
	UFUNCTION(BlueprintPure)
	FName GetSaveName() const { return SaveName; }

protected:
	UPROPERTY(BlueprintReadOnly)
	bool bSaved;
public:
	UFUNCTION(BlueprintPure)
	bool IsSaved() const { return bSaved; }

protected:
	UPROPERTY(BlueprintReadOnly)
	bool bLoaded;
public:
	UFUNCTION(BlueprintPure)
	bool IsLoaded() const { return bLoaded; }
};
