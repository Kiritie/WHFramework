#include "Ability/Inventory/InventoryBinaryArchive.h"
#include "Ability/Inventory/InventoryJournal.h"
#include "Ability/Inventory/InventoryNetworkComponent.h"
#include "Ability/Inventory/InventorySnapshotCodec.h"
#include "Ability/Inventory/InventoryWorldSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Ability/Inventory/AbilityInventoryBase.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/Crc.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NativeGameplayTags.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "UObject/StrongObjectPtr.h"
#include "Voxel/Network/VoxelOverlayReplica.h"
#include "Voxel/Save/InventoryVoxelEntityCodec.h"
#include "Voxel/Save/VoxelBlockEntityCodec.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_InventoryRuntimeTest_Storage, "Inventory.Test.RuntimeStorage");

namespace InventoryRuntimeTests
{
    const FName CounterDomain(TEXT("InventoryTestCounter"));

    struct FTemporaryDirectory
    {
        FString Path = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Automation/Inventory") / FGuid::NewGuid().ToString(EGuidFormats::Digits));

        ~FTemporaryDirectory()
        {
            IFileManager::Get().DeleteDirectory(*Path, false, true);
        }
    };

    struct FData
    {
        FInventoryItemRegistry Registry;
        TArray<FInventoryItemDefinition> Items;
        TMap<FPrimaryAssetId, FInventoryProfileDefinition> Profiles;
        FInventoryProfileDefinition Profile;
        FPrimaryAssetId OreId = FPrimaryAssetId(TEXT("Item:RuntimeTestOre"));
        FPrimaryAssetId SwordId = FPrimaryAssetId(TEXT("Item:RuntimeTestSword"));
        FGuid WorldId = FGuid(901, 902, 903, 904);
        FGuid StoreId = FGuid(905, 906, 907, 908);
        FGuid OwnerId = FGuid(501, 502, 503, 504);
        FString Error;

        bool Initialize(int32 InSlots = 2)
        {
            FInventoryItemDefinition Ore;
            Ore.ItemId = OreId;
            Ore.MaxStack = 10;
            Ore.MaxLevel = 4;
            FInventoryPropertyRule Bound;
            Bound.Key = TEXT("BoundOwner");
            Bound.Type = EInventoryPropertyType::Guid;
            Ore.PropertyRules.Add(Bound);
            FInventoryPropertyRule Tint;
            Tint.Key = TEXT("Tint");
            Tint.Type = EInventoryPropertyType::Name;
            Tint.AllowedNames.Add(TEXT("Blue"));
            Ore.PropertyRules.Add(Tint);
            FInventoryItemDefinition Sword;
            Sword.ItemId = SwordId;
            Sword.StackPolicy = EInventoryStackPolicy::Unique;
            Sword.MaxStack = 1;
            Sword.MaxLevel = 1;
            Items = { Ore, Sword };
            if(!Registry.Initialize(Items, Error))
            {
                return false;
            }
            FInventorySlotGroupDefinition Group;
            Group.Group = TAG_InventoryRuntimeTest_Storage;
            Group.Count = InSlots;
            const TArray<FInventorySlotGroupDefinition> Groups = { Group };
            if(!FInventoryProfileDefinition::Build(FPrimaryAssetId(TEXT("InventoryProfile:RuntimeTest")), Groups, Profile, Error))
            {
                return false;
            }
            Profiles.Add(Profile.ProfileId, Profile);
            return true;
        }

        FInventorySnapshot State(uint32 InIndex) const
        {
            return Profile.CreateState(FGuid(InIndex, 11, 12, 13), WorldId, OwnerId);
        }

        FAbilityItem DecoratedOre(int32 InCount) const
        {
            FAbilityItem Item(OreId, InCount, 2);
            FInventoryItemProperty Bound;
            Bound.Key = TEXT("BoundOwner");
            Bound.Type = EInventoryPropertyType::Guid;
            Bound.GuidValue = OwnerId;
            Item.Properties.Add(Bound);
            FInventoryItemProperty Tint;
            Tint.Key = TEXT("Tint");
            Tint.Type = EInventoryPropertyType::Name;
            Tint.NameValue = TEXT("Blue");
            Item.Properties.Add(Tint);
            return Item;
        }

        FInventoryOperation Add(const FGuid& InInventoryId, int32 InCount) const
        {
            FInventoryOperation Operation;
            Operation.Type = EInventoryOperationType::Add;
            Operation.TargetInventoryId = InInventoryId;
            Operation.Item = FAbilityItem(OreId);
            Operation.Requested = InCount;
            return Operation;
        }
    };

    TArray<uint8> CounterBytes(uint32 InValue)
    {
        FInventoryBinaryWriter Writer(4);
        Writer.U32(InValue);
        return Writer.TakeBytes();
    }

    bool ValidateCounter(TConstArrayView<uint8> InBytes, FString& OutError)
    {
        if(InBytes.Num() != 4)
        {
            OutError = TEXT("Inventory.TestCounterMalformed");
            return false;
        }
        return true;
    }

    FInventoryDomainRedo CounterRedo(const FGuid& InId, uint64 InBefore, uint32 InValue)
    {
        FInventoryDomainRedo Redo;
        Redo.Domain = CounterDomain;
        Redo.EntityId = InId;
        Redo.ExpectedRevision = InBefore;
        Redo.TargetRevision = InBefore + 1;
        Redo.AfterBytes = CounterBytes(InValue);
        return Redo;
    }

    // 故障注入只存在于测试适配器；记录后状态和恢复路径仍执行真实提交协议。
    class FCounterAdapter final : public IInventoryDomainAdapter
    {
    public:
        TMap<FGuid, FInventoryDomainRecord> State;
        FGuid PreparedTransaction;
        TArray<FInventoryDomainRedo> Prepared;
        int32 CancelCount = 0;
        int32 PublishCount = 0;
        int32 RestoreCount = 0;
        bool bRejectPrepare = false;
        bool bRejectApply = false;

        virtual FName GetDomain() const override
        {
            return CounterDomain;
        }

        virtual bool Prepare(const FGuid& InTransactionId, TConstArrayView<FInventoryDomainRedo> InChanges, FString& OutError) override
        {
            if(PreparedTransaction.IsValid() || !InTransactionId.IsValid())
            {
                OutError = TEXT("Inventory.TestAlreadyPrepared");
                return false;
            }
            PreparedTransaction = InTransactionId;
            Prepared.Append(InChanges.GetData(), InChanges.Num());
            if(bRejectPrepare)
            {
                OutError = TEXT("Inventory.TestPrepareFailure");
                return false;
            }
            for(const FInventoryDomainRedo& Redo : Prepared)
            {
                const FInventoryDomainRecord* Before = State.Find(Redo.EntityId);
                if(Redo.Domain != CounterDomain || (Before ? Before->Revision : 0) != Redo.ExpectedRevision ||
                   (!Redo.bDelete && !ValidateCounter(Redo.AfterBytes, OutError)))
                {
                    return false;
                }
            }
            return true;
        }

        virtual bool ValidatePrepared(const FGuid& InTransactionId, FString& OutError) const override
        {
            if(PreparedTransaction != InTransactionId || Prepared.IsEmpty())
            {
                OutError = TEXT("Inventory.TestPreparedIdentity");
                return false;
            }
            return true;
        }

        virtual bool ApplyCommitted(const FGuid& InTransactionId, const FInventoryDomainRedo& InRedo, FString& OutError) override
        {
            if(bRejectApply)
            {
                OutError = TEXT("Inventory.TestApplyFailure");
                return false;
            }
            if(!ValidatePrepared(InTransactionId, OutError))
            {
                return false;
            }
            const FInventoryDomainRecord* Existing = State.Find(InRedo.EntityId);
            if(Existing && Existing->Revision == InRedo.TargetRevision)
            {
                return Existing->Bytes == InRedo.AfterBytes && Existing->bDeleted == InRedo.bDelete;
            }
            if((Existing ? Existing->Revision : 0) != InRedo.ExpectedRevision)
            {
                OutError = TEXT("Inventory.TestApplyRevision");
                return false;
            }
            FInventoryDomainRecord Record;
            Record.Key = { CounterDomain, InRedo.EntityId };
            Record.Revision = InRedo.TargetRevision;
            Record.bDeleted = InRedo.bDelete;
            Record.Bytes = InRedo.AfterBytes;
            State.Add(InRedo.EntityId, MoveTemp(Record));
            return true;
        }

        virtual bool Restore(const FInventoryDomainRecord& InRecord, FString& OutError) override
        {
            if(InRecord.Key.Domain != CounterDomain || (!InRecord.bDeleted && !ValidateCounter(InRecord.Bytes, OutError)))
            {
                return false;
            }
            State.Add(InRecord.Key.EntityId, InRecord);
            PreparedTransaction.Invalidate();
            Prepared.Reset();
            ++RestoreCount;
            return true;
        }

        virtual void Publish(const FGuid& InTransactionId) override
        {
            if(PreparedTransaction == InTransactionId)
            {
                ++PublishCount;
                PreparedTransaction.Invalidate();
                Prepared.Reset();
            }
        }

        virtual void Cancel(const FGuid& InTransactionId) override
        {
            if(PreparedTransaction == InTransactionId)
            {
                ++CancelCount;
                PreparedTransaction.Invalidate();
                Prepared.Reset();
            }
        }
    };

    struct FWorldFixture
    {
        FTemporaryDirectory Directory;
        FData Data;
        UWorld* World = nullptr;
        UInventoryWorldSubsystem* Store = nullptr;
        FString Error;

        ~FWorldFixture()
        {
            Shutdown();
        }

        void Shutdown()
        {
            if(Store)
            {
                Store->CloseStore(false, Error);
                Store = nullptr;
            }
            if(World)
            {
                World->DestroyWorld(false);
                World = nullptr;
            }
        }

