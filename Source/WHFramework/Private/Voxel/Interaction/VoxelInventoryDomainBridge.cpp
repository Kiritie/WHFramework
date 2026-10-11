#include "Voxel/Interaction/VoxelInventoryDomainBridge.h"

#include "Ability/Inventory/InventoryWorldSubsystem.h"
#include "Misc/Crc.h"
#include "Misc/SecureHash.h"
#include "Voxel/Generation/VoxelGenerationMath.h"
#include "Voxel/Save/InventoryVoxelEntityCodec.h"
#include "Voxel/Save/VoxelBlockEntityCodec.h"
#include "Voxel/Save/VoxelDeltaCodec.h"
#include "Voxel/Serialization/VoxelBinaryCodec.h"

namespace InventoryVoxelRecord
{
    constexpr uint32 Magic = 0x32535649;
    constexpr int32 MaximumBytes = 16 * 1024 * 1024;

    FIntVector SectionOf(const FIntVector& InPosition)
    {
        return {VoxelGeneration::FloorDivide(InPosition.X, 16), VoxelGeneration::FloorDivide(InPosition.Y, 16), VoxelGeneration::FloorDivide(InPosition.Z, 16)};
    }

    int32 IndexOf(const FIntVector& InPosition)
    {
        const auto Mod = [](int32 Value) { const int32 Result = Value % 16; return Result < 0 ? Result + 16 : Result; };
        return Mod(InPosition.X) + Mod(InPosition.Y) * 16 + Mod(InPosition.Z) * 256;
    }

    FIntVector PositionOf(const FIntVector& InSection, int32 InIndex)
    {
        return InSection * 16 + FIntVector(InIndex % 16, (InIndex / 16) % 16, InIndex / 256);
    }

    bool Encode(const FVoxelInventorySectionTarget& InTarget, TArray<uint8>& OutBytes, FString& OutError)
    {
        FVoxelByteWriter Writer(MaximumBytes - 4);
        Writer.U32(Magic);
        Writer.U32(2);
        Writer.I32(InTarget.After.Section.X);
        Writer.I32(InTarget.After.Section.Y);
        Writer.I32(InTarget.After.Section.Z);
        Writer.U64(InTarget.ExpectedRevision);
        Writer.U64(InTarget.After.Revision);
        TArray<int32> Keys;
        InTarget.After.Blocks.GetKeys(Keys);
        Keys.Sort();
        Writer.U32(Keys.Num());
        for(int32 Key : Keys)
        {
            const FVoxelBlockState& State = InTarget.After.Blocks.FindChecked(Key);
            Writer.I32(Key);
            Writer.U16(State.TypeId);
            Writer.U16(State.State);
        }
        InTarget.After.Entities.GetKeys(Keys);
        Keys.Sort();
        Writer.U32(Keys.Num());
        for(int32 Key : Keys)
        {
            const FVoxelBlockEntityState& State = InTarget.After.Entities.FindChecked(Key);
            Writer.I32(Key);
            Writer.U16(State.Kind);
            Writer.U16(State.Schema);
            Writer.Blob(State.Payload, FVoxelDeltaCodec::MaximumEntityPayloadBytes);
        }
        if(!Writer.Finish(OutBytes))
        {
            OutError = TEXT("Inventory.VoxelSectionRecordTooLarge");
            return false;
        }
        const uint32 Checksum = FCrc::MemCrc32(OutBytes.GetData(), OutBytes.Num());
        for(int32 Byte = 0; Byte < 4; ++Byte)
        {
            OutBytes.Add(static_cast<uint8>(Checksum >> (Byte * 8)));
        }
        return true;
    }

