#pragma once

#include "CoreMinimal.h"
#include "Common/CommonModuleTypes.h"
#include "Parameter/ParameterTypes.h"
#include "SaveGame/SaveGameStorage.h"
#include <atomic>

// 只在 GT 传给项目参与者；离线复制的 SourceModules 已完整验证并持有源文件代租约。
struct WHFRAMEWORK_API FSaveWorldCaptureContext
{
    FGuid OperationId;
    FGuid SourceSaveId;
    FGuid TargetSaveId;
    FString StorageRoot;
    int32 SourceGeneration = 0;
    int32 TargetGeneration = 1;
    bool bDuplicate = false;
    bool bDeleteWorldOnAbort = false;
    bool bPublicationUncertain = false;
    // 文件已经提交，但原世界未接受普通模块基线；不得以取消回滚，也不得继续采用经济分支。
    bool bProjectionFailed = false;
    FString ProjectionFailure;
};

// worker 只调用原生方法。实现不能持有或回调 UObject，也不能在析构时删除持久数据。
class WHFRAMEWORK_API ISaveWorldExternalStore
{
public:
    virtual ~ISaveWorldExternalStore() = default;
    virtual FGuid GetWorldId() const = 0;
    virtual FGuid GetStoreId() const = 0;
    virtual bool PrepareNative(FString& OutError) = 0;
    virtual bool MarkReadyNative(FString& OutError) = 0;
    virtual void CloseNative() = 0;
};

struct WHFRAMEWORK_API FSaveWorldExternalOperation
{
    FSaveWorldCaptureContext Context;
    TSharedPtr<ISaveWorldExternalStore, ESPMode::ThreadSafe> Store;
    TSharedPtr<FSavePendingWorldLease, ESPMode::ThreadSafe> PendingLease;
    std::atomic_bool bFilesCommitted{false};

    ~FSaveWorldExternalOperation()
    {
        // 先关闭准备句柄，再释放 intent 的跨进程所有权；析构从不回收目录。
        if (Store) Store->CloseNative();
        Store.Reset();
        PendingLease.Reset();
    }
};

using FSaveWorldStoreBegin = TFunction<bool(FSaveWorldCaptureContext&, const TMap<FName, FParameter>&,
    TSharedPtr<ISaveWorldExternalStore, ESPMode::ThreadSafe>&, FString&)>;
using FSaveWorldStoreTransform = TFunction<bool(const FSaveWorldCaptureContext&,
    const TSharedPtr<ISaveWorldExternalStore, ESPMode::ThreadSafe>&, FName, FParameter&, FString&)>;
using FSaveWorldStoreFinish = TFunction<bool(const FSaveWorldCaptureContext&,
    const TSharedPtr<ISaveWorldExternalStore, ESPMode::ThreadSafe>&, bool, FString&)>;
using FSaveWorldStoreDelete = TFunction<bool(const FGuid&, FString&)>;

// 一份中央世界存储对应一个参与者；GT 回调在接受请求时复制，不能在完成时改找新 World。
struct WHFRAMEWORK_API FSaveWorldStoreParticipant
{
    FSaveWorldStoreBegin Begin;
    FSaveWorldStoreTransform Transform;
    FSaveWorldStoreFinish Finish;
    FSaveWorldStoreDelete BeforeDelete;
};
