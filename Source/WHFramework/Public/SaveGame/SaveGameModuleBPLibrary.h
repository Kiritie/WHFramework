// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "WHFramework.h"
#include "SaveGameModuleTypes.h"

#include "Kismet/BlueprintFunctionLibrary.h"
#include "Main/MainModule.h"

#include "SaveGameModuleBPLibrary.generated.h"

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
	static FString GetSaveSlotName(FName InSaveName, int32 InSaveIndex);

public:
	template<class T>
	static bool HasSaveGame(int32 InSaveIndex = -1, bool bFindOnDisk = false, bool bNeedLoaded = false, TSubclassOf<USaveGameBase> InSaveGameClass = T::StaticClass())
	{
		return HasSaveGame(InSaveGameClass, InSaveIndex);
	}

	UFUNCTION(BlueprintPure, Category = "SaveGameModuleBPLibrary")
	static bool HasSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass, int32 InSaveIndex = -1, bool bFindOnDisk = false, bool bNeedLoaded = false);
		
	template<class T>
	static int32 GetValidSaveIndex(TSubclassOf<USaveGameBase> InSaveGameClass = T::StaticClass())
	{
		return GetValidSaveIndex(InSaveGameClass);
	}

	UFUNCTION(BlueprintPure, Category = "SaveGameModuleBPLibrary")
	static int32 GetValidSaveIndex(TSubclassOf<USaveGameBase> InSaveGameClass);

	template<class T>
	static int32 GetActiveSaveIndex(TSubclassOf<USaveGameBase> InSaveGameClass = T::StaticClass())
	{
		return GetActiveSaveIndex(InSaveGameClass);
	}

	UFUNCTION(BlueprintPure, Category = "SaveGameModuleBPLibrary")
	static int32 GetActiveSaveIndex(TSubclassOf<USaveGameBase> InSaveGameClass);

	template<class T>
	static void SetActiveSaveIndex(int32 InSaveIndex, TSubclassOf<USaveGameBase> InSaveGameClass = T::StaticClass())
	{
		SetActiveSaveIndex(InSaveGameClass, InSaveIndex);
	}

	UFUNCTION(BlueprintCallable, Category = "SaveGameModuleBPLibrary")
	static void SetActiveSaveIndex(TSubclassOf<USaveGameBase> InSaveGameClass, int32 InSaveIndex);

	template<class T>
	static FSaveGameInfo GetSaveGameInfo(bool bFromGeneralData = false, TSubclassOf<USaveGameBase> InSaveGameClass = T::StaticClass())
	{
		return GetSaveGameInfo(InSaveGameClass);
	}

	UFUNCTION(BlueprintPure, Category = "SaveGameModuleBPLibrary")
	static FSaveGameInfo GetSaveGameInfo(TSubclassOf<USaveGameBase> InSaveGameClass, bool bFromGeneralData = false);

	template<class T>
	static T* GetSaveGame(int32 InSaveIndex = -1, TSubclassOf<USaveGameBase> InSaveGameClass = T::StaticClass())
	{
		return Cast<T>(GetSaveGame(InSaveGameClass, InSaveIndex));
	}

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InSaveGameClass"), Category = "SaveGameModuleBPLibrary")
	static USaveGameBase* GetSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass, int32 InSaveIndex = -1);

	template<class T>
	static TArray<T*> GetSaveGames(TSubclassOf<USaveGameBase> InSaveGameClass = T::StaticClass())
	{
		TArray<T*> SaveGames;
		for(auto Iter : GetSaveGames(InSaveGameClass))
		{
			SaveGames.Add(Cast<T>(Iter));
		}
		return SaveGames;
	}

	UFUNCTION(BlueprintPure, Category = "SaveGameModuleBPLibrary")
	static TArray<USaveGameBase*> GetSaveGames(TSubclassOf<USaveGameBase> InSaveGameClass);

	template<class T>
	static T* CreateSaveGame(int32 InSaveIndex = -1, bool bAutoLoad = false, TSubclassOf<USaveGameBase> InSaveGameClass = T::StaticClass())
	{
		return Cast<T>(CreateSaveGame(InSaveGameClass, InSaveIndex, bAutoLoad));
	}

	UFUNCTION(BlueprintCallable, meta = (DeterminesOutputType = "InSaveGameClass"), Category = "SaveGameModuleBPLibrary")
	static USaveGameBase* CreateSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass, int32 InSaveIndex = -1, bool bAutoLoad = false);

	template<class T>
	static T* GetOrCreateSaveGame(int32 InSaveIndex = -1, bool bAutoLoad = false, TSubclassOf<USaveGameBase> InSaveGameClass = T::StaticClass())
	{
		return Cast<T>(GetOrCreateSaveGame(InSaveGameClass, InSaveIndex, bAutoLoad));
	}

	UFUNCTION(BlueprintCallable, meta = (DeterminesOutputType = "InSaveGameClass"), Category = "SaveGameModuleBPLibrary")
	static USaveGameBase* GetOrCreateSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass, int32 InSaveIndex = -1, bool bAutoLoad = false);

	template<class T>
	static T* LoadOrCreateSaveGame(int32 InSaveIndex = -1, TSubclassOf<USaveGameBase> InSaveGameClass = T::StaticClass())
	{
		return Cast<T>(LoadOrCreateSaveGame(InSaveGameClass, InSaveIndex));
	}

	UFUNCTION(BlueprintCallable, meta = (DeterminesOutputType = "InSaveGameClass"), Category = "SaveGameModuleBPLibrary")
	static USaveGameBase* LoadOrCreateSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass, int32 InSaveIndex = -1);

	template<class T>
	static bool SaveSaveGame(int32 InSaveIndex = -1, bool bRefresh = false, TSubclassOf<USaveGameBase> InSaveGameClass = T::StaticClass())
	{
		return SaveSaveGame(InSaveGameClass, InSaveIndex, bRefresh);
	}

	UFUNCTION(BlueprintCallable, Category = "SaveGameModuleBPLibrary")
	static bool SaveSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass, int32 InSaveIndex = -1, bool bRefresh = false);

	template<class T>
	static bool SaveSaveGames(bool bRefresh = false, TSubclassOf<USaveGameBase> InSaveGameClass = T::StaticClass())
	{
		return SaveSaveGames(InSaveGameClass, bRefresh);
	}

	UFUNCTION(BlueprintCallable, Category = "SaveGameModuleBPLibrary")
	static bool SaveSaveGames(TSubclassOf<USaveGameBase> InSaveGameClass, bool bRefresh = false);

	UFUNCTION(BlueprintCallable, Category = "SaveGameModuleBPLibrary")
	static bool SaveAllSaveGame(bool bRefresh = false);

	template<class T>
	static T* LoadSaveGame(int32 InSaveIndex = -1, EPhase InPhase = EPhase::Lesser, TSubclassOf<USaveGameBase> InSaveGameClass = T::StaticClass())
	{
		return Cast<T>(LoadSaveGame(InSaveGameClass, InSaveIndex, InPhase));
	}

	UFUNCTION(BlueprintCallable, meta = (DeterminesOutputType = "InSaveGameClass"), Category = "SaveGameModuleBPLibrary")
	static USaveGameBase* LoadSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass, int32 InSaveIndex = -1, EPhase InPhase = EPhase::Lesser);

	template<class T>
	static bool UnloadSaveGame(int32 InSaveIndex = -1, EPhase InPhase = EPhase::Lesser, TSubclassOf<USaveGameBase> InSaveGameClass = T::StaticClass())
	{
		return UnloadSaveGame(InSaveGameClass, InSaveIndex, InPhase);
	}

	UFUNCTION(BlueprintCallable, Category = "SaveGameModuleBPLibrary")
	static bool UnloadSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass, int32 InSaveIndex = -1, EPhase InPhase = EPhase::Lesser);

	template<class T>
	static bool ResetSaveGame(int32 InSaveIndex = -1, TSubclassOf<USaveGameBase> InSaveGameClass = T::StaticClass())
	{
		return ResetSaveGame(InSaveGameClass, InSaveIndex);
	}

	UFUNCTION(BlueprintCallable, Category = "SaveGameModuleBPLibrary")
	static bool ResetSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass, int32 InSaveIndex = -1);

	template<class T>
	static bool RefreshSaveGame(int32 InSaveIndex = -1, TSubclassOf<USaveGameBase> InSaveGameClass = T::StaticClass())
	{
		return RefreshSaveGame(InSaveGameClass, InSaveIndex);
	}

	UFUNCTION(BlueprintCallable, Category = "SaveGameModuleBPLibrary")
	static bool RefreshSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass, int32 InSaveIndex = -1);

	template<class T>
	static bool DestroySaveGame(int32 InSaveIndex = -1, TSubclassOf<USaveGameBase> InSaveGameClass = T::StaticClass())
	{
		return DestroySaveGame(InSaveGameClass, InSaveIndex);
	}

	UFUNCTION(BlueprintCallable, Category = "SaveGameModuleBPLibrary")
	static bool DestroySaveGame(TSubclassOf<USaveGameBase> InSaveGameClass, int32 InSaveIndex = -1);
};