    bool Decode(TConstArrayView<uint8> InBytes, FVoxelInventorySectionTarget& OutTarget, FString& OutError)
    {
        if(InBytes.Num() < 48 || InBytes.Num() > MaximumBytes)
        {
            OutError = TEXT("Inventory.VoxelSectionRecordBounds");
            return false;
        }
        const auto Body = InBytes.Left(InBytes.Num() - 4);
        FVoxelByteReader ChecksumReader(InBytes.Right(4));
        if(ChecksumReader.U32() != FCrc::MemCrc32(Body.GetData(), Body.Num()))
        {
            OutError = TEXT("Inventory.VoxelSectionRecordChecksum");
            return false;
        }
        FVoxelByteReader Reader(Body);
        if(Reader.U32() != Magic || Reader.U32() != 2)
        {
            OutError = TEXT("Inventory.VoxelSectionRecordSchema");
            return false;
        }
        FVoxelInventorySectionTarget Candidate;
        Candidate.After.Section.X = Reader.I32();
        Candidate.After.Section.Y = Reader.I32();
        Candidate.After.Section.Z = Reader.I32();
        Candidate.ExpectedRevision = Reader.U64();
        Candidate.After.Revision = Reader.U64();
        const uint32 CellCount = Reader.U32();
        if(CellCount > 4096 || Candidate.ExpectedRevision >= Candidate.After.Revision || Candidate.After.Revision == MAX_uint64 ||
            Candidate.After.Section.X < MIN_int32 / 16 || Candidate.After.Section.X > MAX_int32 / 16 - 1 ||
            Candidate.After.Section.Y < MIN_int32 / 16 || Candidate.After.Section.Y > MAX_int32 / 16 - 1 ||
            Candidate.After.Section.Z < MIN_int32 / 16 || Candidate.After.Section.Z > MAX_int32 / 16 - 1)
        {
            Reader.Reject();
        }
        int32 Previous = INDEX_NONE;
        for(uint32 Index = 0; Index < CellCount && Reader.IsValid(); ++Index)
        {
            const int32 Key = Reader.I32();
            FVoxelBlockState State;
            State.TypeId = Reader.U16();
            State.State = Reader.U16();
            if(Key <= Previous || Key < 0 || Key >= 4096)
            {
                Reader.Reject();
                break;
            }
            Candidate.After.Blocks.Add(Key, State);
            Previous = Key;
        }
        const uint32 EntityCount = Reader.U32();
        if(EntityCount > FVoxelDeltaCodec::MaximumEntityCount)
        {
            Reader.Reject();
        }
        Previous = INDEX_NONE;
        for(uint32 Index = 0; Index < EntityCount && Reader.IsValid(); ++Index)
        {
            const int32 Key = Reader.I32();
            FVoxelBlockEntityState State;
            State.Kind = Reader.U16();
            State.Schema = Reader.U16();
            State.Payload = Reader.Blob(FVoxelDeltaCodec::MaximumEntityPayloadBytes);
            if(Key <= Previous || Key < 0 || Key >= 4096 || !FVoxelBlockEntityCodec::Validate(State) || FInventoryVoxelEntityCodec::IsPublic(State))
            {
                Reader.Reject();
                break;
            }
            Candidate.After.Entities.Add(Key, MoveTemp(State));
            Previous = Key;
        }
        if(!Reader.End())
        {
            OutError = TEXT("Inventory.VoxelSectionRecordMalformed");
            return false;
        }
        OutTarget = MoveTemp(Candidate);
        return true;
    }
}

FVoxelInventoryDomainBridge::FVoxelInventoryDomainBridge(UInventoryWorldSubsystem& InSystem)
    : System(InSystem)
{
}

FVoxelInventoryDomainBridge::~FVoxelInventoryDomainBridge()
{
    DetachRuntime();
}

FName FVoxelInventoryDomainBridge::GetDomain() const
{
    return TEXT("Inventory.VoxelSection");
}

