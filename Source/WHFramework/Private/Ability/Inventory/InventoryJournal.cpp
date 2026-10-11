#include "Ability/Inventory/InventoryJournal.h"

#include "Ability/Inventory/InventoryBinaryArchive.h"
#include "Ability/Inventory/InventorySnapshotCodec.h"
#include "Ability/Inventory/InventoryStoreLock.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/Crc.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#elif PLATFORM_LINUX
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#endif

namespace InventoryJournalFormat
{
    constexpr uint32 RecordMagic = 0x324a5749;
    constexpr uint32 RecordEnd = 0x32444e45;
    constexpr uint32 CheckpointMagic = 0x32504b43;
    constexpr uint32 ManifestMagic = 0x3354464d;
    constexpr uint32 ForkOwnerMagic = 0x334f4649;
    constexpr uint32 ForkReadyMagic = 0x33524649;
    constexpr int32 MaximumDomainBytes = 16 * 1024 * 1024;
    constexpr uint32 MaximumEntries = 2000000;

    struct FForkIdentity
    {
        FGuid WorldId;
        FGuid StoreId;
        FGuid SourceStoreId;
        FGuid OperationId;
        uint64 Sequence = 0;
        uint32 CheckpointCrc = 0;

        bool operator==(const FForkIdentity& InOther) const
        {
            return WorldId == InOther.WorldId && StoreId == InOther.StoreId && SourceStoreId == InOther.SourceStoreId &&
                OperationId == InOther.OperationId && Sequence == InOther.Sequence && CheckpointCrc == InOther.CheckpointCrc;
        }
    };

    void WriteCheckedBlob(FInventoryBinaryWriter& InWriter, TConstArrayView<uint8> InBytes, int32 InLimit)
    {
        InWriter.Blob(InBytes, InLimit);
        InWriter.U32(FCrc::MemCrc32(InBytes.GetData(), InBytes.Num()));
    }

    TArray<uint8> ReadCheckedBlob(FInventoryBinaryReader& InReader, int32 InLimit)
    {
        TArray<uint8> Bytes = InReader.Blob(InLimit);
        const uint32 Checksum = InReader.U32();
        if(!InReader.IsValid() || Checksum != FCrc::MemCrc32(Bytes.GetData(), Bytes.Num()))
        {
            InReader.Reject();
        }
        return Bytes;
    }

    void WriteDomain(FInventoryBinaryWriter& InWriter, const FInventoryDomainRecord& InRecord)
    {
        InWriter.Name(InRecord.Key.Domain);
        InWriter.Guid(InRecord.Key.EntityId);
        InWriter.U64(InRecord.Revision);
        InWriter.U8(InRecord.bDeleted ? 1 : 0);
        WriteCheckedBlob(InWriter, InRecord.Bytes, MaximumDomainBytes);
    }

    FInventoryDomainRecord ReadDomain(FInventoryBinaryReader& InReader)
    {
        FInventoryDomainRecord Record;
        Record.Key.Domain = InReader.Name();
        Record.Key.EntityId = InReader.Guid();
        Record.Revision = InReader.U64();
        const uint8 Deleted = InReader.U8();
        Record.bDeleted = Deleted == 1;
        Record.Bytes = ReadCheckedBlob(InReader, MaximumDomainBytes);
        if(Deleted > 1 || Record.Key.Domain.IsNone() || !Record.Key.EntityId.IsValid() || Record.Revision == 0 ||
           (Record.bDeleted && !Record.Bytes.IsEmpty()))
        {
            InReader.Reject();
        }
        return Record;
    }

    bool VerifyChecksum(TConstArrayView<uint8> InBytes)
    {
        if(InBytes.Num() < 4)
        {
            return false;
        }
        FInventoryBinaryReader Reader(InBytes.Slice(InBytes.Num() - 4, 4));
        return Reader.U32() == FCrc::MemCrc32(InBytes.GetData(), InBytes.Num() - 4);
    }

    bool DecodeForkIdentity(TConstArrayView<uint8> InBytes, uint32 InMagic, FForkIdentity& OutIdentity)
    {
        if(InBytes.Num() != 88 || !VerifyChecksum(InBytes)) return false;
        FInventoryBinaryReader Reader(InBytes.Slice(0, InBytes.Num() - 4));
        if(Reader.U32() != InMagic || Reader.U32() != 1) return false;
        FForkIdentity Identity;
        Identity.WorldId = Reader.Guid();
        Identity.StoreId = Reader.Guid();
        Identity.SourceStoreId = Reader.Guid();
        Identity.OperationId = Reader.Guid();
        Identity.Sequence = Reader.U64();
        Identity.CheckpointCrc = Reader.U32();
        if(!Reader.AtEnd() || !Identity.WorldId.IsValid() || !Identity.StoreId.IsValid() || !Identity.SourceStoreId.IsValid() ||
           !Identity.OperationId.IsValid() || Identity.StoreId == Identity.SourceStoreId) return false;
        OutIdentity = Identity;
        return true;
    }

    TArray<uint8> EncodeForkIdentity(const FForkIdentity& InIdentity, uint32 InMagic)
    {
        FInventoryBinaryWriter Writer(128);
        Writer.U32(InMagic);
        Writer.U32(1);
        Writer.Guid(InIdentity.WorldId);
        Writer.Guid(InIdentity.StoreId);
        Writer.Guid(InIdentity.SourceStoreId);
        Writer.Guid(InIdentity.OperationId);
        Writer.U64(InIdentity.Sequence);
        Writer.U32(InIdentity.CheckpointCrc);
        Writer.U32(FCrc::MemCrc32(Writer.GetBytes().GetData(), Writer.GetBytes().Num()));
        return Writer.TakeBytes();
    }

    bool CreateExclusiveDirectory(const FString& InDirectory)
    {
#if PLATFORM_WINDOWS
        return ::CreateDirectoryW(*InDirectory, nullptr) != 0;
#elif PLATFORM_LINUX
        const FTCHARToUTF8 Path(*InDirectory);
        return ::mkdir(Path.Get(), 0700) == 0;
#else
        return false;
#endif
    }
}

FInventoryJournal::FInventoryJournal()
{
}

FInventoryJournal::~FInventoryJournal()
{
    Close();
}

bool FInventoryJournal::AcquireStoreLock(FString& OutError)
{
    StoreLock = FInventoryStoreLock::Acquire(Directory, OutError);
    return StoreLock != -1;
}

void FInventoryJournal::ReleaseStoreLock()
{
    if(StoreLock == -1)
    {
        return;
    }
    FInventoryStoreLock::Release(StoreLock);
    StoreLock = -1;
}

