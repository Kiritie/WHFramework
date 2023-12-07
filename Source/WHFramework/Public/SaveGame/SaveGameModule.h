// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "SaveGameModuleTypes.h"
#include "Event/Handle/Common/EventHandle_ExitGame.h"
#include "Main/Base/ModuleBase.h"

#include "SaveGameModule.generated.h"

class UGeneralSaveGame;
class ATargetPoint;

UCLASS()
class WHFRAMEWORK_API USaveGameModule : public UModuleBase
{
	GENERATED_BODY()

	friend class USaveGameBase;
				
	GENERATED_MODULE(USaveGameModule)

public:	
	// ParamSets default values for this actor's properties
	USaveGameModule();

	~USaveGameModule();

	//////////////////////////////////////////////////////////////////////////
	/// Module
public:
#if WITH_EDITOR
	virtual void OnGenerate() override;

	virtual void OnDestroy() override;
#endif

	virtual void OnInitialize() override;

	virtual void OnPreparatory(EPhase InPhase) override;

	virtual void OnRefresh(float DeltaSeconds) override;

	virtual void OnPause() override;

	virtual void OnUnPause() override;

	virtual void OnTermination(EPhase InPhase) override;

	virtual void OnExitGame(UObject* InSender, UEventHandle_ExitGame* InEventHandle);

protected:
	virtual void LoadData(FSaveData* InSaveData, EPhase InPhase) override;

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
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SaveData")
	TMap<FName, FSaveGameInfo> SaveGameInfos;

	UPROPERTY()
	TArray<FString> DestroyedSlotNames;

public:
	UFUNCTION(BlueprintPure)
	FString GetSlotName(FName InSaveName, int32 InIndex) const;

	template<class T>
	bool HasSaveGame(int32 InIndex = -1, TSubclassOf<USaveGameBase> InClass = T::StaticClass()) const
	{
		return HasSaveGame(InClass, InIndex);
	}

	UFUNCTION(BlueprintPure)
	bool HasSaveGame(TSubclassOf<USaveGameBase> InClass, int32 InIndex = -1) const;
		
	template<class T>
	FName GetSaveName(TSubclassOf<USaveGameBase> InClass = T::StaticClass()) const
	{
		return GetSaveName(InClass);
	}

	UFUNCTION(BlueprintPure)
	FName GetSaveName(TSubclassOf<USaveGameBase> InClass) const;

	template<class T>
	int32 GetValidSaveIndex(TSubclassOf<USaveGameBase> InClass = T::StaticClass()) const
	{
		return GetValidSaveIndex(InClass);
	}

	UFUNCTION(BlueprintPure)
	int32 GetValidSaveIndex(TSubclassOf<USaveGameBase> InClass) const;

	template<class T>
	int32 GetActiveSaveIndex(TSubclassOf<USaveGameBase> InClass = T::StaticClass()) const
	{
		return GetActiveSaveIndex(InClass);
	}

	UFUNCTION(BlueprintPure)
	int32 GetActiveSaveIndex(TSubclassOf<USaveGameBase> InClass) const;

	template<class T>
	FSaveGameInfo GetSaveGameInfo(TSubclassOf<USaveGameBase> InClass = T::StaticClass()) const
	{
		return GetSaveGameInfo(InClass);
	}

	UFUNCTION(BlueprintPure)
	FSaveGameInfo GetSaveGameInfo(TSubclassOf<USaveGameBase> InClass) const;

	template<class T>
	T* GetSaveGame(int32 InIndex = -1, TSubclassOf<USaveGameBase> InClass = T::StaticClass()) const
	{
		return Cast<T>(GetSaveGame(InClass, InIndex));
	}

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"))
	USaveGameBase* GetSaveGame(TSubclassOf<USaveGameBase> InClass, int32 InIndex = -1) const;

	template<class T>
	TArray<T*> GetSaveGames(TSubclassOf<USaveGameBase> InClass = T::StaticClass()) const
	{
		TArray<T*> SaveGames;
		for(auto Iter : GetSaveGames(InClass))
		{
			SaveGames.Add(Cast<T>(Iter));
		}
		return SaveGames;
	}

	UFUNCTION(BlueprintPure)
	TArray<USaveGameBase*> GetSaveGames(TSubclassOf<USaveGameBase> InClass) const;

	template<class T>
	T* CreateSaveGame(int32 InIndex = -1, EPhase InPhase = EPhase::None, TSubclassOf<USaveGameBase> InClass = T::StaticClass())
	{
		return Cast<T>(CreateSaveGame(InClass, InIndex, InPhase));
	}

