#include "SaveGame/SaveGameStorage.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Ability/Inventory/InventoryJournal.h"
#include "HAL/FileManager.h"
#include "JsonObjectConverter.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "SaveGame/SaveGameModuleTypes.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

namespace SaveBranchTests
{
    struct FDirectory
    {
        FString Path = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Automation/SaveBranches") /
            FGuid::NewGuid().ToString(EGuidFormats::Digits));
        ~FDirectory() { IFileManager::Get().DeleteDirectory(*Path, false, true); }
    };

    bool CreateSlot(FSaveGameStorage& Storage, const FGuid& SaveId, int32 Generation, const FString& Name = TEXT("Fixture"),
        const TSharedPtr<FSavePendingWorldLease, ESPMode::ThreadSafe>& Lease = nullptr)
    {
        FSaveManifest Manifest;
        Manifest.SaveId = SaveId;
        Manifest.DisplayName = Name;
        if (!Storage.EnsureRoot() || !Storage.CreateWorldDirectory(SaveId)) return false;
        if (Generation > 0)
        {
            Manifest.CurrentGeneration = Generation;
            Manifest.LastCommitOperationId = FGuid::NewGuid();
            Manifest.ModuleVersions.Add(TEXT("BranchFixture"), 1);
            const TArray<uint8> Bytes = {11, 22, 33, 44};
            if (!Storage.PrepareTempGeneration(SaveId, 0, Generation) ||
                !FSaveGameStorage::WriteBinaryDurable(Storage.GetTempModuleFilePath(SaveId, Generation, TEXT("BranchFixture")), Bytes) ||
                !Storage.FlushTempGeneration(SaveId, Generation) || !Storage.CommitGeneration(SaveId, Generation)) return false;
        }
        return Storage.WriteManifestAtomic(SaveId, Manifest, Lease);
    }

    FSavePendingWorld Intent(const FGuid& SourceId, bool bDeleteWorld = true)
    {
        FSavePendingWorld Value;
        Value.OperationId = FGuid::NewGuid();
        Value.TargetSaveId = FGuid::NewGuid();
        Value.SourceSaveId = SourceId;
        Value.SourceGeneration = SourceId.IsValid() ? 1 : 0;
        Value.TargetGeneration = 1;
        Value.StoreId = FGuid::NewGuid();
        Value.WorldId = FGuid::NewGuid();
        Value.bDeleteWorldOnAbort = bDeleteWorld;
        return Value;
    }

    FString StoreFixture(const FSaveGameStorage& Storage, const FSavePendingWorld& Intent)
    {
        return Storage.GetWorldDir(Intent.TargetSaveId) / TEXT("Inventory") /
            Intent.StoreId.ToString(EGuidFormats::Digits) / TEXT("fixture.bin");
    }

    // Storage 测试只负责目录所有权与最终清单；经济格式与 WAL 用 InventoryRuntime 测试验证。
    bool PrepareTarget(FSaveGameStorage& Storage, const FSavePendingWorld& Intent,
        const TSharedPtr<FSavePendingWorldLease, ESPMode::ThreadSafe>& Lease, bool bExistingEmpty = false)
    {
        if (!bExistingEmpty && !CreateSlot(Storage, Intent.TargetSaveId, 0, TEXT("Fixture"), Lease)) return false;
        const TArray<uint8> Bytes = {77, 88, 99};
        return Storage.PrepareTempGeneration(Intent.TargetSaveId, 0, Intent.TargetGeneration) &&
            FSaveGameStorage::WriteBinaryDurable(Storage.GetTempModuleFilePath(Intent.TargetSaveId, Intent.TargetGeneration, TEXT("BranchFixture")), Bytes) &&
            FSaveGameStorage::WriteBinaryDurable(StoreFixture(Storage, Intent), Bytes);
    }

    bool PublishTarget(FSaveGameStorage& Storage, const FSavePendingWorld& Intent,
        const TSharedPtr<FSavePendingWorldLease, ESPMode::ThreadSafe>& Lease)
    {
        FSaveManifest Manifest;
        if (!Storage.ReadManifest(Intent.TargetSaveId, Manifest) || !Storage.FlushTempGeneration(Intent.TargetSaveId, Intent.TargetGeneration) ||
            !Storage.CommitGeneration(Intent.TargetSaveId, Intent.TargetGeneration)) return false;
        Manifest.CurrentGeneration = Intent.TargetGeneration;
        Manifest.LastCommitOperationId = Intent.OperationId;
        Manifest.ModuleVersions.Add(TEXT("BranchFixture"), 1);
        return Storage.WriteManifestAtomic(Intent.TargetSaveId, Manifest, Lease);
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSaveBranchAbortRecoveryTest,
    "WHFramework.SaveGame.Branch.PendingRecoveryAndBusyOwner",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSaveBranchAbortRecoveryTest::RunTest(const FString& InParameters)
{
    using namespace SaveBranchTests;
    FDirectory Directory;
    FSaveGameStorage Storage(0, Directory.Path);
    const FGuid SourceId = FGuid::NewGuid();
    if (!CreateSlot(Storage, SourceId, 1)) return false;
    const FSavePendingWorld Pending = Intent(SourceId);
    TSharedPtr<FSavePendingWorldLease, ESPMode::ThreadSafe> Lease;
    FString Error;
    if (!Storage.BeginPendingWorld(Pending, Lease, Error) || !PrepareTarget(Storage, Pending, Lease)) return false;
    TArray<FString> Errors;
    FSaveGameStorage Other(0, Directory.Path);
    Other.RecoverPendingWorlds(Errors);
    TestEqual(TEXT("活跃跨进程锁阻止恢复抢走操作"), Errors.Num(), 1);
    Other.CleanupAllTempGenerations();
    TestTrue(TEXT("gen0 常规清理跳过仍有 intent 的临时代"),
        IFileManager::Get().FileExists(*Storage.GetTempModuleFilePath(Pending.TargetSaveId, 1, TEXT("BranchFixture"))));
    TestFalse(TEXT("普通删除不能删除正在准备的目标"), Other.DeleteWorldDirectory(Pending.TargetSaveId));
    Lease.Reset();
    Other.RecoverPendingWorlds(Errors);
    TestTrue(TEXT("释放操作句柄后恢复完成"), Errors.IsEmpty());
    TestFalse(TEXT("未提交的新槽连同其经济分支被回收"), IFileManager::Get().DirectoryExists(*Storage.GetWorldDir(Pending.TargetSaveId)));
    TestFalse(TEXT("已处理 intent 被删除"), IFileManager::Get().FileExists(*Storage.GetPendingWorldPath(Pending.TargetSaveId)));
    TArray<uint8> SourceBytes;
    TestTrue(TEXT("源代次保持原始内容"), Storage.ReadBinary(Storage.GetModuleFilePath(SourceId, 1, TEXT("BranchFixture")), SourceBytes) &&
        SourceBytes == TArray<uint8>({11, 22, 33, 44}));
    Other.RecoverPendingWorlds(Errors);
    TestTrue(TEXT("重复启动恢复幂等"), Errors.IsEmpty());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSaveBranchPublishedRecoveryTest,
    "WHFramework.SaveGame.Branch.PublishedDecisionSurvivesAbort",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSaveBranchPublishedRecoveryTest::RunTest(const FString& InParameters)
{
    using namespace SaveBranchTests;
    FDirectory Directory;
    FSaveGameStorage Storage(0, Directory.Path);
    const FSavePendingWorld Pending = Intent({});
    TSharedPtr<FSavePendingWorldLease, ESPMode::ThreadSafe> Lease;
    FString Error;
    if (!Storage.BeginPendingWorld(Pending, Lease, Error) || !PrepareTarget(Storage, Pending, Lease) || !PublishTarget(Storage, Pending, Lease)) return false;
    TestTrue(TEXT("清单发布之后的 abort 只能完成回执"), Storage.AbortPendingWorld(Lease, Error));
    TestTrue(TEXT("目标经济文件保留"), IFileManager::Get().FileExists(*StoreFixture(Storage, Pending)));
    TestTrue(TEXT("目标普通代次保留"), IFileManager::Get().FileExists(*Storage.GetModuleFilePath(Pending.TargetSaveId, 1, TEXT("BranchFixture"))));
    FSaveManifest Manifest;
    TestTrue(TEXT("最终清单保留同一提交身份"), Storage.ReadManifest(Pending.TargetSaveId, Manifest) &&
        Manifest.LastCommitOperationId == Pending.OperationId && Manifest.CurrentGeneration == 1);
    Lease.Reset();
    TArray<FString> Errors;
    Storage.RecoverPendingWorlds(Errors);
    Storage.CleanupAllTempGenerations();
    TestTrue(TEXT("后续恢复不会重新删除已提交分支"), Errors.IsEmpty() && IFileManager::Get().FileExists(*StoreFixture(Storage, Pending)));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSaveBranchEmptySlotRecoveryTest,
    "WHFramework.SaveGame.Branch.ExistingEmptySlotRetainsMetadata",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSaveBranchEmptySlotRecoveryTest::RunTest(const FString& InParameters)
{
    using namespace SaveBranchTests;
    FDirectory Directory;
    FSaveGameStorage Storage(0, Directory.Path);
    const FSavePendingWorld Pending = Intent({}, false);
    if (!CreateSlot(Storage, Pending.TargetSaveId, 0, TEXT("Keep my empty slot"))) return false;
    TSharedPtr<FSavePendingWorldLease, ESPMode::ThreadSafe> Lease;
    FString Error;
    if (!Storage.BeginPendingWorld(Pending, Lease, Error) || !PrepareTarget(Storage, Pending, Lease, true)) return false;
    if (!Storage.FlushTempGeneration(Pending.TargetSaveId, 1) || !Storage.CommitGeneration(Pending.TargetSaveId, 1)) return false;
    Lease.Reset();
    TArray<FString> Errors;
    Storage.RecoverPendingWorlds(Errors);
    FSaveManifest Manifest;
    TestTrue(TEXT("只发布代目录但未发布清单仍是未提交"), Errors.IsEmpty() && Storage.ReadManifest(Pending.TargetSaveId, Manifest));
    TestEqual(TEXT("原有空槽名称保留"), Manifest.DisplayName, FString(TEXT("Keep my empty slot")));
    TestEqual(TEXT("原有空槽代次保留"), Manifest.CurrentGeneration, 0);
    TestFalse(TEXT("目标经济目录回收"), IFileManager::Get().FileExists(*StoreFixture(Storage, Pending)));
    TestFalse(TEXT("尚未提交的正式代目录也回收"), IFileManager::Get().DirectoryExists(*Storage.GetGenerationDir(Pending.TargetSaveId, 1)));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSaveBranchCorruptRecoveryTest,
    "WHFramework.SaveGame.Branch.CorruptIntentAndIncompletePublishedGeneration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSaveBranchCorruptRecoveryTest::RunTest(const FString& InParameters)
{
    using namespace SaveBranchTests;
    FDirectory Directory;
    FSaveGameStorage Storage(0, Directory.Path);
    FSavePendingWorld Pending = Intent({});
    TSharedPtr<FSavePendingWorldLease, ESPMode::ThreadSafe> Lease;
    FString Error;
    if (!Storage.BeginPendingWorld(Pending, Lease, Error) || !PrepareTarget(Storage, Pending, Lease)) return false;
    Lease.Reset();
    TArray<uint8> Bytes;
    if (!Storage.ReadBinary(Storage.GetPendingWorldPath(Pending.TargetSaveId), Bytes) || Bytes.IsEmpty()) return false;
    Bytes[0] ^= 0x20;
    if (!FSaveGameStorage::WriteBinaryDurable(Storage.GetPendingWorldPath(Pending.TargetSaveId), Bytes)) return false;
    TArray<FString> Errors;
    Storage.RecoverPendingWorlds(Errors);
    Storage.CleanupAllTempGenerations();
    TestEqual(TEXT("损坏 intent 有明确恢复错误"), Errors.Num(), 1);
    TestTrue(TEXT("损坏 intent 不授权删除任何目标数据"), IFileManager::Get().FileExists(*StoreFixture(Storage, Pending)) &&
        IFileManager::Get().FileExists(*Storage.GetTempModuleFilePath(Pending.TargetSaveId, 1, TEXT("BranchFixture"))));

    Pending = Intent({});
    if (!Storage.BeginPendingWorld(Pending, Lease, Error) || !PrepareTarget(Storage, Pending, Lease) || !PublishTarget(Storage, Pending, Lease)) return false;
    if (!IFileManager::Get().Delete(*Storage.GetModuleFilePath(Pending.TargetSaveId, 1, TEXT("BranchFixture")), false, true)) return false;
    TestFalse(TEXT("已发布但文件缺失时不猜测回滚"), Storage.AbortPendingWorld(Lease, Error));
    TestTrue(TEXT("已发布的不完整目标保留经济事实和 intent 供明确恢复"),
        IFileManager::Get().FileExists(*StoreFixture(Storage, Pending)) && IFileManager::Get().FileExists(*Storage.GetPendingWorldPath(Pending.TargetSaveId)));

    Lease.Reset();
    Pending = Intent({});
    if (!Storage.BeginPendingWorld(Pending, Lease, Error) || !PrepareTarget(Storage, Pending, Lease)) return false;
    FSaveManifest Corrupt;
    if (!Storage.ReadManifest(Pending.TargetSaveId, Corrupt)) return false;
    Corrupt.CurrentGeneration = -1;
    FString Json;
    if (!FJsonObjectConverter::UStructToJsonObjectString(Corrupt, Json) ||
        !Storage.WriteTextAtomic(Storage.GetManifestPath(Pending.TargetSaveId), Json)) return false;
    TestFalse(TEXT("可解析 JSON 的负代数也是损坏清单"), Storage.ReadManifest(Pending.TargetSaveId, Corrupt));
    TestFalse(TEXT("负代数不能被当作未提交 gen0 回收"), Storage.AbortPendingWorld(Lease, Error));
    TestTrue(TEXT("负代数清单保留目标经济数据和恢复 intent"), Storage.HasPendingWorld(Pending.TargetSaveId) &&
        IFileManager::Get().FileExists(*StoreFixture(Storage, Pending)));
    Json = FString::Printf(TEXT("{\"saveId\":\"%s\"}"), *Pending.TargetSaveId.ToString());
    if (!Storage.WriteTextAtomic(Storage.GetManifestPath(Pending.TargetSaveId), Json)) return false;
    TestFalse(TEXT("缺失代数字段不能回填默认 gen0"), Storage.ReadManifest(Pending.TargetSaveId, Corrupt));
    TestFalse(TEXT("缺字段的可解析清单同样不能授权回收"), Storage.AbortPendingWorld(Lease, Error));
    TestTrue(TEXT("缺字段清单保留目标经济数据"), IFileManager::Get().FileExists(*StoreFixture(Storage, Pending)));
    Json = FString::Printf(TEXT("{\"saveId\":\"%s\",\"currentGeneration\":1,\"currentGeneration\":0}"),
        *Pending.TargetSaveId.ToString());
    if (!Storage.WriteTextAtomic(Storage.GetManifestPath(Pending.TargetSaveId), Json)) return false;
    TestFalse(TEXT("完全同名代数字段不能经 JSON 覆盖隐藏"), Storage.ReadManifest(Pending.TargetSaveId, Corrupt));
    TestFalse(TEXT("重复字段不授权回收"), Storage.AbortPendingWorld(Lease, Error));
    TestTrue(TEXT("重复字段清单保留目标经济数据"), IFileManager::Get().FileExists(*StoreFixture(Storage, Pending)));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSaveBranchProtectsExistingSlotTest,
    "WHFramework.SaveGame.Branch.NonemptyTargetAndActiveStoreCannotBeDeleted",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSaveBranchProtectsExistingSlotTest::RunTest(const FString& InParameters)
{
    using namespace SaveBranchTests;
    FDirectory Directory;
    FSaveGameStorage Storage(0, Directory.Path);
    FSavePendingWorld Pending = Intent({});
    if (!CreateSlot(Storage, Pending.TargetSaveId, 1)) return false;
    TSharedPtr<FSavePendingWorldLease, ESPMode::ThreadSafe> Lease;
    FString Error;
    TestFalse(TEXT("分支入口拒绝覆盖非空槽"), Storage.BeginPendingWorld(Pending, Lease, Error));
    TestTrue(TEXT("拒绝后原清单仍在"), IFileManager::Get().FileExists(*Storage.GetManifestPath(Pending.TargetSaveId)));
    const FString InventoryRoot = Storage.GetWorldDir(Pending.TargetSaveId) / TEXT("Inventory");
    if (!IFileManager::Get().MakeDirectory(*InventoryRoot, true)) return false;
    FInventoryJournal Journal;
    FInventoryCheckpoint Checkpoint;
    TArray<FInventoryJournalRecord> Records;
    if (!Journal.Open(InventoryRoot / Pending.StoreId.ToString(EGuidFormats::Digits), Pending.WorldId, Pending.StoreId,
        true, Checkpoint, Records, Error)) return false;
    if (!Storage.PrepareTempGeneration(Pending.TargetSaveId, 1, 2)) return false;
    Storage.CleanupAllTempGenerations();
    TestTrue(TEXT("启动清理不能删除活跃 Journal 所属槽的在存临时代"),
        IFileManager::Get().DirectoryExists(*Storage.GetTempGenerationDir(Pending.TargetSaveId, 2)));
    TestFalse(TEXT("另一个有效 Journal 句柄阻止整槽删除"), Storage.DeleteWorldDirectory(Pending.TargetSaveId));
    TestTrue(TEXT("失败删除不先移走普通清单"), IFileManager::Get().FileExists(*Storage.GetManifestPath(Pending.TargetSaveId)));
    Journal.Close();
    auto Pin = FSaveGameGenerationReadLease::Acquire(Storage.GetGenerationDir(Pending.TargetSaveId, 1));
    if (!Pin) return false;
    TestFalse(TEXT("普通代次读租约同样阻止删除"), Storage.DeleteWorldDirectory(Pending.TargetSaveId));
    Pin.Reset();
    TestTrue(TEXT("所有真实所有者退出后原子退役整槽"), Storage.DeleteWorldDirectory(Pending.TargetSaveId));
    TestFalse(TEXT("已删除槽退出可加载目录"), IFileManager::Get().DirectoryExists(*Storage.GetWorldDir(Pending.TargetSaveId)));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSaveBranchSourceLeaseTest,
    "WHFramework.SaveGame.Branch.SourceLeaseProtectsWholeCopy",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSaveBranchSourceLeaseTest::RunTest(const FString& InParameters)
{
    using namespace SaveBranchTests;
    FDirectory Directory;
    FSaveGameStorage Storage(0, Directory.Path);
    FSaveGameStorage Other(0, Directory.Path);
    const FGuid SourceId = FGuid::NewGuid();
    if (!CreateSlot(Storage, SourceId, 1)) return false;
    if (!Storage.PrepareTempGeneration(SourceId, 1, 2)) return false;
    FString Error;
    TSharedPtr<FSaveWorldSourceLease, ESPMode::ThreadSafe> SourceLease;
    TSharedPtr<FSaveWorldSourceLease, ESPMode::ThreadSafe> OtherLease;
    if (!Storage.AcquireWorldSourceLease(SourceId, SourceLease, Error)) return false;
    TestFalse(TEXT("源租约不创建待提交 intent"), Storage.HasPendingWorld(SourceId));
    TestFalse(TEXT("同源复制忙碌时不得重入"), Other.AcquireWorldSourceLease(SourceId, OtherLease, Error));
    TestFalse(TEXT("经济导出句柄关闭后源槽仍受复制租约保护"), Other.DeleteWorldDirectory(SourceId));
    Other.CleanupAllTempGenerations();
    TestTrue(TEXT("普通清理同样尊重复制源原生租约"),
        IFileManager::Get().DirectoryExists(*Storage.GetTempGenerationDir(SourceId, 2)));
    TestTrue(TEXT("复制锁失败不移走源清单"), IFileManager::Get().FileExists(*Storage.GetManifestPath(SourceId)));
    SourceLease.Reset();
    TestTrue(TEXT("复制完成后下一次源租约可取得"), Other.AcquireWorldSourceLease(SourceId, OtherLease, Error));
    OtherLease.Reset();
    TestTrue(TEXT("所有源租约退出后允许删除"), Storage.DeleteWorldDirectory(SourceId));

    const FSavePendingWorld Pending = Intent({});
    TSharedPtr<FSavePendingWorldLease, ESPMode::ThreadSafe> PendingLease;
    if (!Storage.BeginPendingWorld(Pending, PendingLease, Error) || !PrepareTarget(Storage, Pending, PendingLease)) return false;
    PendingLease.Reset();
    TestFalse(TEXT("遗留 intent 即使无活跃锁也不能作为复制源"),
        Other.AcquireWorldSourceLease(Pending.TargetSaveId, OtherLease, Error));
    TestTrue(TEXT("拒绝源租约不会清除未决数据"), Storage.HasPendingWorld(Pending.TargetSaveId) &&
        IFileManager::Get().FileExists(*StoreFixture(Storage, Pending)));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSaveBranchStaleManifestTest,
    "WHFramework.SaveGame.Branch.StaleManifestCannotRollBackPublishedBranch",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSaveBranchStaleManifestTest::RunTest(const FString& InParameters)
{
    using namespace SaveBranchTests;
    FDirectory Directory;
    FSaveGameStorage Storage(0, Directory.Path);
    FSaveGameStorage Other(0, Directory.Path);
    const FSavePendingWorld Pending = Intent({});
    TSharedPtr<FSavePendingWorldLease, ESPMode::ThreadSafe> Lease;
    FString Error;
    if (!Storage.BeginPendingWorld(Pending, Lease, Error) || !PrepareTarget(Storage, Pending, Lease)) return false;
    FSaveManifest StaleEmpty;
    if (!Storage.ReadManifest(Pending.TargetSaveId, StaleEmpty)) return false;
    StaleEmpty.DisplayName = TEXT("Stale rename");
    TestFalse(TEXT("无实际 pending 租约不能发布 gen0 元数据"), Other.WriteManifestAtomic(Pending.TargetSaveId, StaleEmpty));
    if (!PublishTarget(Storage, Pending, Lease)) return false;
    TestFalse(TEXT("即使操作所有者也不能以旧 gen0 回退提交"), Storage.WriteManifestAtomic(Pending.TargetSaveId, StaleEmpty, Lease));
    if (!Storage.CompletePendingWorld(Lease, Error)) return false;
    Lease.Reset();
    TestFalse(TEXT("清理 intent 后迟到的 gen0 写仍被拒绝"), Other.WriteManifestAtomic(Pending.TargetSaveId, StaleEmpty));
    FSaveManifest Current;
    if (!Storage.ReadManifest(Pending.TargetSaveId, Current)) return false;
    FSaveManifest WrongReceipt = Current;
    WrongReceipt.LastCommitOperationId = FGuid::NewGuid();
    TestFalse(TEXT("相同代次不能换成另一个提交回执"), Other.WriteManifestAtomic(Pending.TargetSaveId, WrongReceipt));
    TestTrue(TEXT("目标提交身份与经济文件均保留"), Current.CurrentGeneration == Pending.TargetGeneration &&
        Current.LastCommitOperationId == Pending.OperationId && IFileManager::Get().FileExists(*StoreFixture(Storage, Pending)));
    return true;
}

#if PLATFORM_WINDOWS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSaveBranchNativeOpenFileRetirementTest,
    "WHFramework.SaveGame.Branch.NativeOpenFileRetirement",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSaveBranchNativeOpenFileRetirementTest::RunTest(const FString& InParameters)
{
    SaveBranchTests::FDirectory Directory;
    FSaveGameStorage Storage(0, Directory.Path);
    const FGuid SaveId = FGuid::NewGuid();
    if (!TestTrue(TEXT("建立实际存档槽"), SaveBranchTests::CreateSlot(Storage, SaveId, 1))) return false;
    FString Path = Storage.GetWorldDir(SaveId) / TEXT("NativeSharing.bin");
    const TArray<uint8> Original = {17, 31, 47};
    if (!TestTrue(TEXT("写入受占用保护的数据"), FSaveGameStorage::WriteBinaryDurable(Path, Original))) return false;
    FPaths::MakePlatformFilename(Path);
    HANDLE Handle = ::CreateFileW(*Path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (!TestTrue(TEXT("建立不允许删除共享的真实原生读句柄"), Handle != INVALID_HANDLE_VALUE)) return false;
    ON_SCOPE_EXIT
    {
        if (Handle != INVALID_HANDLE_VALUE) ::CloseHandle(Handle);
    };
    TestFalse(TEXT("持续原生占用不能被有限重试强制退役"), Storage.DeleteWorldDirectory(SaveId));
    TestTrue(TEXT("未成功 rename 时原存档仍可发现"), IFileManager::Get().DirectoryExists(*Storage.GetWorldDir(SaveId)));
    TArray<uint8> Readback;
    TestTrue(TEXT("拒绝退役后数据完整"), Storage.ReadBinary(Path, Readback) && Readback == Original);
    ::CloseHandle(Handle);
    Handle = INVALID_HANDLE_VALUE;
    TestTrue(TEXT("真实句柄释放后同一槽可原子退役"), Storage.DeleteWorldDirectory(SaveId));
    TestFalse(TEXT("成功后才退出可加载目录"), IFileManager::Get().DirectoryExists(*Storage.GetWorldDir(SaveId)));
    return true;
}
#endif

#endif