bool FInventoryJournal::EncodeRecord(const FInventoryJournalRecord& InRecord, TArray<uint8>& OutBytes, FString& OutError)
{
    if(!InRecord.WorldId.IsValid() || !InRecord.TransactionId.IsValid() || InRecord.Sequence == 0 ||
       InRecord.Inventories.Num() > 64 || InRecord.Domains.Num() > 256 ||
       (InRecord.Inventories.IsEmpty() && InRecord.Domains.IsEmpty()))
    {
        OutError = TEXT("Inventory.JournalRecordInvalid");
        return false;
    }
    FInventoryBinaryWriter Payload(MaximumRecordBytes - 20);
    Payload.Guid(InRecord.WorldId);
    Payload.Guid(InRecord.TransactionId);
    Payload.U64(InRecord.Sequence);
    Payload.U32(static_cast<uint32>(InRecord.Inventories.Num()));
    TSet<FGuid> InventoryIds;
    for(const FInventoryJournalChange& Change : InRecord.Inventories)
    {
        if(!Change.InventoryId.IsValid() || InventoryIds.Contains(Change.InventoryId) || Change.BeforeRevision == MAX_uint64 || Change.AfterRevision != Change.BeforeRevision + 1 ||
           Change.Generation == 0 || (Change.bCreated && (Change.bDeleted || Change.BeforeRevision != 0)) ||
           (Change.bDeleted ? !Change.AfterBytes.IsEmpty() : Change.AfterBytes.IsEmpty()))
        {
            OutError = TEXT("Inventory.JournalInventoryChangeInvalid");
            return false;
        }
        InventoryIds.Add(Change.InventoryId);
        Payload.Guid(Change.InventoryId);
        Payload.U64(Change.BeforeRevision);
        Payload.U64(Change.AfterRevision);
        Payload.U32(Change.Generation);
        Payload.U8((Change.bCreated ? 1 : 0) | (Change.bDeleted ? 2 : 0));
        InventoryJournalFormat::WriteCheckedBlob(Payload, Change.AfterBytes, FInventorySnapshotCodec::MaximumBytes);
    }
    Payload.U32(static_cast<uint32>(InRecord.Domains.Num()));
    TSet<FInventoryRecordKey> DomainKeys;
    for(const FInventoryDomainRedo& Change : InRecord.Domains)
    {
        const FInventoryRecordKey Key{Change.Domain, Change.EntityId};
        if(Change.Domain.IsNone() || !Change.EntityId.IsValid() || DomainKeys.Contains(Key) ||
           Change.ExpectedRevision == MAX_uint64 || Change.TargetRevision != Change.ExpectedRevision + 1 ||
           Change.AfterBytes.Num() > InventoryJournalFormat::MaximumDomainBytes || (Change.bDelete && !Change.AfterBytes.IsEmpty()))
        {
            OutError = TEXT("Inventory.JournalDomainChangeInvalid");
            return false;
        }
        DomainKeys.Add(Key);
        Payload.U64(Change.ExpectedRevision);
        FInventoryDomainRecord After;
        After.Key = FInventoryRecordKey{Change.Domain, Change.EntityId};
        After.Revision = Change.TargetRevision;
        After.bDeleted = Change.bDelete;
        After.Bytes = Change.AfterBytes;
        InventoryJournalFormat::WriteDomain(Payload, After);
    }
    FInventoryBinaryWriter Frame(MaximumRecordBytes);
    Frame.U32(InventoryJournalFormat::RecordMagic);
    Frame.U32(2);
    Frame.U32(static_cast<uint32>(Payload.GetBytes().Num()));
    Frame.Raw(Payload.GetBytes());
    Frame.U32(FCrc::MemCrc32(Payload.GetBytes().GetData(), Payload.GetBytes().Num()));
    Frame.U32(InventoryJournalFormat::RecordEnd);
    if(!Payload.IsValid() || !Frame.IsValid())
    {
        OutError = TEXT("Inventory.JournalRecordSizeLimit");
        return false;
    }
    OutBytes = Frame.TakeBytes();
    return true;
}

bool FInventoryJournal::DecodeRecord(TConstArrayView<uint8> InBytes, FInventoryJournalRecord& OutRecord, FString& OutError)
{
    FInventoryBinaryReader Reader(InBytes);
    FInventoryJournalRecord Record;
    Record.WorldId = Reader.Guid();
    Record.TransactionId = Reader.Guid();
    Record.Sequence = Reader.U64();
    const uint32 InventoryCount = Reader.U32();
    if(!Reader.IsValid() || InventoryCount > 64 || !Record.WorldId.IsValid() || !Record.TransactionId.IsValid() || Record.Sequence == 0)
    {
        OutError = TEXT("Inventory.JournalRecordMalformed");
        return false;
    }
    TSet<FGuid> InventoryIds;
    for(uint32 ChangeIndex = 0; ChangeIndex < InventoryCount && Reader.IsValid(); ++ChangeIndex)
    {
        FInventoryJournalChange Change;
        Change.InventoryId = Reader.Guid();
        Change.BeforeRevision = Reader.U64();
        Change.AfterRevision = Reader.U64();
        Change.Generation = Reader.U32();
        const uint8 Flags = Reader.U8();
        Change.bCreated = (Flags & 1) != 0;
        Change.bDeleted = (Flags & 2) != 0;
        Change.AfterBytes = InventoryJournalFormat::ReadCheckedBlob(Reader, FInventorySnapshotCodec::MaximumBytes);
        if(Flags > 2 || !Change.InventoryId.IsValid() || InventoryIds.Contains(Change.InventoryId) || Change.Generation == 0 ||
           Change.BeforeRevision == MAX_uint64 || Change.AfterRevision != Change.BeforeRevision + 1 ||
           (Change.bCreated && Change.BeforeRevision != 0) || (Change.bDeleted && !Change.AfterBytes.IsEmpty()) ||
           (!Change.bDeleted && Change.AfterBytes.IsEmpty()))
        {
            Reader.Reject();
        }
        InventoryIds.Add(Change.InventoryId);
        Record.Inventories.Add(MoveTemp(Change));
    }
    const uint32 DomainCount = Reader.U32();
    if(DomainCount > 256 || (InventoryCount == 0 && DomainCount == 0))
    {
        Reader.Reject();
    }
    TSet<FInventoryRecordKey> DomainKeys;
    for(uint32 ChangeIndex = 0; ChangeIndex < DomainCount && Reader.IsValid(); ++ChangeIndex)
    {
        FInventoryDomainRedo Redo;
        Redo.ExpectedRevision = Reader.U64();
        FInventoryDomainRecord After = InventoryJournalFormat::ReadDomain(Reader);
        Redo.Domain = After.Key.Domain;
        Redo.EntityId = After.Key.EntityId;
        Redo.TargetRevision = After.Revision;
        Redo.bDelete = After.bDeleted;
        Redo.AfterBytes = MoveTemp(After.Bytes);
        if(Redo.ExpectedRevision == MAX_uint64 || Redo.TargetRevision != Redo.ExpectedRevision + 1 || DomainKeys.Contains(After.Key))
        {
            Reader.Reject();
        }
        DomainKeys.Add(After.Key);
        Record.Domains.Add(MoveTemp(Redo));
    }
    if(!Reader.AtEnd())
    {
        OutError = TEXT("Inventory.JournalRecordMalformed");
        return false;
    }
    OutRecord = MoveTemp(Record);
    return true;
}

