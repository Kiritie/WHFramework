// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "SaveGameModuleTypes.h"
#include "Common/CommonTypes.h"

#include "Kismet/BlueprintFunctionLibrary.h"

#include "SaveGameModuleStatics.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API USaveGameModuleStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "SaveGameModuleStatics")
	static int32 GetUserIndex();

	UFUNCTION(BlueprintCallable, Category = "SaveGameModuleStatics")
	static void SetUserIndex(int32 InUserIndex);

	UFUNCTION(BlueprintPure, Category = "SaveGameModuleStatics")
	static FString GetSaveSlotName(FName InSaveName, int32 InIndex);

public:
	template<class T>
	static bool HasSaveGame(int32 InIndex = -1, TSubclassOf<USaveGameBase> InClass = T::StaticClass())
	{
		return HasSaveGame(InClass, InIndex);
	}

	UFUNCTION(BlueprintPure, Category = "SaveGameModuleStatics")
	static bool HasSaveGame(TSubclassOf<USaveGameBase> InClass, int32 InIndex = -1);

	template<class T>
	static FSaveGameInfo GetSaveGameInfo(TSubclassOf<USaveGameBase> InClass = T::StaticClass())
	{
		return GetSaveGameInfo(InClass);
	}

	UFUNCTION(BlueprintPure, Category = "SaveGameModuleStatics")
	static FSaveGameInfo GetSaveGameInfo(TSubclassOf<USaveGameBase> InClass);

	template<class T>
	static T* GetSaveGame(int32 InIndex = -1, TSubclassOf<USaveGameBase> InClass = T::StaticClass())
	{
		return Cast<T>(GetSaveGame(InClass, InIndex));
	}

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"), Category = "SaveGameModuleStatics")
	static USaveGameBase* GetSaveGame(TSubclassOf<USaveGameBase> InClass, int32 InIndex = -1);

	template<class T>
	static TArray<T*> GetSaveGames(TSubclassOf<USaveGameBase> InClass = T::StaticClass())
	{
		TArray<T*> SaveGames;
		for(auto Iter : GetSaveGames(InClass))
		{
			SaveGames.Add(Cast<T>(Iter));
		}
		return SaveGames;
	}

	UFUNCTION(BlueprintPure, Category = "SaveGameModuleStatics")
	static TArray<USaveGameBase*> GetSaveGames(TSubclassOf<USaveGameBase> InClass);

	template<class T>
	static T* CreateSaveGame(int32 InIndex = -1, EPhase InPhase = EPhase::None, TSubclassOf<USaveGameBase> InClass = T::StaticClass())
	{
		return Cast<T>(CreateSaveGame(InClass, InIndex, InPhase));
	}

	UFUNCTION(BlueprintCallable, meta = (DeterminesOutputType = "InClass"), Category = "SaveGameModuleStatics")
	static USaveGameBase* CreateSaveGame(TSubclassOf<USaveGameBase> InClass, int32 InIndex = -1, EPhase InPhase = EPhase::None);

	template<class T>
	static T* GetOrCreateSaveGame(int32 InIndex = -1, EPhase InPhase = EPhase::None, TSubclassOf<USaveGameBase> InClass = T::StaticClass())
	{
		return Cast<T>(GetOrCreateSaveGame(InClass, InIndex, InPhase));
	}

	UFUNCTION(BlueprintCallable, meta = (DeterminesOutputType = "InClass"), Category = "SaveGameModuleStatics")
	static USaveGameBase* GetOrCreateSaveGame(TSubclassOf<USaveGameBase> InClass, int32 InIndex = -1, EPhase InPhase = EPhase::None);

	template<class T>
	static T* LoadOrCreateSaveGame(int32 InIndex = -1, EPhase InPhase = EPhase::All, TSubclassOf<USaveGameBase> InClass = T::StaticClass())
	{
		return Cast<T>(LoadOrCreateSaveGame(InClass, InIndex, InPhase));
	}

	UFUNCTION(BlueprintCallable, meta = (DeterminesOutputType = "InClass"), Category = "SaveGameModuleStatics")
	static USaveGameBase* LoadOrCreateSaveGame(TSubclassOf<USaveGameBase> InClass, int32 InIndex = -1, EPhase InPhase = EPhase::All);

	template<class T>
	static bool SaveSaveGame(int32 InIndex = -1, bool bRefresh = false, TSubclassOf<USaveGameBase> InClass = T::StaticClass())
	{
		return SaveSaveGame(InClass, InIndex, bRefresh);
	}

	UFUNCTION(BlueprintCallable, Category = "SaveGameModuleStatics")
	static bool SaveSaveGame(TSubclassOf<USaveGameBase> InClass, int32 InIndex = -1, bool bRefresh = false);

	template<class T>
	static bool SaveSaveGames(bool bRefresh = false, TSubclassOf<USaveGameBase> InClass = T::StaticClass())
	{
		return SaveSaveGames(InClass, bRefresh);
	}

	UFUNCTION(BlueprintCallable, Category = "SaveGameModuleStatics")
	static bool SaveSaveGames(TSubclassOf<USaveGameBase> InClass, bool bRefresh = false);

	UFUNCTION(BlueprintCallable, Category = "SaveGameModuleStatics")
	static bool SaveAllSaveGame(bool bRefresh = false);

	template<class T>
	static T* LoadSaveGame(int32 InIndex = -1, EPhase InPhase = EPhase::All, TSubclassOf<USaveGameBase> InClass = T::StaticClass())
	{
		return Cast<T>(LoadSaveGame(InClass, InIndex, InPhase));
	}

	UFUNCTION(BlueprintCallable, meta = (DeterminesOutputType = "InClass"), Category = "SaveGameModuleStatics")
	static USaveGameBase* LoadSaveGame(TSubclassOf<USaveGameBase> InClass, int32 InIndex = -1, EPhase InPhase = EPhase::Lesser);

	template<class T>
	static bool UnloadSaveGame(int32 InIndex = -1, EPhase InPhase = EPhase::All, TSubclassOf<USaveGameBase> InClass = T::StaticClass())
	{
		return UnloadSaveGame(InClass, InIndex, InPhase);
	}

	UFUNCTION(BlueprintCallable, Category = "SaveGameModuleStatics")
	static bool UnloadSaveGame(TSubclassOf<USaveGameBase> InClass, int32 InIndex = -1, EPhase InPhase = EPhase::Lesser);

	template<class T>
	static bool ResetSaveGame(int32 InIndex = -1, TSubclassOf<USaveGameBase> InClass = T::StaticClass())
	{
		return ResetSaveGame(InClass, InIndex);
	}

	UFUNCTION(BlueprintCallable, Category = "SaveGameModuleStatics")
	static bool ResetSaveGame(TSubclassOf<USaveGameBase> InClass, int32 InIndex = -1);

	template<class T>
	static bool RefreshSaveGame(int32 InIndex = -1, TSubclassOf<USaveGameBase> InClass = T::StaticClass())
	{
		return RefreshSaveGame(InClass, InIndex);
	}

	UFUNCTION(BlueprintCallable, Category = "SaveGameModuleStatics")
	static bool RefreshSaveGame(TSubclassOf<USaveGameBase> InClass, int32 InIndex = -1);

	template<class T>
	static bool DestroySaveGame(int32 InIndex = -1, TSubclassOf<USaveGameBase> InClass = T::StaticClass())
	{
		return DestroySaveGame(InClass, InIndex);
	}

	UFUNCTION(BlueprintCallable, Category = "SaveGameModuleStatics")
	static bool DestroySaveGame(TSubclassOf<USaveGameBase> InClass, int32 InIndex = -1);
};
