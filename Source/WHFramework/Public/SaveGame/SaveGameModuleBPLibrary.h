// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "SaveGameModule.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Main/MainModule.h"

#include "SaveGameModuleBPLibrary.generated.h"

class USaveGameBase;
class AParameterModule;
class ATargetPoint;

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API USaveGameModuleBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "SaveGameModuleBPLibrary")
	static int32 GetUserIndex();

	UFUNCTION(BlueprintCallable, Category = "SaveGameModuleBPLibrary")
	static void SetUserIndex(int32 InUserIndex);

	UFUNCTION(BlueprintPure, Category = "SaveGameModuleBPLibrary")
	static FString GetSaveSlotName(FName InSaveName, int32 InSaveIndex = 0);

	template<class T>
	static bool HasSaveGame(int32 InSaveIndex = -1, TSubclassOf<USaveGameBase> InSaveGameClass = T::StaticClass())
	{
		if(ASaveGameModule* SaveGameModule = AMainModule::GetModuleByClass<ASaveGameModule>())
		{
			return SaveGameModule->HasSaveGame<T>(InSaveIndex, InSaveGameClass);
		}
		return false;
	}

	UFUNCTION(BlueprintPure, Category = "SaveGameModuleBPLibrary")
	static bool HasSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass, int32 InSaveIndex = -1);

	template<class T>
	static T* GetSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass = T::StaticClass())
	{
		if(ASaveGameModule* SaveGameModule = AMainModule::GetModuleByClass<ASaveGameModule>())
		{
			return SaveGameModule->GetSaveGame<T>(InSaveGameClass);
		}
		return nullptr;
	}

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InSaveGameClass"), Category = "SaveGameModuleBPLibrary")
	static USaveGameBase* GetSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass);

	template<class T>
	static T* CreateSaveGame(int32 InSaveIndex = 0, bool bAutoLoad = false, TSubclassOf<USaveGameBase> InSaveGameClass = T::StaticClass())
	{
		if(ASaveGameModule* SaveGameModule = AMainModule::GetModuleByClass<ASaveGameModule>())
		{
			return SaveGameModule->CreateSaveGame<T>(InSaveIndex, bAutoLoad, InSaveGameClass);
		}
		return nullptr;
	}

	UFUNCTION(BlueprintCallable, meta = (DeterminesOutputType = "InSaveGameClass"), Category = "SaveGameModuleBPLibrary")
	static USaveGameBase* CreateSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass, int32 InSaveIndex = 0, bool bAutoLoad = false);

	template<class T>
	static bool SaveSaveGame(bool bRefresh = false, TSubclassOf<USaveGameBase> InSaveGameClass = T::StaticClass())
	{
		if(ASaveGameModule* SaveGameModule = AMainModule::GetModuleByClass<ASaveGameModule>())
		{
			return SaveGameModule->SaveSaveGame<T>(bRefresh, InSaveGameClass);
		}
		return false;
	}

	UFUNCTION(BlueprintCallable, Category = "SaveGameModuleBPLibrary")
	static bool SaveSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass, bool bRefresh = false);

	UFUNCTION(BlueprintCallable, Category = "SaveGameModuleBPLibrary")
	static bool SaveSaveGames(TArray<TSubclassOf<USaveGameBase>> InSaveGameClass, bool bRefresh = false);

	UFUNCTION(BlueprintCallable, Category = "SaveGameModuleBPLibrary")
	static bool SaveAllSaveGame(bool bRefresh = false);

	template<class T>
	static T* LoadSaveGame(int32 InSaveIndex = 0, TSubclassOf<USaveGameBase> InSaveGameClass = T::StaticClass())
	{
		if(ASaveGameModule* SaveGameModule = AMainModule::GetModuleByClass<ASaveGameModule>())
		{
			return SaveGameModule->LoadSaveGame<T>(InSaveIndex, InSaveGameClass);
		}
		return nullptr;
	}

	UFUNCTION(BlueprintCallable, meta = (DeterminesOutputType = "InSaveGameClass"), Category = "SaveGameModuleBPLibrary")
	static USaveGameBase* LoadSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass, int32 InSaveIndex = 0);

	template<class T>
	static bool UnloadSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass = T::StaticClass())
	{
		if(ASaveGameModule* SaveGameModule = AMainModule::GetModuleByClass<ASaveGameModule>())
		{
			return SaveGameModule->UnloadSaveGame<T>(InSaveGameClass);
		}
		return false;
	}

	UFUNCTION(BlueprintCallable, Category = "SaveGameModuleBPLibrary")
	static bool UnloadSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass);

	template<class T>
	static bool ResetSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass = T::StaticClass())
	{
		if(ASaveGameModule* SaveGameModule = AMainModule::GetModuleByClass<ASaveGameModule>())
		{
			return SaveGameModule->ResetSaveGame<T>(InSaveGameClass);
		}
		return false;
	}

	UFUNCTION(BlueprintCallable, Category = "SaveGameModuleBPLibrary")
	static bool ResetSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass);

	template<class T>
	static bool RefreshSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass = T::StaticClass())
	{
		if(ASaveGameModule* SaveGameModule = AMainModule::GetModuleByClass<ASaveGameModule>())
		{
			return SaveGameModule->RefreshSaveGame<T>(InSaveGameClass);
		}
		return false;
	}

	UFUNCTION(BlueprintCallable, Category = "SaveGameModuleBPLibrary")
	static bool RefreshSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass);

	template<class T>
	static bool DestroySaveGame(int32 InSaveIndex = 0, TSubclassOf<USaveGameBase> InSaveGameClass = T::StaticClass())
	{
		if(ASaveGameModule* SaveGameModule = AMainModule::GetModuleByClass<ASaveGameModule>())
		{
			return SaveGameModule->DestroySaveGame<T>(InSaveIndex, InSaveGameClass);
		}
		return false;
	}

	UFUNCTION(BlueprintCallable, Category = "SaveGameModuleBPLibrary")
	static bool DestroySaveGame(TSubclassOf<USaveGameBase> InSaveGameClass, int32 InSaveIndex = 0);

	//////////////////////////////////////////////////////////////////////////
	/// SaveData
	static void ObjectLoadData(UObject* InObject, FSaveData* InSaveData, bool bLoadMemoryData = false);

	template<class T>
	static T* ObjectToData(UObject* InObject, bool bMakeSaved = true, bool bSaveMemoryData = false)
	{
		return static_cast<T*>(ObjectToData(InObject, bMakeSaved, bSaveMemoryData));
	}

	static FSaveData* ObjectToData(UObject* InObject, bool bMakeSaved = true, bool bSaveMemoryData = false);

	static void ObjectUnloadData(UObject* InObject);
};
