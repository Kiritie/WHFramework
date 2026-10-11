#include "SaveGame/SaveGameAsyncExecutor.h"
#include "SaveGame/SaveDataSerializer.h"
#include "SaveGame/SaveGameStorage.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "HAL/FileManager.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
enum class EExternalFault : uint8 { None, Prepare, Ready, CancelAfterReady };

// 可控的原生参与者用于故障注入；它实际写入文件并检查普通 Manifest 仍未发布。
class FObservedExternalStore final : public ISaveWorldExternalStore
{
public:
    FGuid WorldId;
    FGuid StoreId;
    FGuid SaveId;
    FGuid OperationId;
    FString Root;
    EExternalFault Fault = EExternalFault::None;
    std::atomic_bool* Cancel = nullptr;
    bool bSawUnpublished = false;
    bool bPrepared = false;
    bool bReady = false;
    bool bClosed = false;

    virtual FGuid GetWorldId() const override { return WorldId; }
    virtual FGuid GetStoreId() const override { return StoreId; }
    virtual bool PrepareNative(FString& OutError) override
    {
        FSaveGameStorage Storage(0, Root, OperationId);
        FSaveManifest Current;
        bSawUnpublished = Storage.ReadManifest(SaveId, Current) && Current.CurrentGeneration == 0 &&
            Current.LastCommitOperationId != OperationId;
        if (Fault == EExternalFault::Prepare)
        { OutError = TEXT("Injected.ExternalPrepareFailure"); return false; }
        const TArray<uint8> Bytes{17, 31, 49};
        bPrepared = FSaveGameStorage::WriteBinaryDurable(Storage.GetWorldDir(SaveId) / TEXT("Inventory") /
            StoreId.ToString(EGuidFormats::Digits) / TEXT("external.checkpoint"), Bytes);
        if (!bPrepared) OutError = TEXT("Injected.ExternalCheckpointWriteFailure");
        return bPrepared;
    }
    virtual bool MarkReadyNative(FString& OutError) override
    {
        if (!bPrepared || Fault == EExternalFault::Ready)
        { OutError = TEXT("Injected.ExternalReadyFailure"); return false; }
        FSaveGameStorage Storage(0, Root, OperationId);
        const TArray<uint8> Bytes{67, 83};
        bReady = FSaveGameStorage::WriteBinaryDurable(Storage.GetWorldDir(SaveId) / TEXT("Inventory") /
            StoreId.ToString(EGuidFormats::Digits) / TEXT("external.ready"), Bytes);
        if (Fault == EExternalFault::CancelAfterReady && Cancel) Cancel->store(true);
        if (!bReady) OutError = TEXT("Injected.ExternalReadyWriteFailure");
        return bReady;
    }
    virtual void CloseNative() override { bClosed = true; }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSaveWorldBranchExecutorTest,
    "WHFramework.SaveGame.WorldBranch.ExternalPreparationPublicationAndCancellation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSaveWorldBranchExecutorTest::RunTest(const FString& Parameters)
{
    const FString Root = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"),
        TEXT("SaveWorldBranch"), FGuid::NewGuid().ToString(EGuidFormats::Digits));
    ON_SCOPE_EXIT { IFileManager::Get().DeleteDirectory(*Root, false, true); };
    for (const EExternalFault Fault : {EExternalFault::Prepare, EExternalFault::Ready,
        EExternalFault::CancelAfterReady, EExternalFault::None})
    {
        FSaveGenerationPlan Plan;
        Plan.StorageRoot = Root;
        Plan.SaveId = FGuid::NewGuid();
        Plan.OperationId = FGuid::NewGuid();
        // 临时代事务 ID 与最终发布操作 ID 可以不同，必须使用后者写回执。
        Plan.TransactionId = FGuid::NewGuid();
        Plan.bCreateSlotAtWrite = true;
        Plan.Manifest.SaveId = Plan.SaveId;
        Plan.Manifest.DisplayName = TEXT("Branch contract");
        Plan.Manifest.CreatedAt = FDateTime::UtcNow();
        Plan.Manifest.UpdatedAt = Plan.Manifest.CreatedAt;
        Plan.CurrentMap = TEXT("TestWorld");
        FSaveData Data;
        Data.SetDatas(TArray<uint8>{2, 4, 8, 16});
        FSaveModuleBytes Module;
        Module.ModuleName = TEXT("TestModule");
        Module.Version = 7;
        if (!TestTrue(TEXT("Module bytes built"), FSaveDataSerializer::BuildModuleFile(Module.ModuleName,
            Module.Version, FParameter(Data), Module.Bytes))) return false;
        Plan.Modules.Add(MoveTemp(Module));
        std::atomic_bool Cancel{false};
        TSharedPtr<FObservedExternalStore, ESPMode::ThreadSafe> Store = MakeShared<FObservedExternalStore, ESPMode::ThreadSafe>();
        Store->WorldId = FGuid::NewGuid();
        Store->StoreId = FGuid::NewGuid();
        Store->SaveId = Plan.SaveId;
        Store->OperationId = Plan.OperationId;
        Store->Root = Root;
        Store->Fault = Fault;
        Store->Cancel = &Cancel;
        Plan.ExternalOperation = MakeShared<FSaveWorldExternalOperation, ESPMode::ThreadSafe>();
        Plan.ExternalOperation->Store = Store;
        Plan.ExternalOperation->Context.OperationId = Plan.OperationId;
        Plan.ExternalOperation->Context.TargetSaveId = Plan.SaveId;
        Plan.ExternalOperation->Context.TargetGeneration = 1;
        Plan.ExternalOperation->Context.StorageRoot = Root;
        Plan.ExternalOperation->Context.bDeleteWorldOnAbort = true;
        FSavePendingWorld Intent;
        Intent.OperationId = Plan.OperationId;
        Intent.TargetSaveId = Plan.SaveId;
        Intent.WorldId = Store->WorldId;
        Intent.StoreId = Store->StoreId;
        Intent.bDeleteWorldOnAbort = true;
        FSaveGameStorage Storage(0, Root, Plan.TransactionId);
        FString Error;
        if (!TestTrue(TEXT("Intent persisted before target"), Storage.BeginPendingWorld(Intent,
            Plan.ExternalOperation->PendingLease, Error))) return false;
        TestFalse(TEXT("Preparing intent does not expose a target directory"),
            IFileManager::Get().DirectoryExists(*Storage.GetWorldDir(Plan.SaveId)));
        if (!TestTrue(TEXT("Original worker owns target"), Storage.BeginWorldWrite(Plan.SaveId, nullptr, {}) ==
            ESaveWorldWriteResult::Acquired)) return false;
        const FSaveOperationResult Result = FSaveGameAsyncExecutor::Execute(Plan, Cancel, Storage);
        TestTrue(TEXT("External preparation observes only gen0"), Store->bSawUnpublished);
        FSaveManifest Observed;
        TestTrue(TEXT("Target manifest can be inspected"), Storage.ReadManifest(Plan.SaveId, Observed));
        if (Fault != EExternalFault::None)
        {
            TestFalse(TEXT("Failure or cancellation has no success receipt"), bool(Result));
            TestEqual(TEXT("Unpublished target remains gen0"), Observed.CurrentGeneration, 0);
            TestFalse(TEXT("No external publication flag on failure"), Plan.ExternalOperation->bFilesCommitted.load());
            Store->CloseNative();
            TestTrue(TEXT("Exact intent can retire failed branch"), Storage.AbortPendingWorld(Plan.ExternalOperation->PendingLease, Error));
            TestFalse(TEXT("Failed branch directory was retired"), IFileManager::Get().DirectoryExists(*Storage.GetWorldDir(Plan.SaveId)));
        }
        else
        {
            TestTrue(TEXT("Ready native store and ordinary generation commit together"), bool(Result));
            TestTrue(TEXT("Native store was made ready first"), Store->bReady);
            TestEqual(TEXT("Committed target generation"), Observed.CurrentGeneration, 1);
            TestEqual(TEXT("Manifest uses save operation id, not voxel/temp transaction id"), Observed.LastCommitOperationId, Plan.OperationId);
            TestTrue(TEXT("Publication flag proves ordinary manifest commit"), Plan.ExternalOperation->bFilesCommitted.load());
            TestTrue(TEXT("Whole committed generation is available"), Storage.IsGenerationComplete(Plan.SaveId, 1, Observed));
            FSaveManifest Unauthorized = Observed;
            Unauthorized.DisplayName = TEXT("Writer without pending ownership");
            TestFalse(TEXT("An outstanding branch cannot be rewritten without its actual lease"),
                Storage.WriteManifestAtomic(Plan.SaveId, Unauthorized));
            TestFalse(TEXT("Even the pending owner cannot overwrite a positive generation with gen0"),
                Storage.WriteManifestAtomic(Plan.SaveId, Plan.Manifest, Plan.ExternalOperation->PendingLease));
            FSaveManifest StillCommitted;
            TestTrue(TEXT("Rejected manifest writers retain the published receipt"), Storage.ReadManifest(Plan.SaveId, StillCommitted));
            TestEqual(TEXT("Rejected manifest writers retain the positive generation"), StillCommitted.CurrentGeneration, 1);
            TestEqual(TEXT("Rejected manifest writers retain the operation identity"), StillCommitted.LastCommitOperationId, Plan.OperationId);
            TestTrue(TEXT("Commit durability can be independently confirmed"), Storage.ConfirmManifestCommit(Plan.SaveId, Plan.OperationId, 1, Error));
            TestTrue(TEXT("Intent completes only after durable receipt"), Storage.CompletePendingWorld(Plan.ExternalOperation->PendingLease, Error));
            TestFalse(TEXT("Completed target has no pending load guard"), Storage.HasPendingWorld(Plan.SaveId));
            const FSaveOperationResult Overwrite = FSaveGameAsyncExecutor::Execute(Plan, Cancel, Storage);
            TestFalse(TEXT("An old branch plan cannot overwrite a nonempty target"), bool(Overwrite));
            FSaveManifest Retained;
            TestTrue(TEXT("Published manifest retained after rejected overwrite"), Storage.ReadManifest(Plan.SaveId, Retained));
            TestEqual(TEXT("Rejected overwrite leaves revision unchanged"), Retained.CurrentGeneration, 1);
            TestEqual(TEXT("Rejected overwrite leaves receipt unchanged"), Retained.LastCommitOperationId, Plan.OperationId);
        }
        Store->Cancel = nullptr;
        Plan.ExternalOperation.Reset();
        Store.Reset();
        Storage.EndWorldWrite();
    }
    return true;
}
#endif
