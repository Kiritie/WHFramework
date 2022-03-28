// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "SaveGameModuleTypes.h"
#include "Base/SaveGameBase.h"
#include "Main/Base/ModuleBase.h"
#include "Kismet/GameplayStatics.h"

#include "SaveGameModule.generated.h"

class ATargetPoint;

UCLASS()
class WHFRAMEWORK_API ASaveGameModule : public AModuleBase
{
	GENERATED_BODY()
	
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

protected:
	UPROPERTY(EditAnywhere, Category = "UserData")
	int32 UserIndex;

public:
	UFUNCTION(BlueprintPure, Category = "UserData")
	int32 GetUserIndex() const { return UserIndex; }

	UFUNCTION(BlueprintCallable, Category = "UserData")
	void SetUserIndex(int32 InUserIndex) { UserIndex = InUserIndex; }

protected:
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	TMap<FName, USaveGameBase*> AllSaveGames;

public:
	template<class T>
	bool HasSaveGame(int32 InIndex = -1, TSubclassOf<USaveGameBase> InSaveGameClass = T::StaticClass()) const
	{
		if(!InSaveGameClass) return false;

		const FName SaveName = InSaveGameClass.GetDefaultObject()->GetSaveName();
		if(InIndex == -1)
		{
			return AllSaveGames.Contains(SaveName);
		}
		else
		{
			return UGameplayStatics::DoesSaveGameExist(GetSaveSlotName(SaveName, InIndex), UserIndex);
		}
	}