FGuid FVoxelInventoryDomainBridge::SectionId(const FIntVector& InSection) const
{
    const FString Key = FString::Printf(TEXT("%s/Inventory.VoxelSection/2/%d/%d/%d"),
        *System.GetWorldId().ToString(EGuidFormats::Digits), InSection.X, InSection.Y, InSection.Z);
    const FTCHARToUTF8 Utf8(*Key);
    FMD5 Hash;
    Hash.Update(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
    uint8 Digest[16];
    Hash.Final(Digest);
    FVoxelByteReader Reader(MakeArrayView(Digest));
    return Reader.Guid();
}

bool FVoxelInventoryDomainBridge::Register(FString& OutError)
{
    if(!FInventoryVoxelEntityCodec::Register())
    {
        OutError = TEXT("Inventory.VoxelCodecRegistrationFailed");
        return false;
    }
    const TWeakPtr<FVoxelInventoryDomainBridge> Weak = AsShared();
    return System.RegisterRecordDomain(GetDomain(), InventoryVoxelRecord::MaximumBytes,
        [Weak](TConstArrayView<uint8> Bytes, FString& Error)
        {
            const auto Bridge = Weak.Pin();
            return Bridge && Bridge->ValidateRecord(Bytes, Error);
        }, OutError)
        && System.RegisterDomainAdapter(AsShared(), OutError)
        && System.RegisterSnapshotProjector(GetDomain(),
        [Weak](const FInventoryPlan& Plan, TArray<FInventoryDomainRedo>& Changes, FString& Error)
        {
            const auto Bridge = Weak.Pin();
            return Bridge && Bridge->ProjectInventories(Plan, Changes, Error);
        }, OutError);
}

bool FVoxelInventoryDomainBridge::ValidateRecord(TConstArrayView<uint8> InBytes, FString& OutError) const
{
    FVoxelInventorySectionTarget Target;
    if(!InventoryVoxelRecord::Decode(InBytes, Target, OutError))
    {
        return false;
    }
    for(const auto& Entity : Target.After.Entities)
    {
        if(FInventoryVoxelEntityCodec::IsInventory(Entity.Value) && !FInventoryVoxelEntityCodec::IsUninitialized(Entity.Value))
        {
            FInventorySnapshot Snapshot;
            if(!FInventoryVoxelEntityCodec::Decode(Entity.Value, System.GetRegistry(), System.GetProfiles(), Snapshot, OutError) ||
                Snapshot.WorldId != System.GetWorldId())
            {
                return false;
            }
        }
    }
    return true;
}

bool FVoxelInventoryDomainBridge::AttachRuntime(FVoxelWorldRuntime& InRuntime, FString& OutError)
{
    if(Runtime || !InRuntime.IsServer() || !System.IsReady())
    {
        OutError = TEXT("Inventory.VoxelAttachInvalid");
        return false;
    }
    Runtime = &InRuntime;
    TArray<FInventoryDomainRecord> Records;
    System.GetRecords().Enumerate(GetDomain(), Records);
    Records.Sort([](const auto& A, const auto& B) { return InventoryOrder::GuidLess(A.Key.EntityId, B.Key.EntityId); });
    for(const FInventoryDomainRecord& Record : Records)
    {
        if(!Restore(Record, OutError))
        {
            DetachRuntime();
            return false;
        }
    }
    return true;
}

void FVoxelInventoryDomainBridge::DetachRuntime()
{
    if(Runtime)
    {
        for(auto& Pair : Pending)
        {
            Runtime->CancelInventorySections(Pair.Value->Prepared);
        }
    }
    Pending.Empty();
    ContainerAnchors.Empty();
    Runtime = nullptr;
    Publisher = nullptr;
}

void FVoxelInventoryDomainBridge::SetPublisher(TFunction<void(const FVoxelEditBatch&)> InPublisher)
{
    Publisher = MoveTemp(InPublisher);
}

bool FVoxelInventoryDomainBridge::BindContainer(const FGuid& InInventoryId, const FIntVector& InAnchor)
{
    if(!InInventoryId.IsValid())
    {
        return false;
    }
    for(const auto& Existing : ContainerAnchors)
    {
        if((Existing.Key == InInventoryId && Existing.Value != InAnchor) ||
            (Existing.Key != InInventoryId && Existing.Value == InAnchor))
        {
            return false;
        }
    }
    ContainerAnchors.Add(InInventoryId, InAnchor);
    return true;
}

void FVoxelInventoryDomainBridge::UnbindContainer(const FGuid& InInventoryId)
{
    ContainerAnchors.Remove(InInventoryId);
}

FGuid FVoxelInventoryDomainBridge::FindInventoryAt(const FIntVector& InAnchor) const
{
    for(const auto& Pair : ContainerAnchors)
    {
        if(Pair.Value == InAnchor)
        {
            return Pair.Key;
        }
    }
    return {};
}

bool FVoxelInventoryDomainBridge::ReadCurrent(const FIntVector& InSection, FVoxelInventorySectionTarget& OutTarget, FString& OutError) const
{
    if(!Runtime)
    {
        OutError = TEXT("Inventory.VoxelWorldNotAttached");
        return false;
    }
    FVoxelInventorySectionTarget Target;
    Target.After.Section = InSection;
    if(const FVoxelSection* Loaded = Runtime->FindSection(InSection); Loaded && Loaded->Status == EVoxelSectionStatus::DataReady)
    {
        Target.ExpectedRevision = Loaded->CommittedRevision;
        Target.After.Blocks = Loaded->Overlay;
        Target.After.Entities = Loaded->Entities;
    }
    else if(const auto Journal = Runtime->FindCommittedOverlay(InSection))
    {
        Target.ExpectedRevision = Journal->Revision;
        Target.After.Blocks = Journal->Blocks;
        Target.After.Entities = Journal->Entities;
    }
    else
    {
        OutError = TEXT("Inventory.VoxelSectionNeedsData");
        return false;
    }
    if(Target.ExpectedRevision >= MAX_uint64 - 1)
    {
        OutError = TEXT("Inventory.VoxelRevisionExhausted");
        return false;
    }
    Target.After.Revision = Target.ExpectedRevision + 1;
    OutTarget = MoveTemp(Target);
    return true;
}

bool FVoxelInventoryDomainBridge::PutTarget(const FVoxelInventorySectionTarget& InTarget, TArray<FInventoryDomainRedo>& InOutChanges, FString& OutError) const
{
    FInventoryDomainRedo Redo;
    Redo.Domain = GetDomain();
    Redo.EntityId = SectionId(InTarget.After.Section);
    const FInventoryDomainRecord* Existing = System.GetRecords().Find(Redo.Domain, Redo.EntityId);
    if(Existing && Existing->bDeleted)
    {
        OutError = TEXT("Inventory.VoxelSectionRecordDeleted");
        return false;
    }
    Redo.ExpectedRevision = Existing ? Existing->Revision : 0;
    if(Redo.ExpectedRevision == MAX_uint64)
    {
        OutError = TEXT("Inventory.VoxelRecordRevisionExhausted");
        return false;
    }
    Redo.TargetRevision = Redo.ExpectedRevision + 1;
    if(!InventoryVoxelRecord::Encode(InTarget, Redo.AfterBytes, OutError))
    {
        return false;
    }
    if(FInventoryDomainRedo* PendingRedo = InOutChanges.FindByPredicate([&](const auto& Value) { return Value.Domain == Redo.Domain && Value.EntityId == Redo.EntityId; }))
    {
        *PendingRedo = MoveTemp(Redo);
    }
    else
    {
        InOutChanges.Add(MoveTemp(Redo));
    }
    return true;
}

bool FVoxelInventoryDomainBridge::AppendEdit(const TArray<FVoxelCellEdit>& InCells, const TArray<FVoxelEntityEdit>& InEntities,
    TArray<FInventoryDomainRedo>& InOutChanges, FString& OutError)
{
    if(!Runtime)
    {
        OutError = TEXT("Inventory.VoxelWorldNotAttached");
        return false;
    }
    FVoxelPreparedEdit Probe;
    if(!Runtime->PrepareEdit(InCells, InEntities, Probe, OutError))
    {
        return false;
    }
    for(const FVoxelPreparedSection& Section : Probe.Sections)
    {
        FVoxelInventorySectionTarget Target;
        const FGuid Id = SectionId(Section.Patch.Section);
        const FInventoryDomainRedo* Existing = InOutChanges.FindByPredicate([&](const auto& Value) { return Value.Domain == GetDomain() && Value.EntityId == Id; });
        if(Existing)
        {
            if(Existing->bDelete || !InventoryVoxelRecord::Decode(Existing->AfterBytes, Target, OutError) || Target.ExpectedRevision != Section.Patch.FromRevision)
            {
                OutError = TEXT("Inventory.VoxelMergedRevisionChanged");
                return false;
            }
            for(const auto& Edit : Section.Patch.Edits)
            {
                if(Edit.bNatural) Target.After.Blocks.Remove(Edit.CellIndex);
                else Target.After.Blocks.Add(Edit.CellIndex, Edit.State);
            }
            for(const auto& Edit : Section.Patch.Entities)
            {
                if(Edit.bRemove) Target.After.Entities.Remove(Edit.CellIndex);
                else Target.After.Entities.Add(Edit.CellIndex, Edit.Value);
            }
        }
        else
        {
            Target.ExpectedRevision = Section.Patch.FromRevision;
            Target.After.Section = Section.Patch.Section;
            Target.After.Revision = Section.Patch.ToRevision;
            Target.After.Blocks = Section.Overlay;
            Target.After.Entities = Section.Entities;
        }
        if(!PutTarget(Target, InOutChanges, OutError))
        {
            return false;
        }
    }
    return true;
}

bool FVoxelInventoryDomainBridge::ProjectInventories(const FInventoryPlan& InPlan, TArray<FInventoryDomainRedo>& InOutChanges, FString& OutError)
{
    // 新箱子的绑定只能在持久提交后安装。候选实体提供同批新建库存的位置，
    // 在 WAL 序列化前将初始快照替换为规划器最终快照，不污染运行时绑定表。
    TMap<FGuid, FIntVector> CandidateAnchors = ContainerAnchors;
    for(const FInventoryDomainRedo& Change : InOutChanges)
    {
        if(Change.Domain != GetDomain() || Change.bDelete) continue;
        FVoxelInventorySectionTarget Candidate;
        if(!InventoryVoxelRecord::Decode(Change.AfterBytes, Candidate, OutError)) return false;
        for(const auto& Pair : Candidate.After.Entities)
        {
            if(!FInventoryVoxelEntityCodec::IsInventory(Pair.Value) || FInventoryVoxelEntityCodec::IsUninitialized(Pair.Value)) continue;
            FInventorySnapshot Embedded;
            if(!FInventoryVoxelEntityCodec::Decode(Pair.Value, System.GetRegistry(), System.GetProfiles(), Embedded, OutError)) return false;
            const FIntVector Position = InventoryVoxelRecord::PositionOf(Candidate.After.Section, Pair.Key);
            if(const FIntVector* Bound = CandidateAnchors.Find(Embedded.InventoryId); Bound && *Bound != Position)
            {
                OutError = TEXT("Inventory.VoxelContainerIdentityDuplicated");
                return false;
            }
            CandidateAnchors.Add(Embedded.InventoryId, Position);
        }
    }
    for(const FInventorySnapshot& Snapshot : InPlan.After)
    {
        const FIntVector* Anchor = CandidateAnchors.Find(Snapshot.InventoryId);
        if(!Anchor) continue;
        FVoxelInventorySectionTarget Target;
        const FIntVector Section = InventoryVoxelRecord::SectionOf(*Anchor);
        const FGuid Id = SectionId(Section);
        const FInventoryDomainRedo* Existing = InOutChanges.FindByPredicate([&](const auto& Value) { return Value.Domain == GetDomain() && Value.EntityId == Id; });
        if(Existing ? (Existing->bDelete || !InventoryVoxelRecord::Decode(Existing->AfterBytes, Target, OutError)) : !ReadCurrent(Section, Target, OutError)) return false;
        const bool bEmpty = !Snapshot.Slots.ContainsByPredicate([](const auto& Slot) { return !Slot.Item.IsEmpty(); });
        FVoxelBlockEntityState* Entity = Target.After.Entities.Find(InventoryVoxelRecord::IndexOf(*Anchor));
        if(!Entity)
        {
            // 同批销毁的空箱子不能通过库存投影重新生成。
            if(bEmpty && Existing) continue;
            OutError = TEXT("Inventory.VoxelContainerEntityMissing");
            return false;
        }
        FInventorySnapshot Embedded;
        if(!FInventoryVoxelEntityCodec::Decode(*Entity, System.GetRegistry(), System.GetProfiles(), Embedded, OutError)) return false;
        if(Embedded.InventoryId != Snapshot.InventoryId)
        {
            // 同一坐标可在一批中移除旧空箱并创建新箱；旧库存不能覆盖新身份。
            if(bEmpty && Existing) continue;
            OutError = TEXT("Inventory.VoxelContainerIdentityMismatch");
            return false;
        }
        if(!FInventoryVoxelEntityCodec::Encode(Snapshot, System.GetRegistry(), System.GetProfiles(), *Entity, OutError) ||
            !PutTarget(Target, InOutChanges, OutError)) return false;
    }
    return true;
}

bool FVoxelInventoryDomainBridge::Prepare(const FGuid& InTransactionId, TConstArrayView<FInventoryDomainRedo> InChanges, FString& OutError)
{
    if(!Runtime || Pending.Contains(InTransactionId))
    {
        OutError = TEXT("Inventory.VoxelPrepareUnavailable");
        return false;
    }
    TSharedRef<FPending> Candidate = MakeShared<FPending>();
    TArray<FVoxelInventorySectionTarget> Targets;
    Targets.Reserve(InChanges.Num());
    Candidate->EntityIds.Reserve(InChanges.Num());
    for(const FInventoryDomainRedo& Change : InChanges)
    {
        FVoxelInventorySectionTarget Target;
        if(Change.Domain != GetDomain() || Change.bDelete || !InventoryVoxelRecord::Decode(Change.AfterBytes, Target, OutError) ||
            Change.EntityId != SectionId(Target.After.Section))
        {
            OutError = TEXT("Inventory.VoxelRedoInvalid");
            return false;
        }
        Targets.Add(MoveTemp(Target));
        Candidate->EntityIds.Add(Change.EntityId);
    }
    if(!Runtime->PrepareInventorySections(Targets, InTransactionId, false, Candidate->Prepared, OutError))
    {
        return false;
    }
    Pending.Add(InTransactionId, MoveTemp(Candidate));
    return true;
}

bool FVoxelInventoryDomainBridge::ValidatePrepared(const FGuid& InTransactionId, FString& OutError) const
{
    const auto* Found = Pending.Find(InTransactionId);
    return Runtime && Found && Found->IsValid() && Runtime->ValidateInventorySections((*Found)->Prepared, OutError);
}

bool FVoxelInventoryDomainBridge::ApplyCommitted(const FGuid& InTransactionId, const FInventoryDomainRedo& InRedo, FString& OutError)
{
    auto* Found = Pending.Find(InTransactionId);
    if(!Runtime || !Found || !Found->IsValid() || !(*Found)->EntityIds.Contains(InRedo.EntityId))
    {
        OutError = TEXT("Inventory.VoxelPreparedCommitMissing");
        return false;
    }
    FPending& Transaction = **Found;
    if(!Transaction.bApplied)
    {
        if(!Runtime->CommitInventorySections(Transaction.Prepared, Transaction.Committed, OutError))
        {
            return false;
        }
        Transaction.bApplied = true;
    }
    FVoxelInventorySectionTarget Target;
    return InventoryVoxelRecord::Decode(InRedo.AfterBytes, Target, OutError) && RestoreBindings(Target.After, OutError);
}

bool FVoxelInventoryDomainBridge::RestoreBindings(const FVoxelPersistentSection& InSection, FString& OutError)
{
    // 每个区段后状态完整替换本区派生绑定；同坐标拆旧建新不会遗留旧身份。
    TMap<FGuid, FIntVector> Candidate = ContainerAnchors;
    for(auto Iterator = Candidate.CreateIterator(); Iterator; ++Iterator)
    {
        if(InventoryVoxelRecord::SectionOf(Iterator.Value()) == InSection.Section) Iterator.RemoveCurrent();
    }
    for(const auto& Entity : InSection.Entities)
    {
        if(!FInventoryVoxelEntityCodec::IsInventory(Entity.Value) || FInventoryVoxelEntityCodec::IsUninitialized(Entity.Value))
        {
            continue;
        }
        FInventorySnapshot Embedded;
        FInventorySnapshot Current;
        if(!FInventoryVoxelEntityCodec::Decode(Entity.Value, System.GetRegistry(), System.GetProfiles(), Embedded, OutError) ||
            !System.GetSnapshot(Embedded.InventoryId, Current, OutError) || Embedded != Current)
        {
            OutError = TEXT("Inventory.VoxelEmbeddedSnapshotDiverged");
            return false;
        }
        const FIntVector Anchor = InventoryVoxelRecord::PositionOf(InSection.Section, Entity.Key);
        for(const auto& Existing : Candidate)
        {
            if((Existing.Key == Embedded.InventoryId && Existing.Value != Anchor) || (Existing.Key != Embedded.InventoryId && Existing.Value == Anchor))
            {
                OutError = TEXT("Inventory.VoxelContainerBindingConflict");
                return false;
            }
        }
        Candidate.Add(Embedded.InventoryId, Anchor);
    }
    ContainerAnchors = MoveTemp(Candidate);
    return true;
}

bool FVoxelInventoryDomainBridge::Restore(const FInventoryDomainRecord& InRecord, FString& OutError)
{
    FVoxelInventorySectionTarget Target;
    if(InRecord.Key.Domain != GetDomain() || InRecord.bDeleted || !InventoryVoxelRecord::Decode(InRecord.Bytes, Target, OutError) ||
        InRecord.Key.EntityId != SectionId(Target.After.Section) || !ValidateRecord(InRecord.Bytes, OutError))
    {
        return false;
    }
    // OpenStore 时体素 Runtime 可以尚未创建；中央记录已完整恢复，Attach 再按最终记录物化。
    if(!Runtime)
    {
        return true;
    }
    return RestoreBindings(Target.After, OutError) && Runtime->RestoreInventorySection(Target.After, OutError);
}

void FVoxelInventoryDomainBridge::Publish(const FGuid& InTransactionId)
{
    auto* Found = Pending.Find(InTransactionId);
    if(Found && Found->IsValid() && (*Found)->bApplied)
    {
        // 发布回调可能解除整个桥接；先移出批次，回调后不再借用 Pending 元素。
        FVoxelEditBatch Batch = MoveTemp((*Found)->Committed);
        const auto Callback = Publisher;
        FVoxelWorldRuntime* PublishingRuntime = Runtime;
        if(PublishingRuntime)
        {
            PublishingRuntime->PublishInventorySections(Batch);
        }
        if(Runtime == PublishingRuntime && Callback)
        {
            Callback(Batch);
        }
    }
    Cancel(InTransactionId);
}

void FVoxelInventoryDomainBridge::Cancel(const FGuid& InTransactionId)
{
    TSharedPtr<FPending> Transaction;
    if(Pending.RemoveAndCopyValue(InTransactionId, Transaction) && Runtime)
    {
        Runtime->CancelInventorySections(Transaction->Prepared);
    }
}