bool FInventoryJournal::EncodeCheckpoint(const FInventoryCheckpoint& InCheckpoint, TArray<uint8>& OutBytes, FString& OutError)
{
    if(!InCheckpoint.WorldId.IsValid() || InCheckpoint.Inventories.Num() > InventoryJournalFormat::MaximumEntries ||
       InCheckpoint.Tombstones.Num() > InventoryJournalFormat::MaximumEntries || InCheckpoint.Domains.Num() > InventoryJournalFormat::MaximumEntries)
    {
        OutError = TEXT("Inventory.CheckpointSizeLimit");
        return false;
    }
    FInventoryBinaryWriter Writer(MaximumCheckpointBytes);
    Writer.U32(InventoryJournalFormat::CheckpointMagic);
    Writer.U32(2);
    Writer.Guid(InCheckpoint.WorldId);
    Writer.U64(InCheckpoint.Sequence);
    TArray<FGuid> InventoryIds;
    InCheckpoint.Inventories.GetKeys(InventoryIds);
    InventoryIds.Sort(InventoryOrder::GuidLess);
    Writer.U32(static_cast<uint32>(InventoryIds.Num()));
    for(const FGuid& InventoryId : InventoryIds)
    {
        if(!InventoryId.IsValid() || InCheckpoint.Inventories.FindChecked(InventoryId).IsEmpty())
        {
            OutError = TEXT("Inventory.CheckpointInventoryInvalid");
            return false;
        }
        Writer.Guid(InventoryId);
        InventoryJournalFormat::WriteCheckedBlob(Writer, InCheckpoint.Inventories.FindChecked(InventoryId), FInventorySnapshotCodec::MaximumBytes);
    }
    InCheckpoint.Tombstones.GetKeys(InventoryIds);
    InventoryIds.Sort(InventoryOrder::GuidLess);
    Writer.U32(static_cast<uint32>(InventoryIds.Num()));
    for(const FGuid& InventoryId : InventoryIds)
    {
        const FInventoryTombstone& Tombstone = InCheckpoint.Tombstones.FindChecked(InventoryId);
        if(!InventoryId.IsValid() || InCheckpoint.Inventories.Contains(InventoryId) || Tombstone.Generation == 0 || Tombstone.Revision == 0)
        {
            OutError = TEXT("Inventory.CheckpointTombstoneInvalid");
            return false;
        }
        Writer.Guid(InventoryId);
        Writer.U32(Tombstone.Generation);
        Writer.U64(Tombstone.Revision);
    }
    TArray<FInventoryDomainRecord> Domains = InCheckpoint.Domains;
    Domains.Sort([](const FInventoryDomainRecord& InLeft, const FInventoryDomainRecord& InRight)
    {
        return InLeft.Key.Domain == InRight.Key.Domain ? InventoryOrder::GuidLess(InLeft.Key.EntityId, InRight.Key.EntityId) : InLeft.Key.Domain.LexicalLess(InRight.Key.Domain);
    });
    Writer.U32(static_cast<uint32>(Domains.Num()));
    TSet<FInventoryRecordKey> DomainKeys;
    for(const FInventoryDomainRecord& Domain : Domains)
    {
        if(Domain.Key.Domain.IsNone() || !Domain.Key.EntityId.IsValid() || Domain.Revision == 0 || DomainKeys.Contains(Domain.Key) ||
           Domain.Bytes.Num() > InventoryJournalFormat::MaximumDomainBytes || (Domain.bDeleted && !Domain.Bytes.IsEmpty()))
        {
            OutError = TEXT("Inventory.CheckpointDomainInvalid");
            return false;
        }
        DomainKeys.Add(Domain.Key);
        InventoryJournalFormat::WriteDomain(Writer, Domain);
    }
    Writer.U32(FCrc::MemCrc32(Writer.GetBytes().GetData(), Writer.GetBytes().Num()));
    if(!Writer.IsValid())
    {
        OutError = TEXT("Inventory.CheckpointSizeLimit");
        return false;
    }
    OutBytes = Writer.TakeBytes();
    return true;
}

bool FInventoryJournal::DecodeCheckpoint(TConstArrayView<uint8> InBytes, FInventoryCheckpoint& OutCheckpoint, FString& OutError)
{
    if(InBytes.Num() > MaximumCheckpointBytes || !InventoryJournalFormat::VerifyChecksum(InBytes))
    {
        OutError = TEXT("Inventory.CheckpointChecksum");
        return false;
    }
    FInventoryBinaryReader Reader(InBytes.Slice(0, InBytes.Num() - 4));
    if(Reader.U32() != InventoryJournalFormat::CheckpointMagic || Reader.U32() != 2)
    {
        OutError = TEXT("Inventory.UnsupportedCheckpointSchema");
        return false;
    }
    FInventoryCheckpoint Checkpoint;
    Checkpoint.WorldId = Reader.Guid();
    Checkpoint.Sequence = Reader.U64();
    const uint32 InventoryCount = Reader.U32();
    if(InventoryCount > InventoryJournalFormat::MaximumEntries)
    {
        Reader.Reject();
    }
    for(uint32 EntryIndex = 0; EntryIndex < InventoryCount && Reader.IsValid(); ++EntryIndex)
    {
        const FGuid InventoryId = Reader.Guid();
        TArray<uint8> Bytes = InventoryJournalFormat::ReadCheckedBlob(Reader, FInventorySnapshotCodec::MaximumBytes);
        if(!InventoryId.IsValid() || Checkpoint.Inventories.Contains(InventoryId) || Bytes.IsEmpty())
        {
            Reader.Reject();
        }
        Checkpoint.Inventories.Add(InventoryId, MoveTemp(Bytes));
    }
    const uint32 TombstoneCount = Reader.U32();
    if(TombstoneCount > InventoryJournalFormat::MaximumEntries)
    {
        Reader.Reject();
    }
    for(uint32 EntryIndex = 0; EntryIndex < TombstoneCount && Reader.IsValid(); ++EntryIndex)
    {
        const FGuid InventoryId = Reader.Guid();
        FInventoryTombstone Tombstone;
        Tombstone.Generation = Reader.U32();
        Tombstone.Revision = Reader.U64();
        if(!InventoryId.IsValid() || Tombstone.Generation == 0 || Tombstone.Revision == 0 ||
           Checkpoint.Inventories.Contains(InventoryId) || Checkpoint.Tombstones.Contains(InventoryId))
        {
            Reader.Reject();
        }
        Checkpoint.Tombstones.Add(InventoryId, Tombstone);
    }
    const uint32 DomainCount = Reader.U32();
    if(DomainCount > InventoryJournalFormat::MaximumEntries)
    {
        Reader.Reject();
    }
    TSet<FInventoryRecordKey> Keys;
    for(uint32 EntryIndex = 0; EntryIndex < DomainCount && Reader.IsValid(); ++EntryIndex)
    {
        FInventoryDomainRecord Record = InventoryJournalFormat::ReadDomain(Reader);
        if(Keys.Contains(Record.Key))
        {
            Reader.Reject();
        }
        Keys.Add(Record.Key);
        Checkpoint.Domains.Add(MoveTemp(Record));
    }
    if(!Reader.AtEnd() || !Checkpoint.WorldId.IsValid())
    {
        OutError = TEXT("Inventory.CheckpointMalformed");
        return false;
    }
    OutCheckpoint = MoveTemp(Checkpoint);
    return true;
}

