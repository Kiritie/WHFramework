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

	UFUNCTION(BlueprintPure, meta = (DisplayName = "HasSaveGame"), Category = "SaveGameModuleBPLibrary")
	static bool K2_HasSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass, int32 InSaveIndex = -1);

	template<class T>
	static T* GetSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass = T::StaticClass())
	{
		if(ASaveGameModule* SaveGameModule = AMainModule::GetModuleByClass<ASaveGameModule>())
		{
			return SaveGameModule->GetSaveGame<T>(InSaveGameClass);
		}
		return nullptr;
	}

	UFUNCTION(BlueprintPure, meta = (DisplayName = "GetSaveGame", DeterminesOutputType = "InSaveGameClass"), Category = "SaveGameModuleBPLibrary")
	static USaveGameBase* K2_GetSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass);

	template<class T>
	static T* CreateSaveGame(int32 InSaveIndex = 0, bool bAutoLoad = false, TSubclassOf<USaveGameBase> InSaveGameClass = T::StaticClass())
	{
		if(ASaveGameModule* SaveGameModule = AMainModule::GetModuleByClass<ASaveGameModule>())
		{
			return SaveGameModule->CreateSaveGame<T>(InSaveIndex, bAutoLoad, InSaveGameClass);
		}
		return nullptr;
	}

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "CreateSaveGame", DeterminesOutputType = "InSaveGameClass"), Category = "SaveGameModuleBPLibrary")
	static USaveGameBase* K2_CreateSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass, int32 InSaveIndex = 0, bool bAutoLoad = false);

	template<class T>
	static bool SaveSaveGame(bool bRefresh = false, TSubclassOf<USaveGameBase> InSaveGameClass = T::StaticClass())
	{
		if(ASaveGameModule* SaveGameModule = AMainModule::GetModuleByClass<ASaveGameModule>())
		{
			return SaveGameModule->SaveSaveGame<T>(bRefresh, InSaveGameClass);
		}
		return false;
	}

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "SaveSaveGame"), Category = "SaveGameModuleBPLibrary")
	static bool K2_SaveSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass, bool bRefresh = false);

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "SaveSaveGame"), Category = "SaveGameModuleBPLibrary")
	static bool SaveSaveGames(TArray<TSubclassOf<USaveGameBase>> InSaveGameClass, bool bRefresh = false);

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "SaveSaveGame"), Category = "SaveGameModuleBPLibrary")
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

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "LoadSaveGame", DeterminesOutputType = "InSaveGameClass"), Category = "SaveGameModuleBPLibrary")
	static USaveGameBase* K2_LoadSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass, int32 InSaveIndex = 0);

	template<class T>
	static bool UnloadSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass = T::StaticClass())
	{
		if(ASaveGameModule* SaveGameModule = AMainModule::GetModuleByClass<ASaveGameModule>())
		{
			return SaveGameModule->UnloadSaveGame<T>(InSaveGameClass);
		}
		return false;
	}

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "UnloadSaveGame"), Category = "SaveGameModuleBPLibrary")
	static bool K2_UnloadSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass);

	template<class T>
	static bool ResetSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass = T::StaticClass())
	{
		if(ASaveGameModule* SaveGameModule = AMainModule::GetModuleByClass<ASaveGameModule>())
		{
			return SaveGameModule->ResetSaveGame<T>(InSaveGameClass);
		}
		return false;
	}

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "ResetSaveGame"), Category = "SaveGameModuleBPLibrary")
	static bool K2_ResetSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass);

	template<class T>
	static bool RefreshSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass = T::StaticClass())
	{
		if(ASaveGameModule* SaveGameModule = AMainModule::GetModuleByClass<ASaveGameModule>())
		{
			return SaveGameModule->RefreshSaveGame<T>(InSaveGameClass);
		}
		return false;
	}

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "RefreshSaveGame"), Category = "SaveGameModuleBPLibrary")
	static bool K2_RefreshSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass);

	template<class T>
	static bool DestroySaveGame(int32 InSaveIndex = 0, TSubclassOf<USaveGameBase> InSaveGameClass = T::StaticClass())
	{
		if(ASaveGameModule* SaveGameModule = AMainModule::GetModuleByClass<ASaveGameModule>())
		{
			return SaveGameModule->DestroySaveGame<T>(InSaveIndex, InSaveGameClass);
		}
		return false;
	}

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "DestroySaveGame"), Category = "SaveGameModuleBPLibrary")
	static bool K2_DestroySaveGame(TSubclassOf<USaveGameBase> InSaveGameClass, int32 InSaveIndex = 0);
};
