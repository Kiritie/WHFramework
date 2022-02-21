// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "SaveGameModuleTypes.h"
#include "GameFramework/SaveGame.h"
#include "SaveGameBase.generated.h"

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
	void OnCreate();
	
	UFUNCTION(BlueprintNativeEvent)
	void OnSave(int32 InIndex);

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

public:
	UFUNCTION(BlueprintCallable)
	void Save(int32 InIndex, bool bRefresh = false);

	UFUNCTION(BlueprintCallable)
	void Load();

	UFUNCTION(BlueprintCallable)
	void Unload();

	UFUNCTION(BlueprintCallable)
	void Reset();

	UFUNCTION(BlueprintCallable)
	void Refresh();

	UFUNCTION(BlueprintCallable)
	void Destroy();

protected:
	UPROPERTY(EditDefaultsOnly)
	FName SaveName;
public:
	UFUNCTION(BlueprintPure)
	FName GetSaveName() const { return SaveName; }

protected:
	UPROPERTY()
	int32 SaveIndex;
public:
	UFUNCTION(BlueprintPure)
	int32 GetSaveIndex() const { return SaveIndex; }

protected:
	UPROPERTY()
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