bool FInventoryJournal::ReadFile(const FString& InPath, int64 InLimit, TArray<uint8>& OutBytes, FString& OutError)
{
    IPlatformFile& Platform = FPlatformFileManager::Get().GetPlatformFile();
    TUniquePtr<IFileHandle> Handle(Platform.OpenRead(*InPath));
    if(!Handle || Handle->Size() < 0 || Handle->Size() > InLimit || Handle->Size() > MAX_int32)
    {
        OutError = TEXT("Inventory.StorageReadFailed");
        return false;
    }
    TArray<uint8> Bytes;
    Bytes.SetNumUninitialized(static_cast<int32>(Handle->Size()));
    if(!Bytes.IsEmpty() && !Handle->Read(Bytes.GetData(), Bytes.Num()))
    {
        OutError = TEXT("Inventory.StorageReadFailed");
        return false;
    }
    OutBytes = MoveTemp(Bytes);
    return true;
}

bool FInventoryJournal::WriteDurable(const FString& InPath, TConstArrayView<uint8> InBytes, bool bInAppend, FString& OutError)
{
    IPlatformFile& Platform = FPlatformFileManager::Get().GetPlatformFile();
    TUniquePtr<IFileHandle> Handle(Platform.OpenWrite(*InPath, bInAppend, false));
    if(!Handle || (!InBytes.IsEmpty() && !Handle->Write(InBytes.GetData(), InBytes.Num())) || !Handle->Flush(true))
    {
        OutError = TEXT("Inventory.StorageDurableWriteFailed");
        return false;
    }
    return true;
}

