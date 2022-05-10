// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

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
	void OnCreate(int32 InSaveIndex);
	
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

public:
	UFUNCTION(BlueprintCallable)
	bool Save(bool bRefresh = false);

	UFUNCTION(BlueprintCallable)
	bool Load();

	UFUNCTION(BlueprintCallable)
	bool Unload();

	UFUNCTION(BlueprintCallable)
	bool Reset();

	UFUNCTION(BlueprintCallable)
	bool Refresh();

	UFUNCTION(BlueprintCallable)
	bool Destroy();

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
	
	UFUNCTION(BlueprintCallable)
	void SetSaveIndex(int32 InSaveIndex) { this->SaveIndex = InSaveIndex; }

protected:
	UPROPERTY()
	bool bSaved;
public:
	UFUNCTION(BlueprintPure)
	bool IsSaved() const { return bSaved; }

protected:
	UPROPERTY()
	bool bLoaded;
public:
	UFUNCTION(BlueprintPure)
	bool IsLoaded() const { return bLoaded; }
};
