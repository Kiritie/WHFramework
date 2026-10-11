#include "Ability/Inventory/InventoryStoreLock.h"

#if WITH_DEV_AUTOMATION_TESTS && (PLATFORM_WINDOWS || PLATFORM_LINUX)

#include "Async/Async.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryStoreLockTransferTest,
    "WHFramework.Inventory.Runtime.StoreLockCanonicalPathAndThreadTransfer",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInventoryStoreLockTransferTest::RunTest(const FString& Parameters)
{
    const FString Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() /
        TEXT("Automation/InventoryLock") / FGuid::NewGuid().ToString(EGuidFormats::Digits));
    if (!TestTrue(TEXT("Create an isolated store directory"), IFileManager::Get().MakeDirectory(*Directory, true)))
    {
        return false;
    }
    ON_SCOPE_EXIT { IFileManager::Get().DeleteDirectory(*Directory, false, true); };
    FString Error;
    int64 Handle = FInventoryStoreLock::Acquire(Directory, Error);
    ON_SCOPE_EXIT { FInventoryStoreLock::Release(Handle); };
    if (!TestTrue(TEXT("A worker-independent lease can be acquired"), Handle != -1))
    {
        AddError(Error);
        return false;
    }
    const int64 Alias = FInventoryStoreLock::Acquire(Directory / TEXT("."), Error);
    TestEqual(TEXT("An alternate path cannot open a second owner on the same thread"), Alias, int64(-1));
    FInventoryStoreLock::Release(Alias);
    const bool bWorkerExcluded = Async(EAsyncExecution::Thread, [Directory]()
    {
        FString WorkerError;
        const int64 Other = FInventoryStoreLock::Acquire(Directory, WorkerError);
        FInventoryStoreLock::Release(Other);
        return Other == -1;
    }).Get();
    TestTrue(TEXT("A second thread cannot open the active store"), bWorkerExcluded);
    Async(EAsyncExecution::Thread, [Handle]() { FInventoryStoreLock::Release(Handle); }).Get();
    Handle = -1;
    Handle = FInventoryStoreLock::Acquire(Directory, Error);
    TestTrue(TEXT("A lease closed by another thread can be acquired again"), Handle != -1);
    return true;
}

#endif