        bool Start(bool bInCreateNew, TSharedPtr<IInventoryDomainAdapter> InAdapter = nullptr)
        {
            UWorld::InitializationValues Values;
            Values.AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
            const FName Name(*FString::Printf(TEXT("InventoryTest_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
            // Editor 世界保留 WorldSubsystem，同时隔离项目的 Game/PIE 启动资产配置。
            World = UWorld::CreateWorld(EWorldType::Editor, false, Name, nullptr, true, ERHIFeatureLevel::SM5, &Values);
            Store = World ? World->GetSubsystem<UInventoryWorldSubsystem>() : nullptr;
            const TArray<FInventoryProfileDefinition> Profiles = { Data.Profile };
            if(!Store || !Store->Configure(Data.Items, Profiles, nullptr, Error) ||
               !Store->RegisterRecordDomain(CounterDomain, 4, ValidateCounter, Error))
            {
                return false;
            }
            if(InAdapter && !Store->RegisterDomainAdapter(InAdapter.ToSharedRef(), Error))
            {
                return false;
            }
            return Store->OpenStore(Data.WorldId, Data.StoreId, Directory.Path, bInCreateNew, Error);
        }

        FInventoryCommandResult Execute(const FInventoryBatchRequest& InRequest) const
        {
            return Store->ExecuteAuthorizedBatch(InRequest, Store->MakeServerAuthority(Data.OwnerId, TEXT("Automation")));
        }

        FInventorySnapshot Read(const FGuid& InId)
        {
            FInventorySnapshot Snapshot;
            Store->GetSnapshot(InId, Snapshot, Error);
            return Snapshot;
        }
    };

    void RewriteChecksum(TArray<uint8>& InOutBytes)
    {
        const uint32 Checksum = FCrc::MemCrc32(InOutBytes.GetData(), InOutBytes.Num() - 4);
        for(int32 ByteIndex = 0; ByteIndex < 4; ++ByteIndex)
        {
            InOutBytes[InOutBytes.Num() - 4 + ByteIndex] = static_cast<uint8>(Checksum >> (ByteIndex * 8));
        }
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventorySnapshotIntegrityTest, "WHFramework.Inventory.Runtime.SnapshotIntegrity", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInventorySnapshotIntegrityTest::RunTest(const FString& Parameters)
{
    InventoryRuntimeTests::FData Data;
    if(!TestTrue(TEXT("纯值定义初始化"), Data.Initialize()))
    {
        return false;
    }
    FInventorySnapshot Snapshot = Data.State(1);
    Snapshot.Revision = 19;
    Snapshot.Slots[0].Item = Data.DecoratedOre(7);
    Snapshot.Slots[0].bFavorite = true;
    Snapshot.Slots[1].Item = FAbilityItem(Data.SwordId);
    Snapshot.Slots[1].Item.InstanceId = FGuid(12, 34, 56, 78);
    TArray<uint8> Bytes;
    FString Error;
    TestTrue(TEXT("完整快照编码"), FInventorySnapshotCodec::Encode(Snapshot, Data.Registry, Data.Profiles, Bytes, Error));
    FInventorySnapshot Decoded;
    TestTrue(TEXT("完整快照解码"), FInventorySnapshotCodec::Decode(Bytes, Data.Registry, Data.Profiles, Decoded, Error));
    TestTrue(TEXT("稳定身份、收藏、唯一物品与属性全部往返"), Decoded == Snapshot);
    for(const int32 Cut : { 0, 1, 7, 12, Bytes.Num() - 1 })
    {
        FInventorySnapshot Unchanged = Snapshot;
        TestFalse(TEXT("截断快照拒绝"), FInventorySnapshotCodec::Decode(MakeArrayView(Bytes).Slice(0, Cut), Data.Registry, Data.Profiles, Unchanged, Error));
        TestTrue(TEXT("失败不覆盖已确认副本"), Unchanged == Snapshot);
    }
    TArray<uint8> Corrupt = Bytes;
    Corrupt[16] ^= 0x80;
    TestFalse(TEXT("正文CRC拒绝"), FInventorySnapshotCodec::Decode(Corrupt, Data.Registry, Data.Profiles, Decoded, Error));
    Corrupt = Bytes;
    Corrupt[4] = 1;
    InventoryRuntimeTests::RewriteChecksum(Corrupt);
    TestFalse(TEXT("有效CRC的旧schema也拒绝"), FInventorySnapshotCodec::Decode(Corrupt, Data.Registry, Data.Profiles, Decoded, Error));
    Corrupt.SetNumZeroed(FInventorySnapshotCodec::MaximumBytes + 1);
    TestFalse(TEXT("超限输入在解析前拒绝"), FInventorySnapshotCodec::Decode(Corrupt, Data.Registry, Data.Profiles, Decoded, Error));
    const TArray<FAbilityItem> Items = { Snapshot.Slots[0].Item, Snapshot.Slots[1].Item };
    TArray<FAbilityItem> Restored;
    TestTrue(TEXT("任务物品数组编码"), FInventorySnapshotCodec::EncodeItems(Items, Data.Registry, Bytes, Error));
    TestTrue(TEXT("任务物品数组解码"), FInventorySnapshotCodec::DecodeItems(Bytes, Data.Registry, Restored, Error));
    TestTrue(TEXT("独立物品数组完整往返"), Items == Restored);
    const TArray<FAbilityItem> Duplicate = { Items[1], Items[1] };
    TestFalse(TEXT("编码端也拒绝重复唯一实例"), FInventorySnapshotCodec::EncodeItems(Duplicate, Data.Registry, Bytes, Error));
    return true;
}

#if PLATFORM_WINDOWS || PLATFORM_LINUX

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryJournalRecoveryTest, "WHFramework.Inventory.Runtime.JournalCheckpointRecovery", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInventoryJournalRecoveryTest::RunTest(const FString& Parameters)
{
    InventoryRuntimeTests::FTemporaryDirectory Directory;
    InventoryRuntimeTests::FData Data;
    if(!TestTrue(TEXT("日志定义初始化"), Data.Initialize()))
    {
        return false;
    }
    FInventoryJournal Journal;
    FInventoryCheckpoint Checkpoint;
    TArray<FInventoryJournalRecord> Records;
    FString Error;
    if(!TestTrue(TEXT("显式创建空世界"), Journal.Open(Directory.Path, Data.WorldId, Data.StoreId, true, Checkpoint, Records, Error)))
    {
        AddError(Error);
        return false;
    }
    FInventoryJournal ConcurrentWriter;
    TestFalse(TEXT("同目录第二写入者被独占锁拒绝"), ConcurrentWriter.Open(Directory.Path, Data.WorldId, Data.StoreId, false, Checkpoint, Records, Error));
    FInventorySnapshot Snapshot = Data.State(1);
    Snapshot.Revision = 1;
    Snapshot.Slots[0].Item = FAbilityItem(Data.OreId, 5);
    FInventoryJournalRecord Record;
    Record.WorldId = Data.WorldId;
    Record.TransactionId = FGuid::NewGuid();
    FInventoryJournalChange Change;
    Change.InventoryId = Snapshot.InventoryId;
    Change.bCreated = true;
    Change.AfterRevision = 1;
    TestTrue(TEXT("快照准备"), FInventorySnapshotCodec::Encode(Snapshot, Data.Registry, Data.Profiles, Change.AfterBytes, Error));
    Record.Inventories.Add(Change);
    const FGuid CounterId(87, 86, 85, 84);
    Record.Domains.Add(InventoryRuntimeTests::CounterRedo(CounterId, 0, 1));
    TestTrue(TEXT("WAL同步写入第一笔"), Journal.Append(Record, Error));
    TestEqual(TEXT("全局日志顺序一"), Record.Sequence, uint64(1));
    FInventoryJournalRecord Invalid = Record;
    Invalid.TransactionId = FGuid::NewGuid();
    Invalid.Inventories.Add(Change);
    TestFalse(TEXT("重复参与者在写盘前拒绝"), Journal.Append(Invalid, Error));
    TestEqual(TEXT("拒绝记录不推进水位"), Journal.GetSequence(), uint64(1));
    Snapshot.Revision = 2;
    Snapshot.Slots[0].Item.Count = 3;
    Record.TransactionId = FGuid::NewGuid();
    Record.Inventories[0].bCreated = false;
    Record.Inventories[0].BeforeRevision = 1;
    Record.Inventories[0].AfterRevision = 2;
    TestTrue(TEXT("第二笔后状态编码"), FInventorySnapshotCodec::Encode(Snapshot, Data.Registry, Data.Profiles, Record.Inventories[0].AfterBytes, Error));
    Record.Domains[0] = InventoryRuntimeTests::CounterRedo(CounterId, 1, 2);
    TestTrue(TEXT("库存与domain同笔落日志"), Journal.Append(Record, Error));
    TArray<uint8> DurableTail;
    TestTrue(TEXT("读取持久日志故障样本"), FFileHelper::LoadFileToArray(DurableTail, *(Directory.Path / TEXT("journal.bin"))));
    Journal.Close();
    TestTrue(TEXT("未写运行内存也可恢复完整日志"), Journal.Open(Directory.Path, Data.WorldId, Data.StoreId, false, Checkpoint, Records, Error));
    TestEqual(TEXT("从检查点后重放两笔"), Records.Num(), 2);
    TestEqual(TEXT("恢复顺序完整"), Journal.GetSequence(), uint64(2));
    Checkpoint.WorldId = Data.WorldId;
    Checkpoint.Sequence = 2;
    Checkpoint.Inventories.Add(Snapshot.InventoryId, Record.Inventories[0].AfterBytes);
    FInventoryDomainRecord Counter;
    Counter.Key = { InventoryRuntimeTests::CounterDomain, CounterId };
    Counter.Revision = 2;
    Counter.Bytes = InventoryRuntimeTests::CounterBytes(2);
    Checkpoint.Domains = { Counter };
    TestTrue(TEXT("全域检查点与manifest提交"), Journal.Checkpoint(Checkpoint, Error));
    Journal.Close();
    TestTrue(TEXT("模拟manifest已换代但旧WAL尚未清理"), FFileHelper::SaveArrayToFile(DurableTail, *(Directory.Path / TEXT("journal.bin"))));
    TestTrue(TEXT("旧日志按检查点水位跳过"), Journal.Open(Directory.Path, Data.WorldId, Data.StoreId, false, Checkpoint, Records, Error));
    TestTrue(TEXT("检查点前记录不重复重放"), Records.IsEmpty());
    TestEqual(TEXT("domain检查点保留版本"), Checkpoint.Domains[0].Revision, uint64(2));
    FInventorySnapshot Recovered;
    TestTrue(TEXT("检查点库存可恢复"), FInventorySnapshotCodec::Decode(Checkpoint.Inventories.FindChecked(Snapshot.InventoryId), Data.Registry, Data.Profiles, Recovered, Error));
    TestTrue(TEXT("检查点所有字段一致"), Recovered == Snapshot);
    Journal.Close();
    TestFalse(TEXT("其他世界不能借用同一manifest"), Journal.Open(Directory.Path, FGuid::NewGuid(), Data.StoreId, false, Checkpoint, Records, Error));
    TestFalse(TEXT("已有存档不能当作新世界覆盖"), Journal.Open(Directory.Path, Data.WorldId, Data.StoreId, true, Checkpoint, Records, Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryJournalTailTest, "WHFramework.Inventory.Runtime.TornTailAndCorruption", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInventoryJournalTailTest::RunTest(const FString& Parameters)
{
    InventoryRuntimeTests::FTemporaryDirectory Directory;
    const FGuid WorldId = FGuid::NewGuid();
    const FGuid StoreId = FGuid::NewGuid();
    FInventoryJournal Journal;
    FInventoryCheckpoint Checkpoint;
    TArray<FInventoryJournalRecord> Records;
    FString Error;
    if(!TestTrue(TEXT("故障样本存档创建"), Journal.Open(Directory.Path, WorldId, StoreId, true, Checkpoint, Records, Error)))
    {
        return false;
    }
    FInventoryJournalRecord Record;
    Record.WorldId = WorldId;
    Record.TransactionId = FGuid::NewGuid();
    Record.Domains.Add(InventoryRuntimeTests::CounterRedo(FGuid::NewGuid(), 0, 1));
    TestTrue(TEXT("完整事务先持久化"), Journal.Append(Record, Error));
    Journal.Close();
    const FString Path = Directory.Path / TEXT("journal.bin");
    TArray<uint8> Complete;
    TestTrue(TEXT("读取完整帧"), FFileHelper::LoadFileToArray(Complete, *Path));
    TArray<uint8> Torn = Complete;
    const TArray<uint8> PartialHeader = { 0x49, 0x57, 0x4a, 0x32, 2, 0, 0 };
    Torn.Append(PartialHeader);
    TestTrue(TEXT("写入未完成尾帧"), FFileHelper::SaveArrayToFile(Torn, *Path));
    TestTrue(TEXT("截断尾帧可确定性修复"), Journal.Open(Directory.Path, WorldId, StoreId, false, Checkpoint, Records, Error));
    TestEqual(TEXT("只恢复完整事务"), Records.Num(), 1);
    Journal.Close();
    TArray<uint8> Repaired;
    TestTrue(TEXT("读取修复后日志"), FFileHelper::LoadFileToArray(Repaired, *Path));
    TestTrue(TEXT("尾部修复不改完整记录"), Repaired == Complete);
    TArray<uint8> Corrupt = Complete;
    Corrupt[20] ^= 0x40;
    TestTrue(TEXT("写入完整帧正文损坏"), FFileHelper::SaveArrayToFile(Corrupt, *Path));
    TestFalse(TEXT("完整帧CRC错误禁止静默截尾"), Journal.Open(Directory.Path, WorldId, StoreId, false, Checkpoint, Records, Error));
    TestFalse(TEXT("损坏存档不能继续提交"), Journal.IsHealthy());
    Corrupt = Complete;
    Corrupt[4] = 1;
    TestTrue(TEXT("写入旧日志schema"), FFileHelper::SaveArrayToFile(Corrupt, *Path));
    TestFalse(TEXT("旧schema显式拒绝"), Journal.Open(Directory.Path, WorldId, StoreId, false, Checkpoint, Records, Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryWorldAtomicTest, "WHFramework.Inventory.Runtime.WorldAtomicityAndLifecycle", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInventoryWorldAtomicTest::RunTest(const FString& Parameters)
{
    InventoryRuntimeTests::FWorldFixture Fixture;
    if(!TestTrue(TEXT("运行世界定义初始化"), Fixture.Data.Initialize()) || !TestTrue(TEXT("运行世界存储打开"), Fixture.Start(true)))
    {
        AddError(Fixture.Error);
        return false;
    }
    const FInventorySnapshot Source = Fixture.Data.State(1);
    const FInventorySnapshot Target = Fixture.Data.State(2);
    FInventoryBatchRequest Create;
    Create.CreateInventories = { Source, Target };
    Create.Operations = { Fixture.Data.Add(Source.InventoryId, 10), Fixture.Data.Add(Target.InventoryId, 15) };
    TestTrue(TEXT("两个容器同笔创建和初始化"), Fixture.Execute(Create).Code == EInventoryResultCode::Success);
    const FInventorySnapshot BeforeSource = Fixture.Read(Source.InventoryId);
    const FInventorySnapshot BeforeTarget = Fixture.Read(Target.InventoryId);
    FInventoryOperation Transfer;
    Transfer.Type = EInventoryOperationType::Transfer;
    Transfer.SourceInventoryId = Source.InventoryId;
    Transfer.TargetInventoryId = Target.InventoryId;
    Transfer.Filter.ItemId = Fixture.Data.OreId;
    Transfer.Requested = 6;
    FInventoryBatchRequest Request;
    Request.Operations = { Transfer };
    TestTrue(TEXT("Exact空间不足整笔拒绝"), Fixture.Execute(Request).Code == EInventoryResultCode::NoCapacity);
    TestTrue(TEXT("源和目标均未改变"), Fixture.Read(Source.InventoryId) == BeforeSource && Fixture.Read(Target.InventoryId) == BeforeTarget);
    int32 PublishCount = 0;
    const FDelegateHandle Published = Fixture.Store->OnCommitted.AddLambda([&PublishCount](const FInventoryChangeSet&)
    {
        ++PublishCount;
    });
    Request.Operations[0].AmountMode = EInventoryAmountMode::UpTo;
    const FInventoryCommandResult Partial = Fixture.Execute(Request);
    TestTrue(TEXT("部分转移只接受可放数量"), Partial.Code == EInventoryResultCode::Partial);
    TestEqual(TEXT("实际转移五个"), Partial.AcceptedCount, int64(5));
    TestEqual(TEXT("只发布一次完整事务"), PublishCount, 1);
    TestEqual(TEXT("源容器版本加一"), Fixture.Read(Source.InventoryId).Revision, BeforeSource.Revision + 1);
    TestEqual(TEXT("目标容器版本加一"), Fixture.Read(Target.InventoryId).Revision, BeforeTarget.Revision + 1);
    TestTrue(TEXT("借用租约阻止退休"), Fixture.Store->AcquireLease(Source.InventoryId));
    TestFalse(TEXT("有租约时退休失败"), Fixture.Store->Retire(Source.InventoryId, Fixture.Error));
    Fixture.Store->ReleaseLease(Source.InventoryId);
    TestTrue(TEXT("释放租约后可退休"), Fixture.Store->Retire(Source.InventoryId, Fixture.Error));
    TestNotNull(TEXT("冷容器可重新解析"), Fixture.Store->Resolve(Source.InventoryId));
    const FInventorySnapshot UniqueOwner = Fixture.Data.State(3);
    FInventoryOperation AddUnique;
    AddUnique.Type = EInventoryOperationType::Add;
    AddUnique.TargetInventoryId = UniqueOwner.InventoryId;
    AddUnique.Item = FAbilityItem(Fixture.Data.SwordId);
    AddUnique.Item.InstanceId = FGuid(31, 32, 33, 34);
    AddUnique.Requested = 1;
    FInventoryBatchRequest Unique;
    Unique.CreateInventories = { UniqueOwner };
    Unique.Operations = { AddUnique };
    TestTrue(TEXT("唯一物品首次注册成功"), Fixture.Execute(Unique).Code == EInventoryResultCode::Success);
    TestTrue(TEXT("持有唯一物品的库存退休为冷状态"), Fixture.Store->Retire(UniqueOwner.InventoryId, Fixture.Error));
    const FInventorySnapshot DuplicateOwner = Fixture.Data.State(4);
    Unique.CreateInventories = { DuplicateOwner };
    Unique.Operations[0].TargetInventoryId = DuplicateOwner.InventoryId;
    TestTrue(TEXT("全局索引覆盖冷容器，重复InstanceId拒绝"), Fixture.Execute(Unique).Code == EInventoryResultCode::DuplicateInstance);
    TestNull(TEXT("失败的新容器没有注册"), Fixture.Store->Resolve(DuplicateOwner.InventoryId));
    FInventoryBatchRequest Delete;
    FInventoryOperation MoveUnique;
    MoveUnique.Type = EInventoryOperationType::Transfer;
    MoveUnique.SourceInventoryId = UniqueOwner.InventoryId;
    MoveUnique.TargetInventoryId = Source.InventoryId;
    MoveUnique.Filter.bMatchSignature = true;
    MoveUnique.Filter.Signature = AddUnique.Item.Signature();
    MoveUnique.Requested = 1;
    Delete.Operations = { MoveUnique };
    Delete.DeleteInventories = { UniqueOwner.InventoryId };
    TestTrue(TEXT("转移并删除空容器同笔提交"), Fixture.Execute(Delete).Code == EInventoryResultCode::Success);
    TestNull(TEXT("删除容器不可解析"), Fixture.Store->Resolve(UniqueOwner.InventoryId));
    FInventoryBatchRequest Recreate;
    Recreate.CreateInventories = { UniqueOwner };
    TestFalse(TEXT("墓碑身份不能被重复创建"), Fixture.Execute(Recreate).Code == EInventoryResultCode::Success);
    const FInventorySnapshot FinalSource = Fixture.Read(Source.InventoryId);
    Fixture.Store->OnCommitted.Remove(Published);
    TestTrue(TEXT("全世界检查点成功"), Fixture.Store->SaveCheckpoint(Fixture.Error));
    Fixture.Shutdown();
    TestTrue(TEXT("新UWorld从检查点恢复"), Fixture.Start(false));
    TestTrue(TEXT("热冷切换及重启保持唯一完整状态"), Fixture.Read(Source.InventoryId) == FinalSource);
    TestFalse(TEXT("恢复后墓碑仍拒绝重建"), Fixture.Execute(Recreate).Code == EInventoryResultCode::Success);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryObservedParticipantTest, "WHFramework.Inventory.Runtime.ObservedDomainParticipant", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInventoryObservedParticipantTest::RunTest(const FString& Parameters)
{
    InventoryRuntimeTests::FWorldFixture Fixture;
    if(!TestTrue(TEXT("只读依赖夹具初始化"), Fixture.Data.Initialize()) || !TestTrue(TEXT("只读依赖存档打开"), Fixture.Start(true))) return false;
    const FInventorySnapshot Initial = Fixture.Data.State(61);
    FInventoryBatchRequest Create;
    Create.CreateInventories.Add(Initial);
    Create.Operations.Add(Fixture.Data.Add(Initial.InventoryId, 2));
    if(!TestTrue(TEXT("创建被观察的库存"), Fixture.Execute(Create).IsSuccess())) return false;
    const FInventorySnapshot Before = Fixture.Read(Initial.InventoryId);
    const FGuid CounterId(811, 812, 813, 814);
    FInventoryBatchRequest DomainOnly;
    DomainOnly.ExpectedRevisions.Add({Before.InventoryId, Before.Revision, Before.Generation});
    DomainOnly.DomainChanges.Add(InventoryRuntimeTests::CounterRedo(CounterId, 0, 1));
    const FInventoryCommandResult Committed = Fixture.Execute(DomainOnly);
    TestTrue(TEXT("只有Observe和领域写入的事务可提交"), Committed.IsSuccess());
    TestTrue(TEXT("被观察库存不产生假版本或假变更"), Committed.FinalRevisions.IsEmpty() && Fixture.Read(Initial.InventoryId) == Before);
    const FInventoryDomainRecord* Record = Fixture.Store->GetRecords().Find(InventoryRuntimeTests::CounterDomain, CounterId);
    if(!TestNotNull(TEXT("领域结果实际落中央记录"), Record)) return false;
    TestEqual(TEXT("领域版本推进一次"), Record->Revision, uint64(1));
    FInventoryBatchRequest Mutate;
    Mutate.Operations.Add(Fixture.Data.Add(Initial.InventoryId, 1));
    if(!TestTrue(TEXT("观察后另一个库存事务改变版本"), Fixture.Execute(Mutate).IsSuccess())) return false;
    DomainOnly.DomainChanges[0] = InventoryRuntimeTests::CounterRedo(CounterId, 1, 2);
    TestTrue(TEXT("只读依赖的过期版本会拒绝领域写入"), Fixture.Execute(DomainOnly).Code == EInventoryResultCode::RevisionStale);
    TestEqual(TEXT("拒绝后领域版本保持"), Fixture.Store->GetRecords().Find(InventoryRuntimeTests::CounterDomain, CounterId)->Revision, uint64(1));
    DomainOnly.ExpectedRevisions[0].Revision = Fixture.Read(Initial.InventoryId).Revision;
    DomainOnly.ExpectedRevisions[0].Generation += 1;
    TestTrue(TEXT("只读依赖同样检查代数"), Fixture.Execute(DomainOnly).Code == EInventoryResultCode::GenerationChanged);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryLargeServerBatchTest, "WHFramework.Inventory.Runtime.LargeAuthorizedBatch", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInventoryLargeServerBatchTest::RunTest(const FString& Parameters)
{
    InventoryRuntimeTests::FWorldFixture Fixture;
    if(!TestTrue(TEXT("大目录布局初始化"), Fixture.Data.Initialize(320)) || !TestTrue(TEXT("大批次存档打开"), Fixture.Start(true))) return false;
    const FInventorySnapshot Initial = Fixture.Data.State(51);
    FInventoryBatchRequest Batch;
    Batch.CreateInventories.Add(Initial);
    for(int32 Index = 0; Index < 300; ++Index) Batch.Operations.Add(Fixture.Data.Add(Initial.InventoryId, 1));
    int32 Published = 0;
    const FDelegateHandle Handle = Fixture.Store->OnCommitted.AddLambda([&Published](const FInventoryChangeSet&) { ++Published; });
    const FInventoryCommandResult Result = Fixture.Execute(Batch);
    TestTrue(TEXT("服务器初始化可原子容纳超过256条物品操作"), Result.IsSuccess());
    TestEqual(TEXT("整个初始化只发布一次"), Published, 1);
    const FInventorySnapshot Final = Fixture.Read(Initial.InventoryId);
    int64 Count = 0;
    for(const FInventorySlotState& Slot : Final.Slots) Count += Slot.Item.Count;
    TestEqual(TEXT("完整批次无截断地写入300个物品"), Count, int64(300));
    TestEqual(TEXT("容器版本只递增一次"), Final.Revision, uint64(1));
    Batch.CreateInventories.Reset();
    Batch.Operations.Init(Fixture.Data.Add(Initial.InventoryId, 1), 4097);
    TestTrue(TEXT("服务器操作预算仍为有界4096"), Fixture.Execute(Batch).Code == EInventoryResultCode::InvalidRequest);
    TestTrue(TEXT("超预算拒绝没有部分写入"), Fixture.Read(Initial.InventoryId) == Final);
    Fixture.Store->OnCommitted.Remove(Handle);
    Fixture.Shutdown();
    TestTrue(TEXT("大批次从真实WAL恢复"), Fixture.Start(false));
    TestTrue(TEXT("恢复完整最终快照"), Fixture.Read(Initial.InventoryId) == Final);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryCommitFailureTest, "WHFramework.Inventory.Runtime.BeforeAndAfterDurablePoint", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInventoryCommitFailureTest::RunTest(const FString& Parameters)
{
    InventoryRuntimeTests::FWorldFixture Fixture;
    const TSharedRef<InventoryRuntimeTests::FCounterAdapter> Adapter = MakeShared<InventoryRuntimeTests::FCounterAdapter>();
    if(!TestTrue(TEXT("故障注入定义初始化"), Fixture.Data.Initialize()) || !TestTrue(TEXT("故障注入世界启动"), Fixture.Start(true, Adapter)))
    {
        AddError(Fixture.Error);
        return false;
    }
    const FInventorySnapshot Original = Fixture.Data.State(1);
    FInventoryBatchRequest Create;
    Create.CreateInventories = { Original };
    Create.Operations = { Fixture.Data.Add(Original.InventoryId, 8) };
    TestTrue(TEXT("初始资产创建"), Fixture.Execute(Create).Code == EInventoryResultCode::Success);
    const FInventorySnapshot Before = Fixture.Read(Original.InventoryId);
    FInventoryOperation Consume;
    Consume.Type = EInventoryOperationType::Remove;
    Consume.SourceInventoryId = Original.InventoryId;
    Consume.Filter.ItemId = Fixture.Data.OreId;
    Consume.Requested = 1;
    const FGuid CounterId(91, 92, 93, 94);
    FInventoryBatchRequest Request;
    Request.Operations = { Consume };
    Request.DomainChanges.Add(InventoryRuntimeTests::CounterRedo(CounterId, 0, 1));
    Adapter->bRejectPrepare = true;
    TestTrue(TEXT("Prepare失败有明确返回"), Fixture.Execute(Request).Code == EInventoryResultCode::SourcePreparationFailed);
    TestEqual(TEXT("失败准备也会Cancel释放"), Adapter->CancelCount, 1);
    TestTrue(TEXT("持久点前源库存逐字段不变"), Fixture.Read(Original.InventoryId) == Before);
    TestNull(TEXT("持久点前domain无记录"), Fixture.Store->GetRecords().Find(InventoryRuntimeTests::CounterDomain, CounterId));
    Adapter->bRejectPrepare = false;
    Adapter->bRejectApply = true;
    AddExpectedError(TEXT("Inventory world quarantined: Inventory.TestApplyFailure"), EAutomationExpectedErrorFlags::Contains, 1);
    const FInventoryCommandResult Pending = Fixture.Execute(Request);
    TestTrue(TEXT("持久点后失败不返回可重做的普通失败"), Pending.Code == EInventoryResultCode::ConfirmationPending);
    TestTrue(TEXT("持久点后失败隔离世界"), Fixture.Store->IsQuarantined() && !Fixture.Store->IsReady());
    const FInventoryDomainRecord* Durable = Fixture.Store->GetRecords().Find(InventoryRuntimeTests::CounterDomain, CounterId);
    TestTrue(TEXT("隔离保留已提交记录"), Durable && Durable->Revision == 1 && Durable->Bytes == InventoryRuntimeTests::CounterBytes(1));
    TestEqual(TEXT("失败后没有对外发布成功事件"), Adapter->PublishCount, 0);
    Fixture.Shutdown();
    const TSharedRef<InventoryRuntimeTests::FCounterAdapter> RestoredAdapter = MakeShared<InventoryRuntimeTests::FCounterAdapter>();
    if(!TestTrue(TEXT("崩溃恢复用完整domain记录重建投影"), Fixture.Start(false, RestoredAdapter)))
    {
        AddError(Fixture.Error);
        return false;
    }
    const FInventorySnapshot After = Fixture.Read(Original.InventoryId);
    TestEqual(TEXT("恢复只消费一次"), After.Slots[0].Item.Count, 7);
    TestEqual(TEXT("恢复只增加一版"), After.Revision, Before.Revision + 1);
    TestEqual(TEXT("重建一次投影"), RestoredAdapter->RestoreCount, 1);
    TestEqual(TEXT("恢复不重放发布回调"), RestoredAdapter->PublishCount, 0);
    TestTrue(TEXT("同一domain版本的重复业务请求拒绝"), Fixture.Execute(Request).Code == EInventoryResultCode::RevisionStale);
    TestTrue(TEXT("重复请求不再次消费"), Fixture.Read(Original.InventoryId) == After);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryArchiveForkTest, "WHFramework.Inventory.Runtime.ArchiveLatestCutAndIndependentFork", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInventoryArchiveForkTest::RunTest(const FString& Parameters)
{
    InventoryRuntimeTests::FTemporaryDirectory TargetDirectory;
    InventoryRuntimeTests::FWorldFixture Source;
    const auto Adapter = MakeShared<InventoryRuntimeTests::FCounterAdapter>();
    if(!TestTrue(TEXT("分支定义初始化"), Source.Data.Initialize(3)) || !TestTrue(TEXT("源分支持久存储打开"), Source.Start(true, Adapter))) return false;
    const FInventorySnapshot Initial = Source.Data.State(101);
    const FInventorySnapshot Retired = Source.Data.State(102);
    const FGuid CounterId(110, 111, 112, 113);
    const FGuid DeletedCounterId(114, 115, 116, 117);
    FInventoryOperation Ore = Source.Data.Add(Initial.InventoryId, 5);
    Ore.Item = Source.Data.DecoratedOre(1);
    FInventoryOperation Unique;
    Unique.Type = EInventoryOperationType::Add;
    Unique.TargetInventoryId = Initial.InventoryId;
    Unique.Item = FAbilityItem(Source.Data.SwordId);
    Unique.Item.InstanceId = FGuid(121, 122, 123, 124);
    Unique.Requested = 1;
    FInventoryBatchRequest Create;
    Create.CreateInventories = {Initial, Retired};
    Create.Operations = {Ore, Unique};
    Create.DomainChanges = {InventoryRuntimeTests::CounterRedo(CounterId, 0, 1), InventoryRuntimeTests::CounterRedo(DeletedCounterId, 0, 1)};
    if(!TestTrue(TEXT("源分支同笔初始化资产和领域记录"), Source.Execute(Create).IsSuccess()) ||
       !TestTrue(TEXT("普通投影边界之前的经济检查点"), Source.Store->SaveCheckpoint(Source.Error))) return false;
    FInventoryOperation Remove;
    Remove.Type = EInventoryOperationType::Remove;
    Remove.SourceInventoryId = Initial.InventoryId;
    Remove.Filter.bMatchSignature = true;
    Remove.Filter.Signature = Ore.Item.Signature();
    Remove.Requested = 1;
    FInventoryDomainRedo Deleted = InventoryRuntimeTests::CounterRedo(DeletedCounterId, 1, 0);
    Deleted.bDelete = true;
    Deleted.AfterBytes.Reset();
    FInventoryBatchRequest Advance;
    Advance.Operations = {Remove};
    Advance.DeleteInventories = {Retired.InventoryId};
    Advance.DomainChanges = {InventoryRuntimeTests::CounterRedo(CounterId, 1, 2), Deleted};
    if(!TestTrue(TEXT("普通边界之后的完整经济事务仍进入复制边界"), Source.Execute(Advance).IsSuccess())) return false;
    const FInventorySnapshot Latest = Source.Read(Initial.InventoryId);
    FInventoryArchiveSnapshot Captured;
    if(!TestTrue(TEXT("在线捕获全域一致后状态"), Source.Store->CaptureArchiveSnapshot(Captured, Source.Error))) return false;
    TestTrue(TEXT("捕获携带逻辑世界、存储身份与最新序号"), Captured.WorldId == Source.Data.WorldId && Captured.StoreId == Source.Data.StoreId && Captured.Sequence == 2);
    FInventoryArchiveSnapshot BusyOutput = Captured;
    TestFalse(TEXT("离线导出不能偷读另一个仍持写锁的源"), FInventoryJournal::ExportLatest(Source.Directory.Path, Source.Data.WorldId, Source.Data.StoreId, BusyOutput, Source.Error));
    TestTrue(TEXT("Busy失败保留调用者原输出"), BusyOutput.CheckpointBytes == Captured.CheckpointBytes && Source.Store->IsReady());
    Source.Shutdown();
    const int32 RestoreCountBeforeExport = Adapter->RestoreCount;
    const int32 PublishCountBeforeExport = Adapter->PublishCount;
    TArray<uint8> Complete;
    const FString JournalPath = Source.Directory.Path / TEXT("journal.bin");
    if(!TestTrue(TEXT("读取完整源日志"), FFileHelper::LoadFileToArray(Complete, *JournalPath))) return false;
    TArray<uint8> Torn = Complete;
    Torn.Append(TArray<uint8>{0x49, 0x57, 0x4a, 0x32, 2, 0, 0});
    if(!TestTrue(TEXT("模拟源最后一帧尚未写完"), FFileHelper::SaveArrayToFile(Torn, *JournalPath))) return false;
    FInventoryArchiveSnapshot Offline;
    if(!TestTrue(TEXT("未打开源UWorld也能折叠最新完整WAL"), FInventoryJournal::ExportLatest(Source.Directory.Path, Source.Data.WorldId, Source.Data.StoreId, Offline, Source.Error))) return false;
    TestTrue(TEXT("在线和离线导出得到同一完整边界"), Offline.Sequence == Captured.Sequence && Offline.CheckpointBytes == Captured.CheckpointBytes);
    TestTrue(TEXT("离线折叠不执行领域恢复或发布"), Adapter->RestoreCount == RestoreCountBeforeExport && Adapter->PublishCount == PublishCountBeforeExport);
    TArray<uint8> AfterExport;
    TestTrue(TEXT("离线导出不修复或改写源尾部"), FFileHelper::LoadFileToArray(AfterExport, *JournalPath) && AfterExport == Torn);
    TArray<uint8> Encoded;
    FInventoryArchiveSnapshot RoundTrip;
    if(!TestTrue(TEXT("独立经济快照有界编码"), Offline.Encode(Encoded, Source.Error)) ||
       !TestTrue(TEXT("独立经济快照有界解码"), FInventoryArchiveSnapshot::Decode(Encoded, RoundTrip, Source.Error))) return false;
    TestTrue(TEXT("独立快照全部身份和正文往返"), RoundTrip.WorldId == Offline.WorldId && RoundTrip.StoreId == Offline.StoreId &&
        RoundTrip.Sequence == Offline.Sequence && RoundTrip.CheckpointBytes == Offline.CheckpointBytes);
    TArray<uint8> Corrupt = Encoded;
    Corrupt[24] ^= 1;
    TestFalse(TEXT("快照身份字段也在CRC保护内"), FInventoryArchiveSnapshot::Decode(Corrupt, RoundTrip, Source.Error));
    TestTrue(TEXT("无效快照不覆盖已解码边界"), RoundTrip.StoreId == Offline.StoreId && RoundTrip.CheckpointBytes == Offline.CheckpointBytes);
    const FGuid TargetStoreId = FGuid::NewGuid();
    const FGuid OperationId = FGuid::NewGuid();
    TSharedPtr<FInventoryPreparedStore, ESPMode::ThreadSafe> Prepared;
    if(!TestTrue(TEXT("独立目录准备完整经济后状态"), FInventoryJournal::PrepareFork(Offline, TargetDirectory.Path, TargetStoreId, OperationId, Prepared, Source.Error))) return false;
    TestTrue(TEXT("准备阶段目标日志已打开且保持原序号"), Prepared->IsReady() && !Prepared->IsCommitted() && Prepared->GetSequence() == 2 &&
        Prepared->GetWorldId() == Source.Data.WorldId && Prepared->GetSourceStoreId() == Source.Data.StoreId && Prepared->GetStoreId() == TargetStoreId);
    TSharedPtr<FInventoryPreparedStore, ESPMode::ThreadSafe> Refused;
    TestFalse(TEXT("重试不能覆盖已有独立目标目录"), FInventoryJournal::PrepareFork(Offline, TargetDirectory.Path, FGuid::NewGuid(), FGuid::NewGuid(), Refused, Source.Error));
    TestTrue(TEXT("拒绝覆盖保留原准备对象可用"), !Refused && Prepared->IsReady());
    FInventoryJournal Target;
    FInventoryCheckpoint Checkpoint;
    TArray<FInventoryJournalRecord> Records;
    TestFalse(TEXT("准备句柄阻止其他目标写入者"), Target.Open(TargetDirectory.Path, Source.Data.WorldId, TargetStoreId, false, Checkpoint, Records, Source.Error));
    Prepared->Close();
    TestFalse(TEXT("有owner但未ready的经济目录不能普通打开"), Target.Open(TargetDirectory.Path, Source.Data.WorldId, TargetStoreId, false, Checkpoint, Records, Source.Error));
    TestFalse(TEXT("另一操作不能完成目标经济准备"), FInventoryJournal::MarkPreparedCommitted(TargetDirectory.Path, Source.Data.WorldId, TargetStoreId, FGuid::NewGuid(), Source.Error));
    if(!TestTrue(TEXT("原操作可在进程恢复路径完成内部ready"), FInventoryJournal::MarkPreparedCommitted(TargetDirectory.Path, Source.Data.WorldId, TargetStoreId, OperationId, Source.Error)) ||
       !TestTrue(TEXT("内部ready的经济目录可独立打开"), Target.Open(TargetDirectory.Path, Source.Data.WorldId, TargetStoreId, false, Checkpoint, Records, Source.Error))) return false;
    TestTrue(TEXT("目标从完整检查点开始，没有源的裸WAL副本"), Records.IsEmpty() && Checkpoint.Sequence == 2);
    FInventorySnapshot TargetInventory;
    if(!TestTrue(TEXT("目标库存可按原逻辑身份解码"), FInventorySnapshotCodec::Decode(Checkpoint.Inventories.FindRef(Initial.InventoryId), Source.Data.Registry, Source.Data.Profiles, TargetInventory, Source.Error))) return false;
    TestTrue(TEXT("库存ID、owner、唯一实例、typed属性、数量和版本完整保持"), TargetInventory == Latest);
    const FInventoryTombstone* Tombstone = Checkpoint.Tombstones.Find(Retired.InventoryId);
    TestTrue(TEXT("删除库存墓碑同样复制"), Tombstone && Tombstone->Generation == Retired.Generation && Tombstone->Revision == 2);
    const FInventoryDomainRecord* Counter = Checkpoint.Domains.FindByPredicate([&CounterId](const FInventoryDomainRecord& Entry) { return Entry.Key.EntityId == CounterId; });
    const FInventoryDomainRecord* DeletedRecord = Checkpoint.Domains.FindByPredicate([&DeletedCounterId](const FInventoryDomainRecord& Entry) { return Entry.Key.EntityId == DeletedCounterId; });
    TestTrue(TEXT("领域最终记录与删除版本一同复制"), Counter && Counter->Revision == 2 && Counter->Bytes == InventoryRuntimeTests::CounterBytes(2) &&
        DeletedRecord && DeletedRecord->bDeleted && DeletedRecord->Revision == 2 && DeletedRecord->Bytes.IsEmpty());
    FInventoryJournalRecord BranchWrite;
    BranchWrite.WorldId = Source.Data.WorldId;
    BranchWrite.TransactionId = FGuid::NewGuid();
    BranchWrite.Domains = {InventoryRuntimeTests::CounterRedo(CounterId, 2, 99)};
    TestTrue(TEXT("目标独立追加自己的WAL"), Target.Append(BranchWrite, Source.Error) && BranchWrite.Sequence == 3);
    Target.Close();
    TestFalse(TEXT("保留WorldId不能借另一个StoreId打开目标"), Target.Open(TargetDirectory.Path, Source.Data.WorldId, Source.Data.StoreId, false, Checkpoint, Records, Source.Error));
    TestFalse(TEXT("内部ready目标不由无发布证明的通用取消器删除"), FInventoryJournal::DiscardPrepared(TargetDirectory.Path, Source.Data.WorldId, TargetStoreId, OperationId, Source.Error));
    if(!TestTrue(TEXT("复制之后源仍可按原身份恢复和使用"), Source.Start(false))) return false;
    TestTrue(TEXT("目标追加没有改变源库存"), Source.Read(Initial.InventoryId) == Latest);
    const FInventoryDomainRecord* SourceCounter = Source.Store->GetRecords().Find(InventoryRuntimeTests::CounterDomain, CounterId);
    TestTrue(TEXT("目标计数99没有污染源计数2"), SourceCounter && SourceCounter->Revision == 2 && SourceCounter->Bytes == InventoryRuntimeTests::CounterBytes(2));
    FInventoryBatchRequest SourceWrite;
    SourceWrite.Operations = {Source.Data.Add(Initial.InventoryId, 1)};
    TestTrue(TEXT("复制失败与成功都不关闭源后续写入能力"), Source.Execute(SourceWrite).IsSuccess());
    if(!TestTrue(TEXT("源继续写入时目标仍可独立重开"), Target.Open(TargetDirectory.Path, Source.Data.WorldId, TargetStoreId, false, Checkpoint, Records, Source.Error))) return false;
    TestTrue(TEXT("目标只拥有自己的后续记录"), Records.Num() == 1 && Records[0].Inventories.IsEmpty() &&
        Records[0].Domains.Num() == 1 && Records[0].Domains[0].AfterBytes == InventoryRuntimeTests::CounterBytes(99));
    Target.Close();
    Source.Shutdown();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryUnboundStoreCleanupTest, "WHFramework.Inventory.Runtime.UnboundStoreCleanup", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInventoryUnboundStoreCleanupTest::RunTest(const FString& Parameters)
{
    const FGuid WorldId = FGuid::NewGuid();
    const FGuid StoreId = FGuid::NewGuid();
    InventoryRuntimeTests::FTemporaryDirectory Directory;
    Directory.Path = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Inventory"), TEXT("Unbound"), StoreId.ToString(EGuidFormats::Digits)));
    FInventoryJournal Journal;
    FInventoryCheckpoint Checkpoint;
    TArray<FInventoryJournalRecord> Records;
    FString Error;
    if(!TestTrue(TEXT("首存之前的独立Unbound存储"), Journal.Open(Directory.Path, WorldId, StoreId, true, Checkpoint, Records, Error))) return false;
    TestFalse(TEXT("仍持有源日志时禁止清理"), FInventoryJournal::DiscardUnbound(Directory.Path, WorldId, StoreId, Error));
    TestTrue(TEXT("清理失败保持活动源可用"), Journal.IsHealthy());
    Journal.Close();
    TestFalse(TEXT("清理必须精确匹配逻辑身份"), FInventoryJournal::DiscardUnbound(Directory.Path, FGuid::NewGuid(), StoreId, Error));
    TestFalse(TEXT("不能把普通槽或任意目录当Unbound删除"), FInventoryJournal::DiscardUnbound(FPaths::GetPath(Directory.Path), WorldId, StoreId, Error));
    TestTrue(TEXT("采用完成释放句柄后可退休原Unbound目录"), FInventoryJournal::DiscardUnbound(Directory.Path, WorldId, StoreId, Error));
    TestFalse(TEXT("原Unbound目录已清理"), IFileManager::Get().DirectoryExists(*Directory.Path));
    TestTrue(TEXT("重复清理不存在的源目录幂等"), FInventoryJournal::DiscardUnbound(Directory.Path, WorldId, StoreId, Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryArchiveFreezeTest, "WHFramework.Inventory.Runtime.ArchiveFreezeAndCancellation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInventoryArchiveFreezeTest::RunTest(const FString& Parameters)
{
    InventoryRuntimeTests::FTemporaryDirectory TargetDirectory;
    InventoryRuntimeTests::FWorldFixture Fixture;
    if(!TestTrue(TEXT("冻结夹具初始化"), Fixture.Data.Initialize()) || !TestTrue(TEXT("冻结夹具打开"), Fixture.Start(true))) return false;
    const FInventorySnapshot Initial = Fixture.Data.State(201);
    FInventoryBatchRequest Create;
    Create.CreateInventories = {Initial};
    Create.Operations = {Fixture.Data.Add(Initial.InventoryId, 3)};
    if(!TestTrue(TEXT("冻结前初始资产提交"), Fixture.Execute(Create).IsSuccess())) return false;
    const FInventorySnapshot Before = Fixture.Read(Initial.InventoryId);
    const FInventoryAuthorityContext Authority = Fixture.Store->MakeServerAuthority(Fixture.Data.OwnerId, TEXT("Automation.Freeze"));
    FInventoryBatchRequest Grant;
    Grant.Operations = {Fixture.Data.Add(Initial.InventoryId, 1)};
    const auto ExistingCompletions = MakeShared<int32>(0);
    const auto ExistingResult = MakeShared<EInventoryResultCode>(EInventoryResultCode::InvalidRequest);
    Fixture.Store->EnqueueAuthorizedBatch(Grant, Authority, [ExistingCompletions, ExistingResult](const FInventoryCommandResult& Result)
    {
        ++(*ExistingCompletions);
        *ExistingResult = Result.Code;
    });
    const FGuid OperationId = FGuid::NewGuid();
    FInventoryArchiveSnapshot Frozen;
    if(!TestTrue(TEXT("持原世界边界冻结所有经济写入"), Fixture.Store->FreezeArchive(OperationId, Frozen, Fixture.Error))) return false;
    TestTrue(TEXT("冻结期间读服务保持可用"), Fixture.Store->IsReady() && Fixture.Store->IsArchiveFrozen() && Fixture.Read(Initial.InventoryId) == Before);
    Fixture.Store->Tick(.1f);
    TestEqual(TEXT("冻结之前的排队事务暂不执行"), *ExistingCompletions, 0);
    TestTrue(TEXT("普通增删命令被冻结拒绝"), Fixture.Store->ExecuteAuthorizedBatch(Grant, Authority).Code == EInventoryResultCode::WorldNotReady);
    FInventoryBatchRequest CreateDuringFreeze;
    CreateDuringFreeze.CreateInventories = {Fixture.Data.State(202)};
    TestTrue(TEXT("冻结覆盖容器创建"), Fixture.Execute(CreateDuringFreeze).Code == EInventoryResultCode::WorldNotReady);
    FInventoryBatchRequest DeleteDuringFreeze;
    DeleteDuringFreeze.DeleteInventories = {Initial.InventoryId};
    TestTrue(TEXT("冻结覆盖容器删除"), Fixture.Execute(DeleteDuringFreeze).Code == EInventoryResultCode::WorldNotReady);
    FInventoryBatchRequest DomainDuringFreeze;
    DomainDuringFreeze.DomainChanges = {InventoryRuntimeTests::CounterRedo(FGuid::NewGuid(), 0, 1)};
    TestTrue(TEXT("冻结覆盖只有领域记录的事务"), Fixture.Execute(DomainDuringFreeze).Code == EInventoryResultCode::WorldNotReady);
    const auto RejectedCompletions = MakeShared<int32>(0);
    Fixture.Store->EnqueueAuthorizedBatch(Grant, Authority, [RejectedCompletions, this](const FInventoryCommandResult& Result)
    {
        ++(*RejectedCompletions);
        TestTrue(TEXT("冻结期间新排队请求明确拒绝"), Result.Code == EInventoryResultCode::WorldNotReady);
    });
    TestEqual(TEXT("冻结拒绝仍回调一次"), *RejectedCompletions, 1);
    TestFalse(TEXT("冻结不能退休热库存"), Fixture.Store->Retire(Initial.InventoryId, Fixture.Error));
    TestFalse(TEXT("冻结不能另写检查点"), Fixture.Store->SaveCheckpoint(Fixture.Error));
    TestFalse(TEXT("冻结不能关闭源日志"), Fixture.Store->CloseStore(false, Fixture.Error));
    TestTrue(TEXT("被拒关闭没有令源进入closing状态"), Fixture.Store->IsReady());
    TestFalse(TEXT("其他操作不能释放源冻结"), Fixture.Store->ReleaseArchive(FGuid::NewGuid(), Fixture.Error));
    const FGuid TargetStoreId = FGuid::NewGuid();
    TSharedPtr<FInventoryPreparedStore, ESPMode::ThreadSafe> Prepared;
    if(!TestTrue(TEXT("冻结边界可以准备独立目标"), FInventoryJournal::PrepareFork(Frozen, TargetDirectory.Path, TargetStoreId, OperationId, Prepared, Fixture.Error))) return false;
    Prepared->Close();
    TestFalse(TEXT("取消器必须证明目标属于本操作"), FInventoryJournal::DiscardPrepared(TargetDirectory.Path, Fixture.Data.WorldId, TargetStoreId, FGuid::NewGuid(), Fixture.Error));
    TestTrue(TEXT("错误取消不会移除目标"), IFileManager::Get().DirectoryExists(*TargetDirectory.Path));
    TestTrue(TEXT("正确取消先退休目录再释放锁和删除"), FInventoryJournal::DiscardPrepared(TargetDirectory.Path, Fixture.Data.WorldId, TargetStoreId, OperationId, Fixture.Error));
    TestFalse(TEXT("取消目标已经清理"), IFileManager::Get().DirectoryExists(*TargetDirectory.Path));
    TestTrue(TEXT("重复取消已经不存在的目标幂等成功"), FInventoryJournal::DiscardPrepared(TargetDirectory.Path, Fixture.Data.WorldId, TargetStoreId, OperationId, Fixture.Error));
    if(!TestTrue(TEXT("取消发布后释放原操作冻结"), Fixture.Store->ReleaseArchive(OperationId, Fixture.Error))) return false;
    TestTrue(TEXT("取消未替换存储身份或源内容"), !Fixture.Store->IsArchiveFrozen() && Fixture.Store->GetStoreId() == Fixture.Data.StoreId && Fixture.Read(Initial.InventoryId) == Before);
    Fixture.Store->Tick(.1f);
    TestTrue(TEXT("原队列在同一源分支恢复执行一次"), *ExistingCompletions == 1 && *ExistingResult == EInventoryResultCode::Success);
    TestEqual(TEXT("只执行原已排队的一次加物品"), Fixture.Read(Initial.InventoryId).Slots[0].Item.Count, 4);
    const FGuid DuringDrainOperation = FGuid::NewGuid();
    const TWeakObjectPtr<UInventoryWorldSubsystem> WeakStore = Fixture.Store;
    const auto LaterCompletions = MakeShared<int32>(0);
    Fixture.Store->EnqueueAuthorizedBatch(Grant, Authority, [WeakStore, DuringDrainOperation, this](const FInventoryCommandResult& Result)
    {
        FString Error;
        FInventoryArchiveSnapshot Cut;
        TestTrue(TEXT("队列回调可以在完整事务之间取得冻结边界"), Result.IsSuccess() && WeakStore.IsValid() && WeakStore->FreezeArchive(DuringDrainOperation, Cut, Error));
    });
    Fixture.Store->EnqueueAuthorizedBatch(Grant, Authority, [LaterCompletions](const FInventoryCommandResult&) { ++(*LaterCompletions); });
    Fixture.Store->Tick(.1f);
    TestTrue(TEXT("当前Tick已取出的剩余队列同样暂停"), Fixture.Store->IsArchiveFrozen() && *LaterCompletions == 0 && Fixture.Read(Initial.InventoryId).Slots[0].Item.Count == 5);
    if(!TestTrue(TEXT("中途冻结可取消回原分支"), Fixture.Store->ReleaseArchive(DuringDrainOperation, Fixture.Error))) return false;
    Fixture.Store->Tick(.1f);
    TestTrue(TEXT("恢复保留原队列顺序且不会丢失请求"), *LaterCompletions == 1 && Fixture.Read(Initial.InventoryId).Slots[0].Item.Count == 6);
    Fixture.Shutdown();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryArchiveAdoptionTest, "WHFramework.Inventory.Runtime.ArchiveAdoptionAndSecondSaveAs", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInventoryArchiveAdoptionTest::RunTest(const FString& Parameters)
{
    InventoryRuntimeTests::FTemporaryDirectory FirstTarget;
    InventoryRuntimeTests::FTemporaryDirectory SecondTarget;
    InventoryRuntimeTests::FWorldFixture Fixture;
    const auto Adapter = MakeShared<InventoryRuntimeTests::FCounterAdapter>();
    if(!TestTrue(TEXT("切换夹具初始化"), Fixture.Data.Initialize()) || !TestTrue(TEXT("切换源存档打开"), Fixture.Start(true, Adapter))) return false;
    const FInventorySnapshot Initial = Fixture.Data.State(301);
    const FGuid CounterId(311, 312, 313, 314);
    FInventoryBatchRequest Create;
    Create.CreateInventories = {Initial};
    Create.Operations = {Fixture.Data.Add(Initial.InventoryId, 4)};
    Create.DomainChanges = {InventoryRuntimeTests::CounterRedo(CounterId, 0, 1)};
    if(!TestTrue(TEXT("源初始库存与领域提交"), Fixture.Execute(Create).IsSuccess())) return false;
    UAbilityInventoryBase* HotInventory = Fixture.Store->Resolve(Initial.InventoryId);
    const FInventorySnapshot Before = Fixture.Read(Initial.InventoryId);
    const FGuid Epoch = Fixture.Store->GetWorldEpoch();
    const FInventoryAuthorityContext OriginalAuthority = Fixture.Store->MakeServerAuthority(Fixture.Data.OwnerId, TEXT("Automation.Adoption"));
    APlayerController* Controller = Fixture.World->SpawnActor<APlayerController>();
    APawn* Pawn = Fixture.World->SpawnActor<APawn>();
    if(!TestNotNull(TEXT("本地owner控制器创建"), Controller) || !TestNotNull(TEXT("本地owner Pawn创建"), Pawn)) return false;
    Controller->Possess(Pawn);
    FInventoryAccessBinding Binding;
    Binding.InventoryId = Initial.InventoryId;
    Binding.LifetimeOwner = Pawn;
    Binding.bDistanceLimited = false;
    Binding.Validator = [](APlayerController*, const FGuid&, bool) { return EInventoryResultCode::Success; };
    if(!TestTrue(TEXT("owner访问绑定"), Fixture.Store->SetAccessBinding(MoveTemp(Binding)))) return false;
    UInventoryNetworkComponent* Network = NewObject<UInventoryNetworkComponent>(Controller);
    Controller->AddInstanceComponent(Network);
    Network->RegisterComponent();
    Network->WorldSubsystem = Fixture.Store;
    Network->AdoptedHandle = Fixture.Store->OnStoreAdopted.AddUObject(Network, &UInventoryNetworkComponent::HandleStoreAdopted);
    if(!TestTrue(TEXT("初始owner快照与会话绑定"), Network->BindOwner(Fixture.Data.OwnerId, Initial.InventoryId))) return false;
    const FGuid OriginalConnection = Network->ConnectionId;
    const FGuid OriginalSession = Network->Sessions.FindChecked(Initial.InventoryId).SessionId;
    FInventoryClientRequest OldPacket;
    OldPacket.RequestId = 1;
    OldPacket.ConnectionId = OriginalConnection;
    OldPacket.StoreId = Fixture.Data.StoreId;
    OldPacket.WorldEpoch = Epoch;
    OldPacket.Operation = EInventoryOperationType::SetFavorite;
    OldPacket.SourceInventoryId = Initial.InventoryId;
    OldPacket.SourceSlot = Before.Slots[0].Key;
    OldPacket.ExpectedSignature = Before.Slots[0].Item.Signature();
    OldPacket.Participants.Add({Initial.InventoryId, OriginalSession, Before.Revision, Before.Generation});
    FInventoryBatchRequest Grant;
    Grant.Operations = {Fixture.Data.Add(Initial.InventoryId, 1)};
    const auto CancelledQueue = MakeShared<int32>(0);
    Fixture.Store->EnqueueAuthorizedBatch(Grant, OriginalAuthority, [CancelledQueue, this](const FInventoryCommandResult& Result)
    {
        ++(*CancelledQueue);
        TestTrue(TEXT("已发布后旧队列有明确分支改变回执"), Result.Code == EInventoryResultCode::GenerationChanged);
    });
    int32 AdoptionEvents = 0;
    const FDelegateHandle Adopted = Fixture.Store->OnStoreAdopted.AddLambda([&AdoptionEvents] { ++AdoptionEvents; });
    const FGuid FirstStoreId = FGuid::NewGuid();
    const FGuid FirstOperation = FGuid::NewGuid();
    FInventoryArchiveSnapshot Frozen;
    TSharedPtr<FInventoryPreparedStore, ESPMode::ThreadSafe> Prepared;
    if(!TestTrue(TEXT("另存开始持有源冻结"), Fixture.Store->FreezeArchive(FirstOperation, Frozen, Fixture.Error)) ||
       !TestTrue(TEXT("提交前打开并准备目标日志"), FInventoryJournal::PrepareFork(Frozen, FirstTarget.Path, FirstStoreId, FirstOperation, Prepared, Fixture.Error)) ||
       !TestTrue(TEXT("普通manifest发布之前完成经济ready"), Prepared->MarkCommitted(Fixture.Error)) ||
       !TestTrue(TEXT("普通manifest成功回执后采用已打开目标"), Fixture.Store->AdoptArchive(FirstOperation, *Prepared, Fixture.Error))) return false;
    TestTrue(TEXT("采用后保持逻辑世界与运行epoch"), Fixture.Store->GetWorldId() == Fixture.Data.WorldId && Fixture.Store->GetWorldEpoch() == Epoch);
    TestTrue(TEXT("采用仅更换独立StoreId且释放冻结"), Fixture.Store->GetStoreId() == FirstStoreId && !Fixture.Store->IsArchiveFrozen() && Prepared->IsAdopted());
    TestTrue(TEXT("热库存对象和最终内容保持原样"), Fixture.Store->Resolve(Initial.InventoryId) == HotInventory && Fixture.Read(Initial.InventoryId) == Before);
    TestTrue(TEXT("没有重启领域或重复发布资产"), Adapter->RestoreCount == 0 && Adapter->PublishCount == 1);
    TestTrue(TEXT("原队列被取消一次且只发布一次采用事件"), *CancelledQueue == 1 && AdoptionEvents == 1);
    TestTrue(TEXT("网络connection与每个owner session轮换"), Network->ConnectionId.IsValid() && Network->ConnectionId != OriginalConnection &&
        Network->Sessions.Contains(Initial.InventoryId) && Network->Sessions.FindChecked(Initial.InventoryId).SessionId != OriginalSession && Network->GetStoreId() == FirstStoreId);
    TestTrue(TEXT("旧服务器授权不能进入新分支"), Fixture.Store->ExecuteAuthorizedBatch(Grant, OriginalAuthority).Code == EInventoryResultCode::GenerationChanged);
    const uint64 HighWaterMark = Network->RequestHighWaterMark;
    Network->ServerSubmit_Implementation(OldPacket);
    TestEqual(TEXT("旧网络请求不能污染新分支请求高水位"), Network->RequestHighWaterMark, HighWaterMark);
    TestTrue(TEXT("旧消息没有改变新分支库存"), Fixture.Read(Initial.InventoryId) == Before);
    const FInventoryAuthorityContext FirstAuthority = Fixture.Store->MakeServerAuthority(Fixture.Data.OwnerId, TEXT("Automation.FirstBranch"));
    if(!TestTrue(TEXT("新授权提交到目标WAL"), Fixture.Store->ExecuteAuthorizedBatch(Grant, FirstAuthority).IsSuccess())) return false;
    const FInventorySnapshot FirstFinal = Fixture.Read(Initial.InventoryId);
    FInventoryArchiveSnapshot OriginalCut;
    TestTrue(TEXT("采用后源锁已释放可独立导出"), FInventoryJournal::ExportLatest(Fixture.Directory.Path, Fixture.Data.WorldId, Fixture.Data.StoreId, OriginalCut, Fixture.Error));
    TestEqual(TEXT("新分支写入没有推进原存档水位"), OriginalCut.Sequence, uint64(1));
    const FGuid SecondStoreId = FGuid::NewGuid();
    const FGuid SecondOperation = FGuid::NewGuid();
    const FGuid FirstConnection = Network->ConnectionId;
    TSharedPtr<FInventoryPreparedStore, ESPMode::ThreadSafe> SecondPrepared;
    if(!TestTrue(TEXT("已有独立分支可再次冻结另存"), Fixture.Store->FreezeArchive(SecondOperation, Frozen, Fixture.Error)) ||
       !TestTrue(TEXT("二次另存以当前分支为源"), Frozen.StoreId == FirstStoreId && Frozen.Sequence == 2) ||
       !TestTrue(TEXT("二次目标完整准备"), FInventoryJournal::PrepareFork(Frozen, SecondTarget.Path, SecondStoreId, SecondOperation, SecondPrepared, Fixture.Error)) ||
       !TestTrue(TEXT("二次目标经济ready"), SecondPrepared->MarkCommitted(Fixture.Error)) ||
       !TestTrue(TEXT("二次采用无需加载或重发奖励"), Fixture.Store->AdoptArchive(SecondOperation, *SecondPrepared, Fixture.Error))) return false;
    TestTrue(TEXT("二次切换保持实例和版本，换新网络分支身份"), Fixture.Store->GetStoreId() == SecondStoreId && Fixture.Store->GetWorldEpoch() == Epoch &&
        Fixture.Store->Resolve(Initial.InventoryId) == HotInventory && Fixture.Read(Initial.InventoryId) == FirstFinal &&
        Network->ConnectionId != FirstConnection && Network->GetStoreId() == SecondStoreId && AdoptionEvents == 2);
    TestTrue(TEXT("第一分支的缓存授权在二次切换后失效"), Fixture.Store->ExecuteAuthorizedBatch(Grant, FirstAuthority).Code == EInventoryResultCode::GenerationChanged);
    FInventoryArchiveSnapshot FirstCut;
    TestTrue(TEXT("第一分支独立保留切换前全部写入"), FInventoryJournal::ExportLatest(FirstTarget.Path, Fixture.Data.WorldId, FirstStoreId, FirstCut, Fixture.Error) && FirstCut.Sequence == 2);
    Fixture.Store->OnStoreAdopted.Remove(Adopted);
    Fixture.Store->OnStoreAdopted.Remove(Network->AdoptedHandle);
    Network->UnbindOwner();
    Fixture.Shutdown();
    return true;
}

#endif

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryNetworkBoundsTest, "WHFramework.Inventory.Runtime.NetworkBounds", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInventoryNetworkBoundsTest::RunTest(const FString& Parameters)
{
    FInventoryClientRequest Request;
    Request.RequestId = 42;
    Request.ConnectionId = FGuid::NewGuid();
    Request.StoreId = FGuid::NewGuid();
    Request.WorldEpoch = FGuid::NewGuid();
    Request.Operation = EInventoryOperationType::QuickStack;
    Request.SourceInventoryId = FGuid::NewGuid();
    Request.SourceGroup = TAG_InventoryRuntimeTest_Storage;
    Request.TargetInventories = { FGuid::NewGuid(), FGuid::NewGuid() };
    Request.bAutomatic = true;
    Request.Quantity = 19;
    TArray<uint8> Bytes;
    bool bSuccess = false;
    FMemoryWriter Writer(Bytes, true);
    Request.NetSerialize(Writer, nullptr, bSuccess);
    TestTrue(TEXT("有界请求可编码"), bSuccess);
    FInventoryClientRequest Decoded;
    FMemoryReader Reader(Bytes, true);
    Decoded.NetSerialize(Reader, nullptr, bSuccess);
    TestTrue(TEXT("有界请求可解码"), bSuccess);
    TestTrue(TEXT("批量目标与自动整理语义保留"), Decoded.TargetInventories == Request.TargetInventories && Decoded.bAutomatic && Decoded.Quantity == 19);
    TArray<uint8> BadName = Bytes;
    BadName[56] = 0xff;
    BadName[57] = 0xff;
    FMemoryReader BadReader(BadName, true);
    FInventoryClientRequest Rejected;
    Rejected.NetSerialize(BadReader, nullptr, bSuccess);
    TestFalse(TEXT("名字长度在分配内存前拒绝"), bSuccess);
    Request.TargetInventories.SetNum(9);
    TArray<uint8> Oversized;
    FMemoryWriter OversizedWriter(Oversized, true);
    Request.NetSerialize(OversizedWriter, nullptr, bSuccess);
    TestFalse(TEXT("超限目标数组不能序列化"), bSuccess);
    FInventorySnapshotChunk Chunk;
    Chunk.ConnectionId = FGuid::NewGuid();
    Chunk.StoreId = FGuid::NewGuid();
    Chunk.WorldEpoch = FGuid::NewGuid();
    Chunk.SessionId = FGuid::NewGuid();
    Chunk.InventoryId = FGuid::NewGuid();
    Chunk.TransferId = FGuid::NewGuid();
    Chunk.Revision = 1;
    Chunk.Count = 1;
    Chunk.TotalBytes = 32;
    Chunk.Bytes.Init(0x5a, 32);
    TArray<uint8> ChunkBytes;
    FMemoryWriter ChunkWriter(ChunkBytes, true);
    Chunk.NetSerialize(ChunkWriter, nullptr, bSuccess);
    TestTrue(TEXT("有界快照分片编码"), bSuccess);
    FInventorySnapshotChunk ReadChunk;
    FMemoryReader ChunkReader(ChunkBytes, true);
    ReadChunk.NetSerialize(ChunkReader, nullptr, bSuccess);
    TestTrue(TEXT("分片头和正文完整往返"), bSuccess && ReadChunk.Bytes == Chunk.Bytes && ReadChunk.TransferId == Chunk.TransferId);
    Chunk.TotalBytes = FInventorySnapshotCodec::MaximumBytes + 1;
    TArray<uint8> InvalidChunk;
    FMemoryWriter InvalidWriter(InvalidChunk, true);
    Chunk.NetSerialize(InvalidWriter, nullptr, bSuccess);
    TestFalse(TEXT("声明超大快照在数组序列化前拒绝"), bSuccess);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryNetworkReplicaTest, "WHFramework.Inventory.Runtime.ReplicaAssemblyAndSession", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInventoryNetworkReplicaTest::RunTest(const FString& Parameters)
{
    InventoryRuntimeTests::FData Data;
    if(!TestTrue(TEXT("副本定义初始化"), Data.Initialize(256)))
    {
        return false;
    }
    TStrongObjectPtr<UWorld> World(NewObject<UWorld>());
    TStrongObjectPtr<UInventoryWorldSubsystem> Store(NewObject<UInventoryWorldSubsystem>(World.Get()));
    const TArray<FInventoryProfileDefinition> Profiles = { Data.Profile };
    FString Error;
    TestTrue(TEXT("客户端只配置codec定义"), Store->Configure(Data.Items, Profiles, nullptr, Error));
    TStrongObjectPtr<UInventoryNetworkComponent> Network(NewObject<UInventoryNetworkComponent>());
    Network->WorldSubsystem = Store.Get();
    const FGuid Connection = FGuid::NewGuid();
    const FGuid Epoch = FGuid::NewGuid();
    FInventorySnapshot Snapshot = Data.State(1);
    Snapshot.Revision = 7;
    for(FInventorySlotState& Slot : Snapshot.Slots)
    {
        Slot.Item = Data.DecoratedOre(1);
    }
    Network->ClientBegin_Implementation(Connection, Data.WorldId, Data.StoreId, Epoch, Snapshot.InventoryId);
    TArray<uint8> Bytes;
    TestTrue(TEXT("分片快照编码"), FInventorySnapshotCodec::Encode(Snapshot, Data.Registry, Data.Profiles, Bytes, Error));
    TestTrue(TEXT("测试包含多个真实分片"), Bytes.Num() > 8192);
    FInventorySnapshotChunk Chunk;
    Chunk.ConnectionId = Connection;
    Chunk.StoreId = Data.StoreId;
    Chunk.WorldEpoch = Epoch;
    Chunk.SessionId = FGuid::NewGuid();
    Chunk.InventoryId = Snapshot.InventoryId;
    Chunk.TransferId = FGuid::NewGuid();
    Chunk.Revision = Snapshot.Revision;
    Chunk.Generation = Snapshot.Generation;
    Chunk.Count = FMath::DivideAndRoundUp(Bytes.Num(), 8192);
    Chunk.TotalBytes = Bytes.Num();
    Chunk.Checksum = FCrc::MemCrc32(Bytes.GetData(), Bytes.Num());
    int32 SnapshotEvents = 0;
    int32 AppliedEvents = 0;
    FInventoryChangeSet LastApplied;
    FInventoryDelta LastAppliedView;
    Network->OnAppliedChanges.AddLambda([&AppliedEvents, &LastApplied, &LastAppliedView](const FInventoryDelta& InView, const FInventoryChangeSet& InChanges)
    {
        ++AppliedEvents;
        LastApplied = InChanges;
        LastAppliedView = InView;
    });
    Network->OnSnapshot.AddLambda([&SnapshotEvents](const FInventorySnapshot&)
    {
        ++SnapshotEvents;
    });
    for(int32 Index = Chunk.Count - 1; Index >= 0; --Index)
    {
        Chunk.Index = Index;
        const int32 Offset = Index * 8192;
        const int32 Count = FMath::Min(8192, Bytes.Num() - Offset);
        Chunk.Bytes.Reset(Count);
        Chunk.Bytes.Append(Bytes.GetData() + Offset, Count);
        Network->ClientSnapshot_Implementation(Chunk);
        if(Index > 0)
        {
            TestNull(TEXT("未完整装配前没有半张UI快照"), Network->FindView(Snapshot.InventoryId));
        }
    }
    TestEqual(TEXT("完整快照仅发布一次"), SnapshotEvents, 1);
    TestEqual(TEXT("首份快照不伪造获得事件"), AppliedEvents, 0);
    if(!TestTrue(TEXT("乱序分片装配完整一致"), Network->FindView(Snapshot.InventoryId) && *Network->FindView(Snapshot.InventoryId) == Snapshot))
    {
        return false;
    }
    FInventoryDelta Delta;
    Delta.ConnectionId = Connection;
    Delta.StoreId = Data.StoreId;
    Delta.WorldEpoch = Epoch;
    Delta.SessionId = Chunk.SessionId;
    Delta.InventoryId = Snapshot.InventoryId;
    Delta.TransactionId = FGuid::NewGuid();
    Delta.Generation = Snapshot.Generation;
    Delta.FromRevision = 7;
    Delta.ToRevision = 8;
    Delta.Slots.Add(Snapshot.Slots[0]);
    Delta.Slots[0].bFavorite = true;
    Network->ClientDelta_Implementation(Delta);
    TestEqual(TEXT("连续Delta推进一次版本"), Network->FindView(Snapshot.InventoryId)->Revision, uint64(8));
    TestEqual(TEXT("实际应用的差异发布一次"), AppliedEvents, 1);
    TestTrue(TEXT("差异带有修改前后和原事务身份"), LastApplied.TransactionId == Delta.TransactionId && LastApplied.ChangedSlots.Num() == 1 &&
        !LastApplied.ChangedSlots[0].Before.bFavorite && LastApplied.ChangedSlots[0].After.bFavorite && LastAppliedView.SessionId == Delta.SessionId);
    Network->ClientDelta_Implementation(Delta);
    TestEqual(TEXT("重复Delta幂等"), Network->FindView(Snapshot.InventoryId)->Revision, uint64(8));
    TestEqual(TEXT("重复Delta不重复发布获得来源"), AppliedEvents, 1);
    Delta.FromRevision = 9;
    Delta.ToRevision = 10;
    Network->ClientDelta_Implementation(Delta);
    TestEqual(TEXT("版本缺口保持最后已确认状态"), Network->FindView(Snapshot.InventoryId)->Revision, uint64(8));
    TestEqual(TEXT("未应用版本缺口不发布差异"), AppliedEvents, 1);
    FInventorySnapshot Refreshed = *Network->FindView(Snapshot.InventoryId);
    Refreshed.Revision = 9;
    Refreshed.Slots[0].Item.Count = 2;
    TArray<uint8> RefreshedBytes;
    if(!TestTrue(TEXT("重同步快照编码"), FInventorySnapshotCodec::Encode(Refreshed, Data.Registry, Data.Profiles, RefreshedBytes, Error))) return false;
    FInventorySnapshotChunk RefreshedChunk = Chunk;
    RefreshedChunk.TransferId = FGuid::NewGuid();
    RefreshedChunk.Revision = Refreshed.Revision;
    RefreshedChunk.TotalBytes = RefreshedBytes.Num();
    RefreshedChunk.Count = FMath::DivideAndRoundUp(RefreshedBytes.Num(), 8192);
    RefreshedChunk.Checksum = FCrc::MemCrc32(RefreshedBytes.GetData(), RefreshedBytes.Num());
    for(int32 Index = 0; Index < RefreshedChunk.Count; ++Index)
    {
        RefreshedChunk.Index = Index;
        const int32 Offset = Index * 8192;
        RefreshedChunk.Bytes.Reset();
        RefreshedChunk.Bytes.Append(RefreshedBytes.GetData() + Offset, FMath::Min(8192, RefreshedBytes.Num() - Offset));
        Network->ClientSnapshot_Implementation(RefreshedChunk);
    }
    TestEqual(TEXT("同会话重同步发布净变化"), AppliedEvents, 2);
    TestTrue(TEXT("重同步保留真实前后数量且不编造事务ID"), !LastApplied.TransactionId.IsValid() && LastAppliedView.FromRevision == 8 &&
        LastAppliedView.ToRevision == 9 && LastApplied.ChangedSlots.Num() == 1 && LastApplied.ChangedSlots[0].Before.Item.Count == 1 &&
        LastApplied.ChangedSlots[0].After.Item.Count == 2);
    FInventoryClientRequest Request;
    Request.Operation = EInventoryOperationType::SetFavorite;
    Request.SourceInventoryId = Snapshot.InventoryId;
    Request.SourceSlot = Snapshot.Slots[0].Key;
    int32 QueuedEvents = 0;
    bool bQueuedVisibleDuringCallback = true;
    FInventoryClientRequest LastQueued;
    Network->OnRequestQueued.AddLambda([&Network, &QueuedEvents, &bQueuedVisibleDuringCallback, &LastQueued](const FInventoryClientRequest& InRequest)
    {
        ++QueuedEvents;
        LastQueued = InRequest;
        bQueuedVisibleDuringCallback &= Network->GetPendingRequests().ContainsByPredicate([&InRequest](const FInventoryClientRequest& Candidate)
        {
            return Candidate.RequestId == InRequest.RequestId;
        });
    });
    const uint64 RequestId = Network->SubmitRequest(Request);
    TestTrue(TEXT("Submit立即返回稳定ID但尚未发送RPC"), RequestId != 0 && Network->Pending.FindChecked(RequestId).Attempts == 0);
    TestEqual(TEXT("统一提交入口发布一次实际入队通知"), QueuedEvents, 1);
    TestTrue(TEXT("入队通知已具有当前身份会话与签名"), LastQueued.RequestId == RequestId && LastQueued.ConnectionId == Connection &&
        LastQueued.StoreId == Data.StoreId && LastQueued.WorldEpoch == Epoch && LastQueued.Participants.Num() == 1 &&
        LastQueued.Participants[0].SessionId == Chunk.SessionId && LastQueued.ExpectedSignature == Network->FindView(Snapshot.InventoryId)->Slots[0].Item.Signature());
    TestTrue(TEXT("入队观察者回调中可查询到该请求"), bQueuedVisibleDuringCallback);
    TArray<FInventoryClientRequest> PendingCopy = Network->GetPendingRequests();
    if (!TestEqual(TEXT("打开UI可读取已经排队的请求"), PendingCopy.Num(), 1)) return false;
    PendingCopy[0].Quantity += 1;
    TestEqual(TEXT("只读查询副本不能修改网络原请求"), Network->GetPendingRequests()[0].Quantity, Request.Quantity);
    int32 ResultEvents = 0;
    Network->OnResult.AddLambda([&ResultEvents](const FInventoryCommandResult&)
    {
        ++ResultEvents;
    });
    FInventoryCommandResult Result;
    Result.RequestId = RequestId;
    Result.Code = EInventoryResultCode::Success;
    Network->ClientResult_Implementation(FGuid::NewGuid(), Result);
    TestEqual(TEXT("旧连接ACK不能结算新请求"), ResultEvents, 0);
    Network->ClientResult_Implementation(Connection, Result);
    TestEqual(TEXT("原连接ACK结算一次"), ResultEvents, 1);
    TestTrue(TEXT("已结算请求不再进入待处理查询"), Network->GetPendingRequests().IsEmpty());
    Network->ClientResult_Implementation(Connection, Result);
    TestEqual(TEXT("重复ACK不重复通知UI"), ResultEvents, 1);
    const FDelegateHandle ImmediateAckHandle = Network->OnRequestQueued.AddLambda([&Network, Connection](const FInventoryClientRequest& InRequest)
    {
        FInventoryCommandResult ImmediateResult;
        ImmediateResult.RequestId = InRequest.RequestId;
        ImmediateResult.Code = EInventoryResultCode::Success;
        Network->ClientResult_Implementation(Connection, ImmediateResult);
    });
    TestTrue(TEXT("观察期间已结算仍返回实际入队ID"), Network->SubmitRequest(Request) != 0);
    TestTrue(TEXT("观察期间结算没有遗留Pending"), Network->GetPendingRequests().IsEmpty());
    Network->OnRequestQueued.Remove(ImmediateAckHandle);
    Network->ClientClosed_Implementation(Connection, Snapshot.InventoryId, Chunk.SessionId, EInventoryResultCode::Success);
    TestNull(TEXT("关闭会话立即移除私有视图"), Network->FindView(Snapshot.InventoryId));
    TestTrue(TEXT("关闭会话清理装配缓冲"), Network->Assemblies.IsEmpty());
    UInventoryNetworkComponent::FClientView Reopened;
    Reopened.SessionId = Chunk.SessionId;
    Reopened.Snapshot = Snapshot;
    Network->Views.Add(Snapshot.InventoryId, MoveTemp(Reopened));
    const uint64 UnsentId = Network->SubmitRequest(Request);
    const uint64 SentId = Network->SubmitRequest(Request);
    if(!TestTrue(TEXT("换存档前同时存在已发送和未发送请求"), UnsentId != 0 && SentId != 0))
    {
        return false;
    }
    Network->Pending.FindChecked(SentId).Attempts = 1;
    const TArray<FInventoryClientRequest> ReopenedPending = Network->GetPendingRequests();
    TestTrue(TEXT("恢复UI读取请求顺序稳定且保留实际身份"), ReopenedPending.Num() == 2 && ReopenedPending[0].RequestId == UnsentId && ReopenedPending[1].RequestId == SentId);
    TMap<uint64, EInventoryResultCode> CloseResults;
    Network->OnResult.AddLambda([&CloseResults](const FInventoryCommandResult& InResult)
    {
        CloseResults.Add(InResult.RequestId, InResult.Code);
    });
    Network->ClientReset_Implementation(FGuid::NewGuid(), EInventoryResultCode::WorldNotReady);
    TestTrue(TEXT("旧连接清理消息不能关闭当前连接"), Network->FindView(Snapshot.InventoryId) != nullptr && Network->Pending.Num() == 2);
    Network->UnbindOwner();
    TestNull(TEXT("解绑清除全部私有视图"), Network->FindView(Snapshot.InventoryId));
    TestTrue(TEXT("解绑清除所有装配和请求"), Network->Assemblies.IsEmpty() && Network->Pending.IsEmpty());
    TestTrue(TEXT("未发送请求明确未执行"), CloseResults.FindRef(UnsentId) == EInventoryResultCode::WorldNotReady);
    TestTrue(TEXT("已发送请求保留提交结果待确认语义"), CloseResults.FindRef(SentId) == EInventoryResultCode::ConfirmationPending);
    TestFalse(TEXT("解绑清除owner和epoch"), Network->GetOwnerInventoryId().IsValid() || Network->GetWorldEpoch().IsValid() || Network->GetStoreId().IsValid());
    const int32 QueuedBeforeRejected = QueuedEvents;
    TestEqual(TEXT("解绑后不能排队新请求"), Network->SubmitRequest(Request), uint64(0));
    TestEqual(TEXT("拒绝入队不发布请求通知"), QueuedEvents, QueuedBeforeRejected);
    Network->ClientSnapshot_Implementation(Chunk);
    TestTrue(TEXT("旧世界分片不能在解绑后重建视图"), Network->Assemblies.IsEmpty() && Network->Views.IsEmpty());
    const FGuid NewConnection = FGuid::NewGuid();
    const FGuid NewEpoch = FGuid::NewGuid();
    const FGuid NewStoreId = FGuid::NewGuid();
    Network->ClientBegin_Implementation(NewConnection, FGuid::NewGuid(), NewStoreId, NewEpoch, Snapshot.InventoryId);
    Network->ClientReset_Implementation(Connection, EInventoryResultCode::WorldNotReady);
    TestTrue(TEXT("重新绑定之后旧连接Reset仍被忽略"), Network->ConnectionId == NewConnection && Network->GetWorldEpoch() == NewEpoch && Network->GetStoreId() == NewStoreId);
    const FGuid BranchConnection = FGuid::NewGuid();
    Network->ClientBegin_Implementation(BranchConnection, Data.WorldId, NewStoreId, Epoch, Snapshot.InventoryId);
    Network->ClientSnapshot_Implementation(Chunk);
    Network->ClientDelta_Implementation(Delta);
    TestTrue(TEXT("保留WorldEpoch时旧Store/Connection分片和Delta仍被拒绝"), Network->Assemblies.IsEmpty() && Network->Views.IsEmpty());
    Chunk.ConnectionId = BranchConnection;
    Chunk.StoreId = NewStoreId;
    Chunk.SessionId = FGuid::NewGuid();
    Chunk.TransferId = FGuid::NewGuid();
    for(int32 Index = 0; Index < Chunk.Count; ++Index)
    {
        Chunk.Index = Index;
        const int32 Offset = Index * 8192;
        Chunk.Bytes.Reset();
        Chunk.Bytes.Append(Bytes.GetData() + Offset, FMath::Min(8192, Bytes.Num() - Offset));
        Network->ClientSnapshot_Implementation(Chunk);
        if(Index == 0)
        {
            const int32 AssemblyCount = Network->Assemblies.Num();
            Network->ClientClosed_Implementation(Connection, Snapshot.InventoryId, FGuid(), EInventoryResultCode::WorldNotReady);
            TestEqual(TEXT("旧Closed不能删除新分支尚未完成的装配"), Network->Assemblies.Num(), AssemblyCount);
        }
    }
    TestTrue(TEXT("同逻辑WorldId和epoch的新分支快照正常装配"), Network->FindView(Snapshot.InventoryId) && *Network->FindView(Snapshot.InventoryId) == Snapshot);
    const uint64 BranchRequestId = Network->SubmitRequest(Request);
    TestTrue(TEXT("UI请求自动绑定当前分支身份"), BranchRequestId != 0 && Network->Pending.FindChecked(BranchRequestId).Request.ConnectionId == BranchConnection &&
        Network->Pending.FindChecked(BranchRequestId).Request.StoreId == NewStoreId);
    Network->UnbindOwner();
    const FGuid ReentrantConnection = FGuid::NewGuid();
    Network->ClientBegin_Implementation(ReentrantConnection, Data.WorldId, Data.StoreId, Epoch, Snapshot.InventoryId);
    UInventoryNetworkComponent::FClientView ReentrantView;
    ReentrantView.SessionId = FGuid::NewGuid();
    ReentrantView.Snapshot = Snapshot;
    Network->Views.Add(Snapshot.InventoryId, MoveTemp(ReentrantView));
    const FDelegateHandle ResetQueuedHandle = Network->OnRequestQueued.AddLambda([&Network, ReentrantConnection](const FInventoryClientRequest&)
    {
        Network->ClientReset_Implementation(ReentrantConnection, EInventoryResultCode::WorldNotReady);
    });
    TestEqual(TEXT("入队观察重入清连接后不返回失效请求ID"), Network->SubmitRequest(Request), uint64(0));
    TestTrue(TEXT("重入清理后没有遗留请求"), Network->GetPendingRequests().IsEmpty());
    Network->OnRequestQueued.Remove(ResetQueuedHandle);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryOverlayPrivacyTest, "WHFramework.Inventory.Runtime.PublicOverlayPrivacy", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInventoryOverlayPrivacyTest::RunTest(const FString& Parameters)
{
    if(!TestTrue(TEXT("注册库存实体Codec"), FInventoryVoxelEntityCodec::Register())) return false;
    FVoxelBlockEntityState NaturalEntity;
    if(!TestTrue(TEXT("自然箱只生成未初始化标记"), FVoxelBlockEntityCodec::MakeDefault(FInventoryVoxelEntityCodec::Kind, NaturalEntity))) return false;
    TestEqual(TEXT("默认实体的实际正文为8字节"), NaturalEntity.Payload.Num(), 8);
    TestTrue(TEXT("默认实体预算保留旧32KiB上限"), NaturalEntity.Payload.Num() <= FVoxelBlockEntityCodec::MaximumDefaultPayloadBytes &&
        FVoxelBlockEntityCodec::MaximumDefaultPayloadBytes == 32 * 1024);
    InventoryRuntimeTests::FData Data;
    if(!TestTrue(TEXT("大容器定义初始化"), Data.Initialize(4096)))
    {
        return false;
    }
    FInventorySnapshot Snapshot = Data.State(1);
    Snapshot.Revision = 1;
    for(FInventorySlotState& Slot : Snapshot.Slots)
    {
        Slot.Item = Data.DecoratedOre(1);
    }
    FVoxelBlockEntityState PrivateEntity;
    FString Error;
    if(!TestTrue(TEXT("合法大库存实体编码"), FInventoryVoxelEntityCodec::Encode(Snapshot, Data.Registry, Data.Profiles, PrivateEntity, Error)))
    {
        AddError(Error);
        return false;
    }
    TestTrue(TEXT("私有实体超过旧公共payload上限"), PrivateEntity.Payload.Num() > 256 * 1024);
    TestTrue(TEXT("默认实体的小预算不拒绝合法完整持久实体"), FVoxelBlockEntityCodec::Validate(PrivateEntity));
    FVoxelOverlaySnapshot Overlay;
    Overlay.Section = FIntVector(1, 2, 3);
    Overlay.Revision = 4;
    Overlay.Entities.Add(11, PrivateEntity);
    TArray<uint8> PublicBytes;
    TestTrue(TEXT("合法私有大实体可生成公共投影"), FVoxelOverlayRegionCodec::EncodeOverlay(Overlay, PublicBytes, Error));
    TestEqual(TEXT("公共正文只有section头、实体头和固定8字节标记"), PublicBytes.Num(), 43);
    FVoxelOverlaySnapshot PublicOverlay;
    TestTrue(TEXT("公共投影可被客户端解码"), FVoxelOverlayRegionCodec::DecodeOverlay(PublicBytes, PublicOverlay, Error));
    const FVoxelBlockEntityState* PublicEntity = PublicOverlay.Entities.Find(11);
    if(!TestTrue(TEXT("Kind与Schema保留且没有库存ID、Owner或Slots"), PublicEntity && FInventoryVoxelEntityCodec::IsPublic(*PublicEntity)))
    {
        return false;
    }
    TestTrue(TEXT("编码不修改权威私有正文"), Overlay.Entities.FindChecked(11).Payload == PrivateEntity.Payload);
    FVoxelOverlayDirectoryEntry Entry;
    Entry.Section = Overlay.Section;
    Entry.Revision = Overlay.Revision;
    Entry.PayloadBytes = PublicBytes.Num();
    TestTrue(TEXT("目录哈希绑定投影后正文"), FVoxelOverlayHash::Compute(PublicBytes, Entry.PayloadHash));
    const TArray<FVoxelOverlayDirectoryEntry> Entries = { Entry };
    const TArray<TArray<uint8>> Payloads = { PublicBytes };
    TArray<uint8> DeltaBytes;
    TestTrue(TEXT("公开Delta只装入已验证的公开正文"), FVoxelOverlayRegionCodec::EncodeDelta(Entries, Payloads, DeltaBytes));
    FInventorySnapshot InvalidAuthority;
    TestFalse(TEXT("公开标记不能反解为权威库存"), FInventoryVoxelEntityCodec::Decode(*PublicEntity, Data.Registry, Data.Profiles, InvalidAuthority, Error));
    return true;
}

#endif