bool FInventoryJournal::SyncDirectory(const FString& InDirectory, FString& OutError)
{
#if PLATFORM_LINUX
    const FTCHARToUTF8 Path(*InDirectory);
    const int Descriptor = ::open(Path.Get(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if(Descriptor < 0)
    {
        OutError = TEXT("Inventory.DirectoryOpenFailed");
        return false;
    }
    const int Result = ::fsync(Descriptor);
    ::close(Descriptor);
    if(Result != 0)
    {
        OutError = TEXT("Inventory.DirectorySyncFailed");
        return false;
    }
    return true;
#elif PLATFORM_WINDOWS
    return true;
#else
    OutError = TEXT("Inventory.UnsupportedDurabilityPlatform");
    return false;
#endif
}

bool FInventoryJournal::ReplaceDurable(const FString& InTemporary, const FString& InTarget, FString& OutError)
{
#if PLATFORM_WINDOWS
    if(!::MoveFileExW(*InTemporary, *InTarget, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        OutError = TEXT("Inventory.AtomicReplaceFailed");
        return false;
    }
    return true;
#elif PLATFORM_LINUX
    const FTCHARToUTF8 Temporary(*InTemporary);
    const FTCHARToUTF8 Target(*InTarget);
    if(::rename(Temporary.Get(), Target.Get()) != 0)
    {
        OutError = TEXT("Inventory.AtomicReplaceFailed");
        return false;
    }
    return SyncDirectory(FPaths::GetPath(InTarget), OutError);
#else
    OutError = TEXT("Inventory.UnsupportedDurabilityPlatform");
    return false;
#endif
}

bool FInventoryJournal::Open(const FString& InDirectory, const FGuid& InWorldId, const FGuid& InStoreId, bool bInCreateNew,
    FInventoryCheckpoint& OutCheckpoint, TArray<FInventoryJournalRecord>& OutRecords, FString& OutError)
{
    return OpenInternal(InDirectory, InWorldId, InStoreId, bInCreateNew, true, OutCheckpoint, OutRecords, OutError);
}

bool FInventoryJournal::OpenInternal(const FString& InDirectory, const FGuid& InWorldId, const FGuid& InStoreId, bool bInCreateNew,
    bool bInRepairTail, FInventoryCheckpoint& OutCheckpoint, TArray<FInventoryJournalRecord>& OutRecords, FString& OutError)
{
    if(bHealthy || StoreLock != -1 || !InWorldId.IsValid() || !InStoreId.IsValid() || InDirectory.IsEmpty())
    {
        OutError = TEXT("Inventory.JournalOpenInvalid");
        return false;
    }
#if !PLATFORM_WINDOWS && !PLATFORM_LINUX
    OutError = TEXT("Inventory.UnsupportedDurabilityPlatform");
    return false;
#endif
    Directory = FPaths::ConvertRelativePathToFull(InDirectory);
    FPaths::NormalizeDirectoryName(Directory);
    WorldId = InWorldId;
    StoreId = InStoreId;
    Sequence = 0;
    SaveGeneration = 0;
    JournalBytes = 0;
    bool bOpened = false;
    ON_SCOPE_EXIT
    {
        if(!bOpened)
        {
            Close();
        }
    };
    IPlatformFile& Platform = FPlatformFileManager::Get().GetPlatformFile();
    if(bInCreateNew)
    {
        TArray<FString> Existing;
        IFileManager::Get().FindFiles(Existing, *(Directory / TEXT("*")), true, true);
        if(!Existing.IsEmpty() || !Platform.CreateDirectoryTree(*Directory))
        {
            OutError = TEXT("Inventory.NewStoreRequiresEmptyDirectory");
            return false;
        }
        if(!SyncDirectory(FPaths::GetPath(Directory), OutError))
        {
            return false;
        }
        if(!AcquireStoreLock(OutError))
        {
            return false;
        }
        const TArray<uint8> Empty;
        const FString TemporaryJournal = Directory / TEXT("journal.new");
        if(!WriteDurable(TemporaryJournal, Empty, false, OutError) || !ReplaceDurable(TemporaryJournal, Directory / TEXT("journal.bin"), OutError))
        {
            return false;
        }
        bHealthy = true;
        FInventoryCheckpoint Initial;
        Initial.WorldId = WorldId;
        if(!Checkpoint(Initial, OutError))
        {
            bHealthy = false;
            return false;
        }
        OutCheckpoint = MoveTemp(Initial);
        OutRecords.Reset();
        bOpened = true;
        return true;
    }
    if(!AcquireStoreLock(OutError))
    {
        return false;
    }
    if(!VerifyPreparedReady(OutError)) return false;
    TArray<uint8> ManifestBytes;
    if(!ReadFile(Directory / TEXT("manifest.bin"), 128, ManifestBytes, OutError) || !InventoryJournalFormat::VerifyChecksum(ManifestBytes))
    {
        OutError = TEXT("Inventory.ManifestMissingOrCorrupt");
        return false;
    }
    FInventoryBinaryReader Manifest(MakeArrayView(ManifestBytes).Slice(0, ManifestBytes.Num() - 4));
    if(Manifest.U32() != InventoryJournalFormat::ManifestMagic || Manifest.U32() != 3 || Manifest.Guid() != WorldId || Manifest.Guid() != StoreId)
    {
        OutError = TEXT("Inventory.ManifestWorldOrSchemaMismatch");
        return false;
    }
    SaveGeneration = Manifest.U64();
    Sequence = Manifest.U64();
    const uint32 CheckpointCrc = Manifest.U32();
    if(!Manifest.AtEnd() || SaveGeneration == 0)
    {
        OutError = TEXT("Inventory.ManifestMalformed");
        return false;
    }
    const FString CheckpointPath = Directory / FString::Printf(TEXT("checkpoint-%020llu.bin"), static_cast<unsigned long long>(SaveGeneration));
    TArray<uint8> CheckpointBytes;
    if(!ReadFile(CheckpointPath, MaximumCheckpointBytes, CheckpointBytes, OutError) ||
       FCrc::MemCrc32(CheckpointBytes.GetData(), CheckpointBytes.Num()) != CheckpointCrc ||
       !DecodeCheckpoint(CheckpointBytes, OutCheckpoint, OutError) || OutCheckpoint.WorldId != WorldId || OutCheckpoint.Sequence != Sequence)
    {
        OutError = TEXT("Inventory.CommittedCheckpointInvalid");
        return false;
    }
    if(!ReadJournal(Sequence, OutRecords, bInRepairTail, OutError))
    {
        return false;
    }
    bHealthy = true;
    bOpened = true;
    return true;
}

bool FInventoryJournal::ReadJournal(uint64 InCheckpointSequence, TArray<FInventoryJournalRecord>& OutRecords, bool bInRepairTail, FString& OutError)
{
    TArray<uint8> Bytes;
    if(!ReadFile(Directory / TEXT("journal.bin"), MaximumCheckpointBytes, Bytes, OutError))
    {
        return false;
    }
    OutRecords.Reset();
    int32 Position = 0;
    uint64 LastSequence = 0;
    uint64 ReplayedSequence = InCheckpointSequence;
    TSet<FGuid> TransactionIds;
    while(Position < Bytes.Num())
    {
        if(Bytes.Num() - Position < 12)
        {
            break;
        }
        FInventoryBinaryReader Header(MakeArrayView(Bytes).Slice(Position, 12));
        if(Header.U32() != InventoryJournalFormat::RecordMagic || Header.U32() != 2)
        {
            OutError = TEXT("Inventory.JournalHeaderCorrupt");
            return false;
        }
        const uint32 PayloadCount = Header.U32();
        if(PayloadCount > MaximumRecordBytes - 20)
        {
            OutError = TEXT("Inventory.JournalRecordSizeLimit");
            return false;
        }
        const int32 FrameCount = static_cast<int32>(PayloadCount) + 20;
        if(FrameCount > Bytes.Num() - Position)
        {
            break;
        }
        const TConstArrayView<uint8> Payload = MakeArrayView(Bytes).Slice(Position + 12, PayloadCount);
        FInventoryBinaryReader Trailer(MakeArrayView(Bytes).Slice(Position + 12 + PayloadCount, 8));
        if(Trailer.U32() != FCrc::MemCrc32(Payload.GetData(), Payload.Num()) || Trailer.U32() != InventoryJournalFormat::RecordEnd)
        {
            OutError = TEXT("Inventory.JournalChecksumCorrupt");
            return false;
        }
        FInventoryJournalRecord Record;
        if(!DecodeRecord(Payload, Record, OutError) || Record.WorldId != WorldId ||
           (LastSequence != 0 && Record.Sequence != LastSequence + 1) || TransactionIds.Contains(Record.TransactionId))
        {
            OutError = TEXT("Inventory.JournalSequenceOrIdentityCorrupt");
            return false;
        }
        LastSequence = Record.Sequence;
        TransactionIds.Add(Record.TransactionId);
        if(Record.Sequence > InCheckpointSequence)
        {
            if(Record.Sequence != ReplayedSequence + 1)
            {
                OutError = TEXT("Inventory.JournalMissingCommittedSequence");
                return false;
            }
            ReplayedSequence = Record.Sequence;
            OutRecords.Add(MoveTemp(Record));
        }
        Position += FrameCount;
    }
    if(bInRepairTail && Position != Bytes.Num())
    {
        const FString Temporary = Directory / TEXT("journal.repair");
        if(!WriteDurable(Temporary, MakeArrayView(Bytes).Slice(0, Position), false, OutError) ||
           !ReplaceDurable(Temporary, Directory / TEXT("journal.bin"), OutError))
        {
            return false;
        }
    }
    Sequence = ReplayedSequence;
    JournalBytes = Position;
    return true;
}

bool FInventoryJournal::Append(FInventoryJournalRecord& InOutRecord, FString& OutError)
{
    if(!bHealthy || Sequence == MAX_uint64 || InOutRecord.WorldId != WorldId)
    {
        OutError = TEXT("Inventory.JournalUnavailable");
        return false;
    }
    InOutRecord.Sequence = Sequence + 1;
    TArray<uint8> Bytes;
    if(!EncodeRecord(InOutRecord, Bytes, OutError))
    {
        return false;
    }
    if(JournalBytes + Bytes.Num() > MaximumCheckpointBytes)
    {
        OutError = TEXT("Inventory.CheckpointRequired");
        return false;
    }
    if(!WriteDurable(Directory / TEXT("journal.bin"), Bytes, true, OutError))
    {
        bHealthy = false;
        return false;
    }
    Sequence = InOutRecord.Sequence;
    JournalBytes += Bytes.Num();
    return true;
}

bool FInventoryJournal::Checkpoint(const FInventoryCheckpoint& InCheckpoint, FString& OutError)
{
    if(!bHealthy || InCheckpoint.WorldId != WorldId || InCheckpoint.Sequence != Sequence || SaveGeneration == MAX_uint64)
    {
        OutError = TEXT("Inventory.CheckpointBoundaryInvalid");
        return false;
    }
    TArray<uint8> Bytes;
    if(!EncodeCheckpoint(InCheckpoint, Bytes, OutError))
    {
        return false;
    }
    const uint64 NewGeneration = SaveGeneration + 1;
    const FString NewPath = Directory / FString::Printf(TEXT("checkpoint-%020llu.bin"), static_cast<unsigned long long>(NewGeneration));
    if(!WriteDurable(NewPath + TEXT(".tmp"), Bytes, false, OutError) || !ReplaceDurable(NewPath + TEXT(".tmp"), NewPath, OutError))
    {
        bHealthy = false;
        return false;
    }
    TArray<uint8> VerifiedBytes;
    if(!ReadFile(NewPath, MaximumCheckpointBytes, VerifiedBytes, OutError) || VerifiedBytes != Bytes)
    {
        bHealthy = false;
        OutError = TEXT("Inventory.CheckpointVerificationFailed");
        return false;
    }
    FInventoryBinaryWriter Manifest(128);
    Manifest.U32(InventoryJournalFormat::ManifestMagic);
    Manifest.U32(3);
    Manifest.Guid(WorldId);
    Manifest.Guid(StoreId);
    Manifest.U64(NewGeneration);
    Manifest.U64(Sequence);
    Manifest.U32(FCrc::MemCrc32(Bytes.GetData(), Bytes.Num()));
    Manifest.U32(FCrc::MemCrc32(Manifest.GetBytes().GetData(), Manifest.GetBytes().Num()));
    const FString ManifestTemporary = Directory / TEXT("manifest.new");
    if(!Manifest.IsValid() || !WriteDurable(ManifestTemporary, Manifest.GetBytes(), false, OutError) ||
       !ReplaceDurable(ManifestTemporary, Directory / TEXT("manifest.bin"), OutError))
    {
        bHealthy = false;
        return false;
    }
    TArray<uint8> VerifiedManifest;
    if(!ReadFile(Directory / TEXT("manifest.bin"), 128, VerifiedManifest, OutError) || VerifiedManifest != Manifest.GetBytes())
    {
        bHealthy = false;
        OutError = TEXT("Inventory.ManifestVerificationFailed");
        return false;
    }
    SaveGeneration = NewGeneration;
    const TArray<uint8> Empty;
    const FString JournalTemporary = Directory / TEXT("journal.new");
    if(!WriteDurable(JournalTemporary, Empty, false, OutError) || !ReplaceDurable(JournalTemporary, Directory / TEXT("journal.bin"), OutError))
    {
        bHealthy = false;
        return false;
    }
    JournalBytes = 0;
    return true;
}

void FInventoryJournal::Close()
{
    ReleaseStoreLock();
    bHealthy = false;
    Directory.Reset();
    WorldId.Invalidate();
    StoreId.Invalidate();
    Sequence = 0;
    SaveGeneration = 0;
    JournalBytes = 0;
}

bool FInventoryJournal::IsHealthy() const
{
    return bHealthy;
}

uint64 FInventoryJournal::GetSequence() const
{
    return Sequence;
}

bool FInventoryJournal::NeedsCheckpoint() const
{
    return JournalBytes >= 64 * 1024 * 1024;
}

FGuid FInventoryJournal::GetStoreId() const
{
    return StoreId;
}

void FInventoryJournal::SwapState(FInventoryJournal& InOther)
{
    Swap(Directory, InOther.Directory);
    Swap(WorldId, InOther.WorldId);
    Swap(StoreId, InOther.StoreId);
    Swap(Sequence, InOther.Sequence);
    Swap(SaveGeneration, InOther.SaveGeneration);
    Swap(JournalBytes, InOther.JournalBytes);
    Swap(bHealthy, InOther.bHealthy);
    Swap(StoreLock, InOther.StoreLock);
}

bool FInventoryJournal::FoldRecords(FInventoryCheckpoint& InOutCheckpoint, TArray<FInventoryJournalRecord>& InRecords, FString& OutError)
{
    TMap<FGuid, FInventorySnapshotIdentity> Identities;
    for(const TPair<FGuid, TArray<uint8>>& Pair : InOutCheckpoint.Inventories)
    {
        FInventorySnapshotIdentity Identity;
        if(!FInventorySnapshotCodec::InspectIdentity(Pair.Value, Identity, OutError) || Identity.InventoryId != Pair.Key ||
           Identity.WorldId != InOutCheckpoint.WorldId || Identity.Revision == 0 || InOutCheckpoint.Tombstones.Contains(Pair.Key))
        {
            OutError = TEXT("Inventory.ArchiveStoredIdentityMismatch");
            return false;
        }
        Identities.Add(Pair.Key, Identity);
    }
    TMap<FInventoryRecordKey, FInventoryDomainRecord> Domains;
    for(FInventoryDomainRecord& Record : InOutCheckpoint.Domains)
    {
        if(Domains.Contains(Record.Key))
        {
            OutError = TEXT("Inventory.ArchiveDuplicateDomain");
            return false;
        }
        Domains.Add(Record.Key, MoveTemp(Record));
    }
    InOutCheckpoint.Domains.Reset();
    for(FInventoryJournalRecord& Record : InRecords)
    {
        if(Record.WorldId != InOutCheckpoint.WorldId || InOutCheckpoint.Sequence == MAX_uint64 || Record.Sequence != InOutCheckpoint.Sequence + 1)
        {
            OutError = TEXT("Inventory.ArchiveSequenceMismatch");
            return false;
        }
        for(FInventoryJournalChange& Change : Record.Inventories)
        {
            const FInventorySnapshotIdentity* Before = Identities.Find(Change.InventoryId);
            if(Change.bCreated ? (Before || InOutCheckpoint.Tombstones.Contains(Change.InventoryId) || Change.BeforeRevision != 0) :
                (!Before || Before->Generation != Change.Generation || Before->Revision != Change.BeforeRevision))
            {
                OutError = TEXT("Inventory.ArchiveInventoryBeforeMismatch");
                return false;
            }
            if(Change.bDeleted)
            {
                InOutCheckpoint.Inventories.Remove(Change.InventoryId);
                Identities.Remove(Change.InventoryId);
                InOutCheckpoint.Tombstones.Add(Change.InventoryId, FInventoryTombstone{Change.Generation, Change.AfterRevision});
                continue;
            }
            FInventorySnapshotIdentity After;
            if(!FInventorySnapshotCodec::InspectIdentity(Change.AfterBytes, After, OutError) || After.WorldId != InOutCheckpoint.WorldId ||
               After.InventoryId != Change.InventoryId || After.Generation != Change.Generation || After.Revision != Change.AfterRevision)
            {
                OutError = TEXT("Inventory.ArchiveInventoryAfterMismatch");
                return false;
            }
            Identities.Add(Change.InventoryId, After);
            InOutCheckpoint.Inventories.Add(Change.InventoryId, MoveTemp(Change.AfterBytes));
        }
        for(FInventoryDomainRedo& Change : Record.Domains)
        {
            const FInventoryRecordKey Key{Change.Domain, Change.EntityId};
            const FInventoryDomainRecord* Before = Domains.Find(Key);
            if((Before ? Before->Revision : 0) != Change.ExpectedRevision)
            {
                OutError = TEXT("Inventory.ArchiveDomainBeforeMismatch");
                return false;
            }
            FInventoryDomainRecord After;
            After.Key = Key;
            After.Revision = Change.TargetRevision;
            After.bDeleted = Change.bDelete;
            After.Bytes = MoveTemp(Change.AfterBytes);
            Domains.Add(Key, MoveTemp(After));
        }
        InOutCheckpoint.Sequence = Record.Sequence;
        if(InOutCheckpoint.Inventories.Num() > InventoryJournalFormat::MaximumEntries ||
           InOutCheckpoint.Tombstones.Num() > InventoryJournalFormat::MaximumEntries || Domains.Num() > InventoryJournalFormat::MaximumEntries)
        {
            OutError = TEXT("Inventory.ArchiveEntryLimit");
            return false;
        }
    }
    Domains.GenerateValueArray(InOutCheckpoint.Domains);
    InOutCheckpoint.Domains.Sort([](const FInventoryDomainRecord& InLeft, const FInventoryDomainRecord& InRight)
    {
        return InLeft.Key.Domain == InRight.Key.Domain ? InventoryOrder::GuidLess(InLeft.Key.EntityId, InRight.Key.EntityId) : InLeft.Key.Domain.LexicalLess(InRight.Key.Domain);
    });
    return true;
}

bool FInventoryJournal::ExportLatest(const FString& InDirectory, const FGuid& InWorldId, const FGuid& InStoreId,
    FInventoryArchiveSnapshot& OutSnapshot, FString& OutError)
{
    FInventoryJournal Source;
    FInventoryCheckpoint Checkpoint;
    TArray<FInventoryJournalRecord> Records;
    if(!Source.OpenInternal(InDirectory, InWorldId, InStoreId, false, false, Checkpoint, Records, OutError) ||
       !FoldRecords(Checkpoint, Records, OutError)) return false;
    FInventoryArchiveSnapshot Snapshot;
    Snapshot.WorldId = InWorldId;
    Snapshot.StoreId = InStoreId;
    Snapshot.Sequence = Checkpoint.Sequence;
    if(Snapshot.Sequence != Source.GetSequence() || !EncodeCheckpoint(Checkpoint, Snapshot.CheckpointBytes, OutError))
    {
        if(OutError.IsEmpty()) OutError = TEXT("Inventory.ArchiveBoundaryMismatch");
        return false;
    }
    OutSnapshot = MoveTemp(Snapshot);
    return true;
}

bool FInventoryJournal::PrepareFork(const FInventoryArchiveSnapshot& InSnapshot, const FString& InTargetDirectory,
    const FGuid& InTargetStoreId, const FGuid& InOperationId,
    TSharedPtr<FInventoryPreparedStore, ESPMode::ThreadSafe>& OutPrepared, FString& OutError)
{
    if(OutPrepared || InTargetDirectory.IsEmpty() || !InTargetStoreId.IsValid() || InTargetStoreId == InSnapshot.StoreId ||
       !InOperationId.IsValid() || !InSnapshot.Validate(OutError))
    {
        if(OutError.IsEmpty()) OutError = TEXT("Inventory.ForkRequestInvalid");
        return false;
    }
    FInventoryCheckpoint Checkpoint;
    TArray<uint8> CanonicalBytes;
    if(!DecodeCheckpoint(InSnapshot.CheckpointBytes, Checkpoint, OutError) ||
       !EncodeCheckpoint(Checkpoint, CanonicalBytes, OutError)) return false;
    TSharedPtr<FInventoryPreparedStore, ESPMode::ThreadSafe> Prepared(new FInventoryPreparedStore());
    Prepared->WorldId = InSnapshot.WorldId;
    Prepared->StoreId = InTargetStoreId;
    Prepared->SourceStoreId = InSnapshot.StoreId;
    Prepared->OperationId = InOperationId;
    Prepared->Sequence = InSnapshot.Sequence;
    Prepared->CheckpointCrc = FCrc::MemCrc32(CanonicalBytes.GetData(), CanonicalBytes.Num());
    Prepared->Directory = FPaths::ConvertRelativePathToFull(InTargetDirectory);
    FPaths::NormalizeDirectoryName(Prepared->Directory);
    Prepared->Journal = MakeUnique<FInventoryJournal>();
    FInventoryJournal& Target = *Prepared->Journal;
    Target.Directory = Prepared->Directory;
    Target.WorldId = Prepared->WorldId;
    Target.StoreId = Prepared->StoreId;
    Target.Sequence = Prepared->Sequence;
    IPlatformFile& Platform = FPlatformFileManager::Get().GetPlatformFile();
    if(!Platform.CreateDirectoryTree(*FPaths::GetPath(Target.Directory)) || !InventoryJournalFormat::CreateExclusiveDirectory(Target.Directory))
    {
        OutError = TEXT("Inventory.ForkTargetExistsOrUnavailable");
        return false;
    }
    bool bSucceeded = false;
    ON_SCOPE_EXIT
    {
        if(!bSucceeded)
        {
            FString CleanupError;
            if(Target.StoreLock != -1)
            {
                if(!Target.RemoveLockedDirectory(CleanupError)) OutError += TEXT("; ") + CleanupError;
            }
            else
            {
                Prepared->Close();
            }
        }
    };
    if(!Target.AcquireStoreLock(OutError) || !SyncDirectory(FPaths::GetPath(Target.Directory), OutError)) return false;
    InventoryJournalFormat::FForkIdentity Identity;
    Identity.WorldId = InSnapshot.WorldId;
    Identity.StoreId = InTargetStoreId;
    Identity.SourceStoreId = InSnapshot.StoreId;
    Identity.OperationId = InOperationId;
    Identity.Sequence = InSnapshot.Sequence;
    Identity.CheckpointCrc = Prepared->CheckpointCrc;
    const TArray<uint8> OwnerBytes = InventoryJournalFormat::EncodeForkIdentity(Identity, InventoryJournalFormat::ForkOwnerMagic);
    if(!WriteDurable(Target.Directory / TEXT("fork.owner.new"), OwnerBytes, false, OutError) ||
       !ReplaceDurable(Target.Directory / TEXT("fork.owner.new"), Target.Directory / TEXT("fork.owner"), OutError)) return false;
    Target.bHealthy = true;
    if(!Target.Checkpoint(Checkpoint, OutError)) return false;
    bSucceeded = true;
    OutPrepared = MoveTemp(Prepared);
    return true;
}

bool FInventoryJournal::VerifyPreparedReady(FString& OutError) const
{
    IPlatformFile& Platform = FPlatformFileManager::Get().GetPlatformFile();
    const bool bHasOwner = Platform.FileExists(*(Directory / TEXT("fork.owner")));
    const bool bHasCommit = Platform.FileExists(*(Directory / TEXT("fork.commit")));
    if(!bHasOwner && !bHasCommit) return true;
    TArray<uint8> OwnerBytes;
    TArray<uint8> ReadyBytes;
    InventoryJournalFormat::FForkIdentity Owner;
    InventoryJournalFormat::FForkIdentity Ready;
    if(!bHasOwner || !bHasCommit || !ReadFile(Directory / TEXT("fork.owner"), 128, OwnerBytes, OutError) ||
       !ReadFile(Directory / TEXT("fork.commit"), 128, ReadyBytes, OutError) ||
       !InventoryJournalFormat::DecodeForkIdentity(OwnerBytes, InventoryJournalFormat::ForkOwnerMagic, Owner) ||
       !InventoryJournalFormat::DecodeForkIdentity(ReadyBytes, InventoryJournalFormat::ForkReadyMagic, Ready) ||
       !(Owner == Ready) || Owner.WorldId != WorldId || Owner.StoreId != StoreId)
    {
        OutError = TEXT("Inventory.PreparedStoreNotReady");
        return false;
    }
    return true;
}

bool FInventoryJournal::SealPrepared(const FGuid& InOperationId, FString& OutError)
{
    if(StoreLock == -1 || !InOperationId.IsValid())
    {
        OutError = TEXT("Inventory.PreparedStoreLockMissing");
        return false;
    }
    TArray<uint8> OwnerBytes;
    InventoryJournalFormat::FForkIdentity Identity;
    if(!ReadFile(Directory / TEXT("fork.owner"), 128, OwnerBytes, OutError) ||
       !InventoryJournalFormat::DecodeForkIdentity(OwnerBytes, InventoryJournalFormat::ForkOwnerMagic, Identity) ||
       Identity.WorldId != WorldId || Identity.StoreId != StoreId || Identity.OperationId != InOperationId)
    {
        OutError = TEXT("Inventory.PreparedStoreOwnerMismatch");
        return false;
    }
    IPlatformFile& Platform = FPlatformFileManager::Get().GetPlatformFile();
    if(Platform.FileExists(*(Directory / TEXT("fork.commit")))) return VerifyPreparedReady(OutError);
    TArray<uint8> ManifestBytes;
    if(!ReadFile(Directory / TEXT("manifest.bin"), 128, ManifestBytes, OutError) || !InventoryJournalFormat::VerifyChecksum(ManifestBytes)) return false;
    FInventoryBinaryReader Manifest(MakeArrayView(ManifestBytes).Slice(0, ManifestBytes.Num() - 4));
    if(Manifest.U32() != InventoryJournalFormat::ManifestMagic || Manifest.U32() != 3 || Manifest.Guid() != WorldId || Manifest.Guid() != StoreId)
    {
        OutError = TEXT("Inventory.PreparedManifestIdentityMismatch");
        return false;
    }
    const uint64 Generation = Manifest.U64();
    const uint64 ManifestSequence = Manifest.U64();
    const uint32 CheckpointCrc = Manifest.U32();
    if(!Manifest.AtEnd() || Generation != 1 || ManifestSequence != Identity.Sequence || CheckpointCrc != Identity.CheckpointCrc)
    {
        OutError = TEXT("Inventory.PreparedManifestBoundaryMismatch");
        return false;
    }
    TArray<uint8> CheckpointBytes;
    TArray<uint8> JournalData;
    FInventoryCheckpoint Checkpoint;
    if(!ReadFile(Directory / TEXT("checkpoint-00000000000000000001.bin"), MaximumCheckpointBytes, CheckpointBytes, OutError) ||
       FCrc::MemCrc32(CheckpointBytes.GetData(), CheckpointBytes.Num()) != CheckpointCrc ||
       !DecodeCheckpoint(CheckpointBytes, Checkpoint, OutError) || Checkpoint.WorldId != WorldId || Checkpoint.Sequence != Identity.Sequence ||
       !ReadFile(Directory / TEXT("journal.bin"), 0, JournalData, OutError) || !JournalData.IsEmpty())
    {
        OutError = TEXT("Inventory.PreparedStoreContentsMismatch");
        return false;
    }
    const TArray<uint8> ReadyBytes = InventoryJournalFormat::EncodeForkIdentity(Identity, InventoryJournalFormat::ForkReadyMagic);
    if(!WriteDurable(Directory / TEXT("fork.commit.new"), ReadyBytes, false, OutError) ||
       !ReplaceDurable(Directory / TEXT("fork.commit.new"), Directory / TEXT("fork.commit"), OutError)) return false;
    return VerifyPreparedReady(OutError);
}

bool FInventoryJournal::MarkPreparedCommitted(const FString& InDirectory, const FGuid& InWorldId, const FGuid& InStoreId,
    const FGuid& InOperationId, FString& OutError)
{
    if(InDirectory.IsEmpty() || !InWorldId.IsValid() || !InStoreId.IsValid() || !InOperationId.IsValid())
    {
        OutError = TEXT("Inventory.PreparedStoreIdentityInvalid");
        return false;
    }
    FInventoryJournal Target;
    Target.Directory = FPaths::ConvertRelativePathToFull(InDirectory);
    FPaths::NormalizeDirectoryName(Target.Directory);
    Target.WorldId = InWorldId;
    Target.StoreId = InStoreId;
    return Target.AcquireStoreLock(OutError) && Target.SealPrepared(InOperationId, OutError);
}

bool FInventoryJournal::RemoveLockedDirectory(FString& OutError)
{
    if(StoreLock == -1 || Directory.IsEmpty())
    {
        OutError = TEXT("Inventory.PreparedStoreLockMissing");
        return false;
    }
    const FString Original = Directory;
    const FString Retired = Original + TEXT(".discard.") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
    bool bMoved = false;
#if PLATFORM_WINDOWS
    bMoved = ::MoveFileExW(*Original, *Retired, MOVEFILE_WRITE_THROUGH) != 0;
#elif PLATFORM_LINUX
    const FTCHARToUTF8 SourcePath(*Original);
    const FTCHARToUTF8 TargetPath(*Retired);
    bMoved = ::rename(SourcePath.Get(), TargetPath.Get()) == 0;
#endif
    if(!bMoved)
    {
        OutError = TEXT("Inventory.PreparedStoreRetirementFailed");
        return false;
    }
    Close();
    if(!IFileManager::Get().DeleteDirectory(*Retired, false, true) || !SyncDirectory(FPaths::GetPath(Original), OutError))
    {
        if(OutError.IsEmpty()) OutError = TEXT("Inventory.PreparedStoreCleanupFailed");
        return false;
    }
    return true;
}

bool FInventoryJournal::DiscardPrepared(const FString& InDirectory, const FGuid& InWorldId, const FGuid& InStoreId,
    const FGuid& InOperationId, FString& OutError)
{
    if(InDirectory.IsEmpty() || !InWorldId.IsValid() || !InStoreId.IsValid() || !InOperationId.IsValid())
    {
        OutError = TEXT("Inventory.PreparedStoreIdentityInvalid");
        return false;
    }
    FInventoryJournal Target;
    Target.Directory = FPaths::ConvertRelativePathToFull(InDirectory);
    FPaths::NormalizeDirectoryName(Target.Directory);
    Target.WorldId = InWorldId;
    Target.StoreId = InStoreId;
    IPlatformFile& Platform = FPlatformFileManager::Get().GetPlatformFile();
    if(!Platform.DirectoryExists(*Target.Directory)) return true;
    if(!Target.AcquireStoreLock(OutError)) return false;
    if(Platform.FileExists(*(Target.Directory / TEXT("fork.commit"))) || Platform.FileExists(*(Target.Directory / TEXT("fork.commit.new"))))
    {
        OutError = TEXT("Inventory.PreparedStoreProtected");
        return false;
    }
    TArray<uint8> OwnerBytes;
    InventoryJournalFormat::FForkIdentity Identity;
    if(!ReadFile(Target.Directory / TEXT("fork.owner"), 128, OwnerBytes, OutError) ||
       !InventoryJournalFormat::DecodeForkIdentity(OwnerBytes, InventoryJournalFormat::ForkOwnerMagic, Identity) ||
       Identity.WorldId != InWorldId || Identity.StoreId != InStoreId || Identity.OperationId != InOperationId)
    {
        OutError = TEXT("Inventory.PreparedStoreOwnerMismatch");
        return false;
    }
    return Target.RemoveLockedDirectory(OutError);
}

bool FInventoryJournal::DiscardUnbound(const FString& InDirectory, const FGuid& InWorldId, const FGuid& InStoreId, FString& OutError)
{
    FString Path = FPaths::ConvertRelativePathToFull(InDirectory);
    FPaths::NormalizeDirectoryName(Path);
    FString Expected = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Inventory"), TEXT("Unbound"), InStoreId.ToString(EGuidFormats::Digits)));
    FPaths::NormalizeDirectoryName(Expected);
    if(InDirectory.IsEmpty() || !InWorldId.IsValid() || !InStoreId.IsValid() || !FPaths::IsSamePath(Path, Expected))
    {
        OutError = TEXT("Inventory.UnboundStorePathMismatch");
        return false;
    }
    IPlatformFile& Platform = FPlatformFileManager::Get().GetPlatformFile();
    if(!Platform.DirectoryExists(*Path)) return true;
    FInventoryJournal Detached;
    FInventoryCheckpoint Checkpoint;
    TArray<FInventoryJournalRecord> Records;
    if(!Detached.OpenInternal(Path, InWorldId, InStoreId, false, false, Checkpoint, Records, OutError)) return false;
    if(Platform.FileExists(*(Path / TEXT("fork.owner"))) || Platform.FileExists(*(Path / TEXT("fork.commit"))))
    {
        OutError = TEXT("Inventory.UnboundStoreIsPublishedFork");
        return false;
    }
    return Detached.RemoveLockedDirectory(OutError);
}
