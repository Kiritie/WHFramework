// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "SaveGameModuleTypes.h"
#include "Base/SaveDataInterface.h"
#include "Main/Base/ModuleBase.h"

#include "SaveGameModule.generated.h"

class UGeneralSaveGame;
class ATargetPoint;

UCLASS()
class WHFRAMEWORK_API ASaveGameModule : public AModuleBase, public ISaveDataInterface
{
	GENERATED_BODY()

	friend class USaveGameBase;
	
public:	
	// ParamSets default values for this actor's properties
	ASaveGameModule();
	
	//////////////////////////////////////////////////////////////////////////
	/// Module
public:
#if WITH_EDITOR
	virtual void OnGenerate_Implementation() override;

	virtual void OnDestroy_Implementation() override;
#endif

	virtual void OnInitialize_Implementation() override;

	virtual void OnPreparatory_Implementation() override;

	virtual void OnRefresh_Implementation(float DeltaSeconds) override;

	virtual void OnPause_Implementation() override;

	virtual void OnUnPause_Implementation() override;

	virtual void OnTermination_Implementation() override;

protected:
	virtual void LoadData(FSaveData* InSaveData) override;

	virtual FSaveData* ToData() override;

protected:
	UPROPERTY(EditAnywhere, Category = "UserData")
	int32 UserIndex;

public:
	UFUNCTION(BlueprintPure, Category = "UserData")
	int32 GetUserIndex() const { return UserIndex; }

	UFUNCTION(BlueprintCallable, Category = "UserData")
	void SetUserIndex(int32 InUserIndex) { UserIndex = InUserIndex; }

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SaveData")
	TSubclassOf<UGeneralSaveGame> GeneralSaveGame;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SaveData")
	TMap<FName, FSaveGameInfo> AllSaveGameInfo;

public:
	UFUNCTION(BlueprintPure)
	FString GetSaveSlotName(FName InSaveName, int32 InSaveIndex) const;

	template<class T>
	bool HasSaveGame(int32 InSaveIndex = -1, bool bFindOnDisk = false, bool bNeedLoaded = false, TSubclassOf<USaveGameBase> InSaveGameClass = T::StaticClass()) const
	{
		return HasSaveGame(InSaveGameClass, InSaveIndex);
	}

