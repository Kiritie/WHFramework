#include "Ability/Inventory/InventoryArchiveSnapshot.h"

#include "Ability/Inventory/InventoryBinaryArchive.h"
#include "Ability/Inventory/InventoryJournal.h"
#include "Misc/Crc.h"

bool FInventoryArchiveSnapshot::Validate(FString& OutError) const
{
    if(!WorldId.IsValid() || !StoreId.IsValid() || CheckpointBytes.IsEmpty() || CheckpointBytes.Num() > MaximumCheckpointBytes)
    {
        OutError = TEXT("Inventory.ArchiveSnapshotInvalid");
        return false;
    }
    FInventoryCheckpoint Checkpoint;
    TArray<FInventoryJournalRecord> Empty;
    if(!FInventoryJournal::DecodeCheckpoint(CheckpointBytes, Checkpoint, OutError) || Checkpoint.WorldId != WorldId ||
       Checkpoint.Sequence != Sequence || !FInventoryJournal::FoldRecords(Checkpoint, Empty, OutError))
    {
        if(OutError.IsEmpty()) OutError = TEXT("Inventory.ArchiveSnapshotIdentityMismatch");
        return false;
    }
    return true;
}

bool FInventoryArchiveSnapshot::Encode(TArray<uint8>& OutBytes, FString& OutError) const
{
    if(!Validate(OutError)) return false;
    FInventoryBinaryWriter Writer(MaximumBytes);
    Writer.U32(0x33524149);
    Writer.U32(1);
    Writer.Guid(WorldId);
    Writer.Guid(StoreId);
    Writer.U64(Sequence);
    Writer.Blob(CheckpointBytes, MaximumCheckpointBytes);
    Writer.U32(FCrc::MemCrc32(Writer.GetBytes().GetData(), Writer.GetBytes().Num()));
    if(!Writer.IsValid())
    {
        OutError = TEXT("Inventory.ArchiveSnapshotSizeLimit");
        return false;
    }
    OutBytes = Writer.TakeBytes();
    return true;
}

bool FInventoryArchiveSnapshot::Decode(TConstArrayView<uint8> InBytes, FInventoryArchiveSnapshot& OutSnapshot, FString& OutError)
{
    if(InBytes.Num() < 56 || InBytes.Num() > MaximumBytes)
    {
        OutError = TEXT("Inventory.ArchiveSnapshotSizeLimit");
        return false;
    }
    FInventoryBinaryReader Checksum(InBytes.Slice(InBytes.Num() - 4, 4));
    if(Checksum.U32() != FCrc::MemCrc32(InBytes.GetData(), InBytes.Num() - 4))
    {
        OutError = TEXT("Inventory.ArchiveSnapshotChecksum");
        return false;
    }
    FInventoryBinaryReader Reader(InBytes.Slice(0, InBytes.Num() - 4));
    if(Reader.U32() != 0x33524149 || Reader.U32() != 1)
    {
        OutError = TEXT("Inventory.ArchiveSnapshotSchema");
        return false;
    }
    FInventoryArchiveSnapshot Snapshot;
    Snapshot.WorldId = Reader.Guid();
    Snapshot.StoreId = Reader.Guid();
    Snapshot.Sequence = Reader.U64();
    Snapshot.CheckpointBytes = Reader.Blob(MaximumCheckpointBytes);
    if(!Reader.AtEnd() || !Snapshot.Validate(OutError))
    {
        if(OutError.IsEmpty()) OutError = TEXT("Inventory.ArchiveSnapshotMalformed");
        return false;
    }
    OutSnapshot = MoveTemp(Snapshot);
    return true;
}

FInventoryPreparedStore::FInventoryPreparedStore()
{
}

FInventoryPreparedStore::~FInventoryPreparedStore()
{
    Close();
}

FGuid FInventoryPreparedStore::GetWorldId() const
{
    return WorldId;
}

FGuid FInventoryPreparedStore::GetStoreId() const
{
    return StoreId;
}

FGuid FInventoryPreparedStore::GetSourceStoreId() const
{
    return SourceStoreId;
}

FGuid FInventoryPreparedStore::GetOperationId() const
{
    return OperationId;
}

uint64 FInventoryPreparedStore::GetSequence() const
{
    return Sequence;
}

const FString& FInventoryPreparedStore::GetDirectory() const
{
    return Directory;
}

bool FInventoryPreparedStore::IsReady() const
{
    return !bAdopted && Journal && Journal->IsHealthy() && Journal->GetStoreId() == StoreId && Journal->GetSequence() == Sequence;
}

bool FInventoryPreparedStore::IsCommitted() const
{
    return bCommitted;
}

bool FInventoryPreparedStore::IsAdopted() const
{
    return bAdopted;
}

bool FInventoryPreparedStore::MarkCommitted(FString& OutError)
{
    if(bCommitted) return IsReady() || bAdopted;
    if(!IsReady() || !Journal->SealPrepared(OperationId, OutError))
    {
        if(OutError.IsEmpty()) OutError = TEXT("Inventory.PreparedStoreNotReady");
        return false;
    }
    bCommitted = true;
    return true;
}

void FInventoryPreparedStore::Close()
{
    Journal.Reset();
}
