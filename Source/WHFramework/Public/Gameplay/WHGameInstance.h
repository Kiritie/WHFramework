// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/GameInstance.h"
#include "Engine/EngineBaseTypes.h"
#include "SaveGame/SaveGameLoadSnapshot.h"
#include "SaveGame/SaveGameLocalContext.h"
#include "WHGameInstance.generated.h"

class USaveGameModule;

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
	TSharedPtr<const FSaveGameLocalContext, ESPMode::ThreadSafe> GetLocalSaveContext() const { return LocalSaveContext; }
	const FString& GetLocalSaveContextError() const { return LocalSaveContextError; }

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
	friend class USaveGameModule;
	/** 由持久存档模块按需调用；同一 GameInstance 重用已取得的租约。 */
	bool EnsureLocalSaveContext();
	TSharedPtr<FSaveGameLocalContext, ESPMode::ThreadSafe> LocalSaveContext;
	FString LocalSaveContextError;
	FString RequestedLocalDataNamespace;
	void OnSaveLoadTravelFailure(UWorld* InWorld, ETravelFailure::Type InType, const FString& InError);
	FDelegateHandle SaveLoadTravelFailureHandle;
	TWeakObjectPtr<USaveGameModule> PendingLoadSource;

	FSaveGameLoadSnapshot PendingSaveLoad;
};
