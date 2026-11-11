// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/GameInstance.h"
#include "Engine/EngineBaseTypes.h"
#include "Containers/Ticker.h"
#include "SaveGame/SaveGameLoadSnapshot.h"
#include "SaveGame/SaveGameLocalContext.h"
#include "SaveGame/SaveOperationTypes.h"
#include "WHGameInstance.generated.h"

class USaveGameModule;
class UModuleBase;
class ULocalPlayer;
class FInventoryClientReceiptLedger;

/**
 * 游戏实例基类
 */
UCLASS()
class WHFRAMEWORK_API UWHGameInstance : public UGameInstance
{
	GENERATED_BODY()

#if WITH_DEV_AUTOMATION_TESTS
	friend class FSaveGameTransientSessionTest;
#endif

public:
	UWHGameInstance();

public:
	virtual void Init() override;

	virtual void Shutdown() override;

	UModuleBase* FindModuleByClass(TSubclassOf<UModuleBase> InClass) const;

#if WITH_EDITOR
	UModuleBase* FindModuleTemplateForEditor(FName InName) const;
#endif
	UModuleBase* FindModuleByName(FName InName) const;

	template<class T>
	T* FindModule() const
	{
		return Cast<T>(FindModuleByClass(T::StaticClass()));
	}
	TSharedPtr<const FSaveGameLocalContext, ESPMode::ThreadSafe> GetLocalSaveContext() const { return LocalSaveContext; }
	TSharedPtr<FInventoryClientReceiptLedger, ESPMode::ThreadSafe> GetInventoryReceiptLedger(ULocalPlayer* InPlayer);
	const FString& GetLocalSaveContextError() const { return LocalSaveContextError; }
	bool EnsureLocalSaveContext();
	bool HasClaimedPrimaryInputProfile() const { return bPrimaryInputProfileClaimed; }
	void ClaimPrimaryInputProfile() { bPrimaryInputProfileClaimed = true; }

	bool SetPendingSaveLoad(FSaveGameLoadSnapshot InSnapshot, USaveGameModule* InSource = nullptr);

	void CancelPendingSaveLoad(const FGuid& InOperationId)
	{
		if (PendingSaveLoad.Context.OperationId == InOperationId)
		{
			PendingSaveLoad = FSaveGameLoadSnapshot();
			PendingLoadSource.Reset();
		}
	}

	FSaveGameLoadSnapshot ConsumePendingSaveLoad()
	{
		FSaveGameLoadSnapshot Result = MoveTemp(PendingSaveLoad);
		PendingSaveLoad = FSaveGameLoadSnapshot();
		PendingLoadSource.Reset();
		return Result;
	}

private:
	bool InitializeModules();
	bool TickModules(float DeltaSeconds);
	void TerminateModules();

protected:
	void SetModuleTemplate(UModuleBase* InTemplate);

	UPROPERTY(EditDefaultsOnly, Instanced, Category = "Modules")
	TArray<TObjectPtr<UModuleBase>> ModuleTemplates;

private:
	bool bPrimaryInputProfileClaimed = false;
	TMap<TWeakObjectPtr<ULocalPlayer>, TSharedPtr<FInventoryClientReceiptLedger, ESPMode::ThreadSafe>> InventoryReceiptLedgers;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UModuleBase>> Modules;

	FTSTicker::FDelegateHandle ModuleTickHandle;
	bool bModulesTerminated = false;

	friend class USaveGameModule;
	TSharedPtr<FSaveGameLocalContext, ESPMode::ThreadSafe> LocalSaveContext;
	FString LocalSaveContextError;
	FString RequestedLocalDataNamespace;
	void OnSaveLoadTravelFailure(UWorld* InWorld, ETravelFailure::Type InType, const FString& InError);
	FDelegateHandle SaveLoadTravelFailureHandle;
	TWeakObjectPtr<USaveGameModule> PendingLoadSource;

	FSaveGameLoadSnapshot PendingSaveLoad;
};