	UFUNCTION(BlueprintPure)
	bool HasSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass, int32 InSaveIndex = -1, bool bFindOnDisk = false, bool bNeedLoaded = false) const;
	
	template<class T>
	int32 GetValidSaveIndex(TSubclassOf<USaveGameBase> InSaveGameClass = T::StaticClass()) const
	{
		return GetValidSaveIndex(InSaveGameClass);
	}

	UFUNCTION(BlueprintPure)
	int32 GetValidSaveIndex(TSubclassOf<USaveGameBase> InSaveGameClass) const;

	template<class T>
	int32 GetActiveSaveIndex(TSubclassOf<USaveGameBase> InSaveGameClass = T::StaticClass()) const
	{
		return GetActiveSaveIndex(InSaveGameClass);
	}

	UFUNCTION(BlueprintPure)
	int32 GetActiveSaveIndex(TSubclassOf<USaveGameBase> InSaveGameClass) const;

	template<class T>
	void SetActiveSaveIndex(int32 InSaveIndex, TSubclassOf<USaveGameBase> InSaveGameClass = T::StaticClass())
	{
		SetActiveSaveIndex(InSaveGameClass, InSaveIndex);
	}

	UFUNCTION(BlueprintCallable)
	void SetActiveSaveIndex(TSubclassOf<USaveGameBase> InSaveGameClass, int32 InSaveIndex);

	template<class T>
	T* GetSaveGame(int32 InSaveIndex = -1, TSubclassOf<USaveGameBase> InSaveGameClass = T::StaticClass()) const
	{
		return Cast<T>(GetSaveGame(InSaveGameClass, InSaveIndex));
	}

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InSaveGameClass"))
	USaveGameBase* GetSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass, int32 InSaveIndex = -1) const;

	template<class T>
	TArray<T*> GetSaveGames(TSubclassOf<USaveGameBase> InSaveGameClass = T::StaticClass()) const
	{
		TArray<T*> SaveGames;
		for(auto Iter : GetSaveGames(InSaveGameClass))
		{
			SaveGames.Add(Cast<T>(Iter));
		}
		return SaveGames;
	}

	UFUNCTION(BlueprintPure)
	TArray<USaveGameBase*> GetSaveGames(TSubclassOf<USaveGameBase> InSaveGameClass) const;

	template<class T>
	T* CreateSaveGame(int32 InSaveIndex = -1, bool bAutoLoad = false, TSubclassOf<USaveGameBase> InSaveGameClass = T::StaticClass())
	{
		return Cast<T>(CreateSaveGame(InSaveGameClass, InSaveIndex, bAutoLoad));
	}

	UFUNCTION(BlueprintCallable, meta = (DeterminesOutputType = "InSaveGameClass"))
	USaveGameBase* CreateSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass, int32 InSaveIndex = -1, bool bAutoLoad = false);

	template<class T>
	T* GetOrCreateSaveGame(int32 InSaveIndex = -1, bool bAutoLoad = false, TSubclassOf<USaveGameBase> InSaveGameClass = T::StaticClass())
	{
		return Cast<T>(GetOrCreateSaveGame(InSaveGameClass, InSaveIndex, bAutoLoad));
	}

	UFUNCTION(BlueprintCallable, meta = (DeterminesOutputType = "InSaveGameClass"))
	USaveGameBase* GetOrCreateSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass, int32 InSaveIndex = -1, bool bAutoLoad = false);

	template<class T>
	T* LoadOrCreateSaveGame(int32 InSaveIndex = -1, TSubclassOf<USaveGameBase> InSaveGameClass = T::StaticClass())
	{
		return Cast<T>(LoadOrCreateSaveGame(InSaveGameClass, InSaveIndex));
	}

	UFUNCTION(BlueprintCallable, meta = (DeterminesOutputType = "InSaveGameClass"))
	USaveGameBase* LoadOrCreateSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass, int32 InSaveIndex = -1);

	template<class T>
	bool SaveSaveGame(int32 InSaveIndex = -1, bool bRefresh = false, TSubclassOf<USaveGameBase> InSaveGameClass = T::StaticClass())
	{
		return SaveSaveGame(InSaveGameClass, InSaveIndex, bRefresh);
	}

	UFUNCTION(BlueprintCallable)
	bool SaveSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass, int32 InSaveIndex = -1, bool bRefresh = false);

	template<class T>
	bool SaveSaveGames(bool bRefresh = false, TSubclassOf<USaveGameBase> InSaveGameClass = T::StaticClass())
	{
		return SaveSaveGames(InSaveGameClass, bRefresh);
	}

	UFUNCTION(BlueprintCallable)
	bool SaveSaveGames(TSubclassOf<USaveGameBase> InSaveGameClass, bool bRefresh = false);

	UFUNCTION(BlueprintCallable)
	bool SaveAllSaveGame(bool bRefresh = false);

	template<class T>
	T* LoadSaveGame(int32 InSaveIndex = -1, TSubclassOf<USaveGameBase> InSaveGameClass = T::StaticClass())
	{
		return Cast<T>(LoadSaveGame(InSaveGameClass, InSaveIndex));
	}

	UFUNCTION(BlueprintCallable, meta = (DeterminesOutputType = "InSaveGameClass"))
	USaveGameBase* LoadSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass, int32 InSaveIndex = -1);

	template<class T>
	bool UnloadSaveGame(int32 InSaveIndex = -1, bool bForceMode = false, TSubclassOf<USaveGameBase> InSaveGameClass = T::StaticClass())
	{
		return UnloadSaveGame(InSaveGameClass, InSaveIndex, bForceMode);
	}

	UFUNCTION(BlueprintCallable)
	bool UnloadSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass, int32 InSaveIndex = -1, bool bForceMode = false);

	template<class T>
	bool ResetSaveGame(int32 InSaveIndex = -1, TSubclassOf<USaveGameBase> InSaveGameClass = T::StaticClass())
	{
		return ResetSaveGame(InSaveGameClass, InSaveIndex);
	}

	UFUNCTION(BlueprintCallable)
	bool ResetSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass, int32 InSaveIndex = -1);

	template<class T>
	bool RefreshSaveGame(int32 InSaveIndex = -1, TSubclassOf<USaveGameBase> InSaveGameClass = T::StaticClass())
	{
		return RefreshSaveGame(InSaveGameClass, InSaveIndex);
	}

	UFUNCTION(BlueprintCallable)
	bool RefreshSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass, int32 InSaveIndex = -1);

	template<class T>
	bool DestroySaveGame(int32 InSaveIndex = -1, TSubclassOf<USaveGameBase> InSaveGameClass = T::StaticClass())
	{
		return DestroySaveGame(InSaveGameClass, InSaveIndex);
	}

	UFUNCTION(BlueprintCallable)
	bool DestroySaveGame(TSubclassOf<USaveGameBase> InSaveGameClass, int32 InSaveIndex = -1);

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
