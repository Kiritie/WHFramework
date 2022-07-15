// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "GameFramework/SaveGame.h"
#include "SaveGame/SaveGameModuleTypes.h"
#include "SaveGameBase.generated.h"

/**
* 数据存取基类
*/
UCLASS()
class WHFRAMEWORK_API USaveGameBase : public USaveGame
{
	GENERATED_BODY()

	friend class ASaveGameModule;

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
	void OnUnload(bool bForceMode);

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
	bool Unload(bool bForceMode = false);

	UFUNCTION(BlueprintCallable)
	bool Reset();

	UFUNCTION(BlueprintCallable)
	bool Refresh();

	UFUNCTION(BlueprintCallable)
	bool Destroy();

protected:
	UPROPERTY(EditDefaultsOnly)
	FName SaveName;

	UPROPERTY()
	int32 SaveIndex;

	UPROPERTY()
	bool bSaved;

	UPROPERTY()
	bool bLoaded;
	
public:
	UFUNCTION(BlueprintPure)
	FName GetSaveName() const { return SaveName; }

	UFUNCTION(BlueprintPure)
	int32 GetSaveIndex() const { return SaveIndex; }
	
	UFUNCTION(BlueprintCallable)
	void SetSaveIndex(int32 InSaveIndex) { this->SaveIndex = InSaveIndex; }

	UFUNCTION(BlueprintPure)
	bool IsSaved() const { return bSaved; }

	UFUNCTION(BlueprintPure)
	bool IsLoaded() const { return bLoaded; }

public:
	template<typename T>
	T* GetSaveData()
	{
		return static_cast<T*>(GetSaveData());
	}
	
	template<typename T>
	T& GetSaveDataRef()
	{
		return *GetSaveData<T>();
	}

	virtual FSaveData* GetSaveData() { return nullptr; }

	virtual void SetSaveData(FSaveData* InSaveData) { }
};