	UFUNCTION(BlueprintPure, meta = (DisplayName = "HasSaveGame"))
	bool K2_HasSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass, int32 InIndex = -1) const;

	template<class T>
	T* GetSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass = T::StaticClass()) const
	{
		if(!InSaveGameClass) return nullptr;

		const FName SaveName = InSaveGameClass.GetDefaultObject()->GetSaveName();
		if(AllSaveGames.Contains(SaveName))
		{
			return Cast<T>(AllSaveGames[SaveName]);
		}
		return nullptr;
	}

	UFUNCTION(BlueprintPure, meta = (DisplayName = "GetSaveGame", DeterminesOutputType = "InSaveGameClass"))
	USaveGameBase* K2_GetSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass) const;

	template<class T>
	T* CreateSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass = T::StaticClass())
	{
		if(!InSaveGameClass) return nullptr;
		
		const FName SaveName = InSaveGameClass.GetDefaultObject()->GetSaveName();
		if(!AllSaveGames.Contains(SaveName))
		{
			if(USaveGameBase* SaveGame = Cast<USaveGameBase>(UGameplayStatics::CreateSaveGameObject(InSaveGameClass)))
			{
				AllSaveGames.Add(SaveName, SaveGame);
				SaveGame->OnCreate();
				return Cast<T>(SaveGame);
			}
		}
		return nullptr;
	}

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "CreateSaveGame", DeterminesOutputType = "InSaveGameClass"))
	USaveGameBase* K2_CreateSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass);

	template<class T>
	bool SaveSaveGame(int32 InIndex, bool bRefresh = false, TSubclassOf<USaveGameBase> InSaveGameClass = T::StaticClass())
	{
		if(!InSaveGameClass) return false;

		const FName SaveName = InSaveGameClass.GetDefaultObject()->GetSaveName();
		if(AllSaveGames.Contains(SaveName))
		{
			if(USaveGameBase* SaveGame = AllSaveGames[SaveName])
			{
				if(bRefresh)
				{
					RefreshSaveGame<T>(InSaveGameClass);
				}
				SaveGame->OnSave(InIndex);
				return UGameplayStatics::SaveGameToSlot(SaveGame, GetSaveSlotName(SaveName, InIndex), UserIndex);
			}
			return true;
		}
		return false;
	}

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "SaveSaveGame"))
	bool K2_SaveSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass, int32 InIndex, bool  bRefresh = false);

	template<class T>
	T* LoadSaveGame(int32 InIndex, TSubclassOf<USaveGameBase> InSaveGameClass = T::StaticClass())
	{
		if(!InSaveGameClass) return nullptr;

		const FName SaveName = InSaveGameClass.GetDefaultObject()->GetSaveName();
		if (AllSaveGames.Contains(SaveName))
		{
			AllSaveGames[SaveName]->OnLoad();
			return Cast<T>(AllSaveGames[SaveName]);
		}
		else if(UGameplayStatics::DoesSaveGameExist(GetSaveSlotName(SaveName, InIndex), UserIndex))
		{
			if(USaveGameBase* SaveGame = Cast<USaveGameBase>(UGameplayStatics::LoadGameFromSlot(GetSaveSlotName(SaveName, InIndex), UserIndex)))
			{
				AllSaveGames.Add(SaveName, SaveGame);
				SaveGame->OnLoad();
				return Cast<T>(SaveGame);
			}
		}
		return nullptr;
	}

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "LoadSaveGame", DeterminesOutputType = "InSaveGameClass"))
	USaveGameBase* K2_LoadSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass, int32 InIndex);

	template<class T>
	bool UnloadSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass = T::StaticClass())
	{
		if(!InSaveGameClass) return false;

		const FName SaveName = InSaveGameClass.GetDefaultObject()->GetSaveName();
		if(AllSaveGames.Contains(SaveName))
		{
			AllSaveGames.Remove(SaveName);
			if(USaveGameBase* SaveGame = AllSaveGames[SaveName])
			{
				SaveGame->OnUnload();
				SaveGame->ConditionalBeginDestroy();
			}
		}
		return false;
	}

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "UnloadSaveGame"))
	bool K2_UnloadSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass);

	template<class T>
	bool ResetSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass = T::StaticClass())
	{
		if(!InSaveGameClass) return false;

		const FName SaveName = InSaveGameClass.GetDefaultObject()->GetSaveName();
		if(AllSaveGames.Contains(SaveName))
		{
			if(USaveGameBase* SaveGame = AllSaveGames[SaveName])
			{
				SaveGame->OnReset();
			}
		}
		return false;
	}

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "ResetSaveGame"))
	bool K2_ResetSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass);

	template<class T>
	bool RefreshSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass = T::StaticClass())
	{
		if(!InSaveGameClass) return false;

		const FName SaveName = InSaveGameClass.GetDefaultObject()->GetSaveName();
		if(AllSaveGames.Contains(SaveName))
		{
			if(USaveGameBase* SaveGame = AllSaveGames[SaveName])
			{
				SaveGame->OnRefresh();
			}
		}
		return false;
	}

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "RefreshSaveGame"))
	bool K2_RefreshSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass);

	template<class T>
	bool DestroySaveGame(int32 InIndex, TSubclassOf<USaveGameBase> InSaveGameClass = T::StaticClass())
	{
		if(!InSaveGameClass) return false;

		const FName SaveName = InSaveGameClass.GetDefaultObject()->GetSaveName();
		if(AllSaveGames.Contains(SaveName))
		{
			AllSaveGames.Remove(SaveName);
			if(USaveGameBase* SaveGame = AllSaveGames[SaveName])
			{
				SaveGame->OnUnload();
				SaveGame->OnDestroy();
				SaveGame->ConditionalBeginDestroy();
			}
		}
		if (UGameplayStatics::DoesSaveGameExist(GetSaveSlotName(SaveName, InIndex), UserIndex))
		{
			UGameplayStatics::DeleteGameInSlot(GetSaveSlotName(SaveName, InIndex), UserIndex);
			return true;
		}
		return false;
	}

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "DestroySaveGame"))
	bool K2_DestroySaveGame(TSubclassOf<USaveGameBase> InSaveGameClass, int32 InIndex);

	UFUNCTION(BlueprintPure)
	FString GetSaveSlotName(FName InSaveName, int32 InIndex) const;

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
