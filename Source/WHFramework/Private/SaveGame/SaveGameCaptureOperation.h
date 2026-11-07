#pragma once

#include "SaveGame/SaveManifestTypes.h"
#include "SaveGame/SaveGameAsyncExecutor.h"
#include "SaveGame/SaveGameStorage.h"

struct FCreatedSlotCleanupTask
{
	TSharedPtr<const FSaveGenerationPlan, ESPMode::ThreadSafe> Plan;
	void operator()() const
	{
		FSaveGameStorage OwnerStorage(Plan->UserIndex, Plan->StorageRoot, Plan->TransactionId);
		if (OwnerStorage.BeginWorldWrite(Plan->SaveId, nullptr, {}) != ESaveWorldWriteResult::Acquired)
		{
			UE_LOG(LogTemp, Warning, TEXT("Created slot cleanup could not acquire native write ownership; directory retained: %s"), *Plan->SaveId.ToString());
			return;
		}
        if (Plan->ExternalOperation)
        {
            FString Error;
            Plan->ExternalOperation->Store->CloseNative();
            if (!OwnerStorage.AbortPendingWorld(Plan->ExternalOperation->PendingLease, Error))
                UE_LOG(LogTemp, Error, TEXT("未发布世界分支保留恢复 intent：%s"), *Error);
            return;
        }
		FSaveManifest Current;
		// 只删除原请求创建、且仍没有已提交代的槽；任务不读取模块或 World。
		if (OwnerStorage.ReadManifest(Plan->SaveId, Current) && Current.CurrentGeneration == 0 &&
			!OwnerStorage.DeleteWorldDirectory(Plan->SaveId))
			UE_LOG(LogTemp, Error, TEXT("Failed save capture left an uncommitted created slot: %s"), *Plan->SaveId.ToString());
	}
};