	UFUNCTION(BlueprintCallable, meta = (DeterminesOutputType = "InClass"))
	USaveGameBase* CreateSaveGame(TSubclassOf<USaveGameBase> InClass, int32 InIndex = -1, EPhase InPhase = EPhase::None);

	template<class T>
	T* GetOrCreateSaveGame(int32 InIndex = -1, EPhase InPhase = EPhase::None, TSubclassOf<USaveGameBase> InClass = T::StaticClass())
	{
		return Cast<T>(GetOrCreateSaveGame(InClass, InIndex, InPhase));
	}

	UFUNCTION(BlueprintCallable, meta = (DeterminesOutputType = "InClass"))
	USaveGameBase* GetOrCreateSaveGame(TSubclassOf<USaveGameBase> InClass, int32 InIndex = -1, EPhase InPhase = EPhase::None);

	template<class T>
	T* LoadOrCreateSaveGame(int32 InIndex = -1, EPhase InPhase = EPhase::All, TSubclassOf<USaveGameBase> InClass = T::StaticClass())
	{
		return Cast<T>(LoadOrCreateSaveGame(InClass, InIndex, InPhase));
	}

	UFUNCTION(BlueprintCallable, meta = (DeterminesOutputType = "InClass"))
	USaveGameBase* LoadOrCreateSaveGame(TSubclassOf<USaveGameBase> InClass, int32 InIndex = -1, EPhase InPhase = EPhase::All);

	template<class T>
	bool SaveSaveGame(int32 InIndex = -1, bool bRefresh = false, TSubclassOf<USaveGameBase> InClass = T::StaticClass())
	{
		return SaveSaveGame(InClass, InIndex, bRefresh);
	}

	UFUNCTION(BlueprintCallable)
	bool SaveSaveGame(TSubclassOf<USaveGameBase> InClass, int32 InIndex = -1, bool bRefresh = false);

	template<class T>
	bool SaveSaveGames(bool bRefresh = false, TSubclassOf<USaveGameBase> InClass = T::StaticClass())
	{
		return SaveSaveGames(InClass, bRefresh);
	}

	UFUNCTION(BlueprintCallable)
	bool SaveSaveGames(TSubclassOf<USaveGameBase> InClass, bool bRefresh = false);

	UFUNCTION(BlueprintCallable)
	bool SaveAllSaveGame(bool bRefresh = false);

	template<class T>
	T* LoadSaveGame(int32 InIndex = -1, EPhase InPhase = EPhase::All, TSubclassOf<USaveGameBase> InClass = T::StaticClass())
	{
		return Cast<T>(LoadSaveGame(InClass, InIndex, InPhase));
	}

	UFUNCTION(BlueprintCallable, meta = (DeterminesOutputType = "InClass"))
	USaveGameBase* LoadSaveGame(TSubclassOf<USaveGameBase> InClass, int32 InIndex = -1, EPhase InPhase = EPhase::All);

	template<class T>
	bool UnloadSaveGame(int32 InIndex = -1, EPhase InPhase = EPhase::All, TSubclassOf<USaveGameBase> InClass = T::StaticClass())
	{
		return UnloadSaveGame(InClass, InIndex, InPhase);
	}

	UFUNCTION(BlueprintCallable)
	bool UnloadSaveGame(TSubclassOf<USaveGameBase> InClass, int32 InIndex = -1, EPhase InPhase = EPhase::All);

	template<class T>
	bool ResetSaveGame(int32 InIndex = -1, TSubclassOf<USaveGameBase> InClass = T::StaticClass())
	{
		return ResetSaveGame(InClass, InIndex);
	}

	UFUNCTION(BlueprintCallable)
	bool ResetSaveGame(TSubclassOf<USaveGameBase> InClass, int32 InIndex = -1);

	template<class T>
	bool RefreshSaveGame(int32 InIndex = -1, TSubclassOf<USaveGameBase> InClass = T::StaticClass())
	{
		return RefreshSaveGame(InClass, InIndex);
	}

	UFUNCTION(BlueprintCallable)
	bool RefreshSaveGame(TSubclassOf<USaveGameBase> InClass, int32 InIndex = -1);

	template<class T>
	bool DestroySaveGame(int32 InIndex = -1, TSubclassOf<USaveGameBase> InClass = T::StaticClass())
	{
		return DestroySaveGame(InClass, InIndex);
	}

	UFUNCTION(BlueprintCallable)
	bool DestroySaveGame(TSubclassOf<USaveGameBase> InClass, int32 InIndex = -1);

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
