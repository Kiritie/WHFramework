#include "Voxel/Save/VoxelRegionStore.h"

#include "HAL/FileManager.h"
#include "Misc/Crc.h"
#include "Misc/Paths.h"
#include "SaveGame/SaveGameStorage.h"
#include "String/LexFromString.h"
#include "Voxel/Generation/VoxelGenerationMath.h"
#include "Voxel/Serialization/VoxelBinaryCodec.h"

namespace
{
	constexpr uint32 RegionMagic = 0x33525856;
	constexpr int32 HeaderBytes = 84;
	constexpr int32 IndexEntryBytes = 18;
	constexpr uint32 MaxRecordBytes = 1024 * 1024 + 65536 + 16;

	struct FEntry
	{
		uint16 Local = 0;
		uint64 Offset = 0;
		uint32 Size = 0;
		uint32 Crc = 0;
	};

	struct FRegionIndex
	{
		FVoxelRegionFileHeader Header;
		TArray<FEntry> Entries;
	};

	FIntVector RegionOf(const FIntVector& InSection)
	{
		return FIntVector(
			VoxelGeneration::FloorDivide(InSection.X, 8),
			VoxelGeneration::FloorDivide(InSection.Y, 8),
			VoxelGeneration::FloorDivide(InSection.Z, 8));
	}

	int32 RegionPositiveMod(const int32 InValue)
	{
		const int32 Value = InValue % 8;
		return Value < 0 ? Value + 8 : Value;
	}

	uint16 LocalOf(const FIntVector& InSection)
	{
		return static_cast<uint16>(
			RegionPositiveMod(InSection.X) +
			RegionPositiveMod(InSection.Y) * 8 +
			RegionPositiveMod(InSection.Z) * 64);
	}

	FString RegionPath(const FString& InDirectory, const FIntVector& InRegion)
	{
		return FPaths::Combine(
			InDirectory,
			TEXT("voxel"),
			TEXT("regions"),
			FString::Printf(TEXT("r_%d_%d_%d.bin"), InRegion.X, InRegion.Y, InRegion.Z));
	}

	bool ReadBytes(FArchive& InArchive, const int64 InOffset, const int32 InSize, TArray<uint8>& OutBytes)
	{
		if (InOffset < 0 || InSize < 0 || InOffset > InArchive.TotalSize() || InSize > InArchive.TotalSize() - InOffset)
		{
			return false;
		}
		OutBytes.SetNumUninitialized(InSize);
		InArchive.Seek(InOffset);
		if (InSize > 0)
		{
			InArchive.Serialize(OutBytes.GetData(), InSize);
		}
		return !InArchive.IsError();
	}

	EVoxelRegionRead ReadIndex(const FString& InPath, FRegionIndex& OutIndex, FString& OutError)
	{
		if (!IFileManager::Get().FileExists(*InPath))
		{
			return EVoxelRegionRead::Missing;
		}
		TUniquePtr<FArchive> File(IFileManager::Get().CreateFileReader(*InPath));
		if (!File)
		{
			OutError = TEXT("Cannot open voxel region file");
			return EVoxelRegionRead::Failed;
		}
		TArray<uint8> HeaderData;
		if (!ReadBytes(*File, 0, HeaderBytes, HeaderData))
		{
			OutError = TEXT("Truncated voxel region header");
			return EVoxelRegionRead::Failed;
		}
		FVoxelByteReader Reader(HeaderData);
		FRegionIndex Index;
		Index.Header.Magic = Reader.U32();
		Index.Header.Version = Reader.U32();
		Index.Header.EntryCount = Reader.U32();
		Index.Header.RegionRevision = Reader.U64();
		for (uint64& Word : Index.Header.ModifiedMask)
		{
			Word = Reader.U64();
		}
		if (!Reader.End() ||
			Index.Header.Magic != RegionMagic ||
			Index.Header.Version != VoxelRegionFileVersion ||
			Index.Header.EntryCount > 512)
		{
			OutError = TEXT("Voxel region V3 header is invalid");
			return EVoxelRegionRead::Failed;
		}
		TArray<uint8> IndexData;
		if (!ReadBytes(*File, HeaderBytes, Index.Header.EntryCount * IndexEntryBytes, IndexData))
		{
			OutError = TEXT("Truncated voxel region index");
			return EVoxelRegionRead::Failed;
		}
		FVoxelByteReader IndexReader(IndexData);
		uint64 ExpectedOffset = HeaderBytes + static_cast<uint64>(Index.Header.EntryCount) * IndexEntryBytes;
		int32 PreviousLocal = INDEX_NONE;
		uint64 ExpectedMask[8] = {};
		for (uint32 EntryIndex = 0; EntryIndex < Index.Header.EntryCount; ++EntryIndex)
		{
			FEntry Entry;
			Entry.Local = IndexReader.U16();
			Entry.Offset = IndexReader.U64();
			Entry.Size = IndexReader.U32();
			Entry.Crc = IndexReader.U32();
			if (Entry.Local >= 512 ||
				Entry.Local <= PreviousLocal ||
				Entry.Offset != ExpectedOffset ||
				Entry.Size == 0 ||
				Entry.Size > MaxRecordBytes ||
				Entry.Offset + Entry.Size > static_cast<uint64>(File->TotalSize()))
			{
				OutError = TEXT("Voxel region index bounds are invalid");
				return EVoxelRegionRead::Failed;
			}
			ExpectedMask[Entry.Local / 64] |= 1ull << (Entry.Local % 64);
			PreviousLocal = Entry.Local;
			ExpectedOffset += Entry.Size;
			Index.Entries.Add(Entry);
		}
		if (!IndexReader.End() || ExpectedOffset != static_cast<uint64>(File->TotalSize()))
		{
			OutError = TEXT("Voxel region payload bounds are invalid");
			return EVoxelRegionRead::Failed;
		}
		for (int32 Word = 0; Word < 8; ++Word)
		{
			if (ExpectedMask[Word] != Index.Header.ModifiedMask[Word])
			{
				OutError = TEXT("Voxel region modified mask does not match its index");
				return EVoxelRegionRead::Failed;
			}
		}
		OutIndex = MoveTemp(Index);
		return EVoxelRegionRead::Loaded;
	}

	bool ReadEntry(FArchive& InArchive, const FEntry& InEntry, TArray<uint8>& OutBytes)
	{
		return ReadBytes(InArchive, InEntry.Offset, InEntry.Size, OutBytes) &&
			FCrc::MemCrc32(OutBytes.GetData(), OutBytes.Num()) == InEntry.Crc;
	}
}

void FVoxelRegionStore::SetSource(
	const FGuid& InSaveId,
	const int32 InGeneration,
	FSaveGameStorage* InStorage)
{
	check(IsInGameThread());
	SourceDirectory = InStorage && InSaveId.IsValid() && InGeneration > 0 ?
		InStorage->GetGenerationDir(InSaveId, InGeneration) : FString();
}

void FVoxelRegionStore::Reset()
{
	SourceDirectory.Reset();
}

const FString& FVoxelRegionStore::GetSourceDirectory() const
{
	return SourceDirectory;
}

void FVoxelRegionStore::AdvanceSource(const FString& InCommittedDirectory)
{
	SourceDirectory = InCommittedDirectory;
}

FVoxelRegionReadView FVoxelRegionStore::CaptureRead(const FIntVector& InSection) const
{
	return { SourceDirectory, InSection };
}

EVoxelRegionRead FVoxelRegionStore::ReadSection(
	const FIntVector& InSection,
	TArray<uint8>& OutBytes,
	FString& OutError) const
{
	return Read(CaptureRead(InSection), OutBytes, OutError);
}

bool FVoxelRegionStore::ReadChangeHeader(
	const FIntVector& InRegion,
	uint64& OutRevision,
	TArray<uint64>& OutModifiedMask,
	FString& OutError) const
{
	FRegionIndex Index;
	const EVoxelRegionRead Status = ReadIndex(RegionPath(SourceDirectory, InRegion), Index, OutError);
	if (Status == EVoxelRegionRead::Failed)
	{
		return false;
	}
	OutModifiedMask.SetNumZeroed(8);
	OutRevision = 0;
	if (Status == EVoxelRegionRead::Loaded)
	{
		OutRevision = Index.Header.RegionRevision;
		for (int32 Word = 0; Word < 8; ++Word)
		{
			OutModifiedMask[Word] = Index.Header.ModifiedMask[Word];
		}
	}
	return true;
}

bool FVoxelRegionStore::ScanChangeHeaders(
	TFunctionRef<void(const FIntVector&, uint64, const TArray<uint64>&)> InVisit,
	FString& OutError) const
{
	if (SourceDirectory.IsEmpty())
	{
		return true;
	}
	TArray<FString> Files;
	IFileManager::Get().FindFiles(
		Files,
		*FPaths::Combine(SourceDirectory, TEXT("voxel"), TEXT("regions"), TEXT("r_*_*_*.bin")),
		true,
		false);
	Files.Sort();
	for (const FString& File : Files)
	{
		FString Name = FPaths::GetBaseFilename(File);
		TArray<FString> Parts;
		Name.ParseIntoArray(Parts, TEXT("_"), true);
		FIntVector Region;
		if (Parts.Num() != 4 ||
			Parts[0] != TEXT("r") ||
			!LexTryParseString(Region.X, *Parts[1]) ||
			!LexTryParseString(Region.Y, *Parts[2]) ||
			!LexTryParseString(Region.Z, *Parts[3]))
		{
			OutError = TEXT("Invalid voxel region filename");
			return false;
		}
		uint64 Revision = 0;
		TArray<uint64> Mask;
		if (!ReadChangeHeader(Region, Revision, Mask, OutError))
		{
			return false;
		}
		InVisit(Region, Revision, Mask);
	}
	return true;
}

EVoxelRegionRead FVoxelRegionStore::Read(
	const FVoxelRegionReadView& InView,
	TArray<uint8>& OutBytes,
	FString& OutError)
{
	if (InView.SourceDirectory.IsEmpty())
	{
		return EVoxelRegionRead::Missing;
	}
	const FString Path = RegionPath(InView.SourceDirectory, RegionOf(InView.Section));
	FRegionIndex Index;
	const EVoxelRegionRead Status = ReadIndex(Path, Index, OutError);
	if (Status != EVoxelRegionRead::Loaded)
	{
		return Status;
	}
	const uint16 Local = LocalOf(InView.Section);
	const FEntry* Entry = Index.Entries.FindByPredicate([Local](const FEntry& InEntry)
	{
		return InEntry.Local == Local;
	});
	if (!Entry)
	{
		return EVoxelRegionRead::Missing;
	}
	TUniquePtr<FArchive> File(IFileManager::Get().CreateFileReader(*Path));
	if (!File || !ReadEntry(*File, *Entry, OutBytes))
	{
		OutError = TEXT("Voxel region record read or checksum failed");
		return EVoxelRegionRead::Failed;
	}
	return EVoxelRegionRead::Loaded;
}

bool FVoxelRegionStore::StageSection(
	FVoxelRegionWritePlan& InPlan,
	const FIntVector& InSection,
	TArray<uint8>&& InBytes)
{
	if (InBytes.IsEmpty() || InBytes.Num() > static_cast<int32>(MaxRecordBytes))
	{
		return false;
	}
	FVoxelRegionOperation Operation;
	Operation.Bytes = MakeShared<TArray<uint8>, ESPMode::ThreadSafe>(MoveTemp(InBytes));
	InPlan.Operations.Add(InSection, MoveTemp(Operation));
	return true;
}

void FVoxelRegionStore::StageDelete(FVoxelRegionWritePlan& InPlan, const FIntVector& InSection)
{
	FVoxelRegionOperation Operation;
	Operation.bDelete = true;
	InPlan.Operations.Add(InSection, MoveTemp(Operation));
}

bool FVoxelRegionStore::WritePendingRegions(
	const FVoxelRegionWritePlan& InPlan,
	const FString& InTemporaryGenerationDirectory,
	FString& OutError)
{
	if (!InPlan.TransactionId.IsValid() || InTemporaryGenerationDirectory.IsEmpty())
	{
		OutError = TEXT("Invalid voxel region write target");
		return false;
	}
	TMap<FIntVector, TArray<FIntVector>> Groups;
	for (const TPair<FIntVector, FVoxelRegionOperation>& Pair : InPlan.Operations)
	{
		Groups.FindOrAdd(RegionOf(Pair.Key)).Add(Pair.Key);
	}
	for (const TPair<FIntVector, TArray<FIntVector>>& Group : Groups)
	{
		const FString SourcePath = RegionPath(InPlan.SourceDirectory, Group.Key);
		const FString DestinationPath = RegionPath(InTemporaryGenerationDirectory, Group.Key);
		FRegionIndex Old;
		const EVoxelRegionRead OldStatus = InPlan.SourceDirectory.IsEmpty() ?
			EVoxelRegionRead::Missing : ReadIndex(SourcePath, Old, OutError);
		if (OldStatus == EVoxelRegionRead::Failed)
		{
			return false;
		}
		struct FOutput
		{
			FEntry Entry;
			TSharedPtr<const TArray<uint8>, ESPMode::ThreadSafe> Replacement;
		};
		TMap<uint16, FOutput> Output;
		for (const FEntry& Entry : Old.Entries)
		{
			FOutput Existing;
			Existing.Entry = Entry;
			Output.Add(Entry.Local, MoveTemp(Existing));
		}
		for (const FIntVector& Section : Group.Value)
		{
			const FVoxelRegionOperation& Operation = InPlan.Operations.FindChecked(Section);
			const uint16 Local = LocalOf(Section);
			if (Operation.bDelete)
			{
				Output.Remove(Local);
				continue;
			}
			FOutput Replacement;
			Replacement.Entry.Local = Local;
			Replacement.Entry.Size = Operation.Bytes->Num();
			Replacement.Entry.Crc = FCrc::MemCrc32(Operation.Bytes->GetData(), Operation.Bytes->Num());
			Replacement.Replacement = Operation.Bytes;
			Output.Add(Local, MoveTemp(Replacement));
		}
		if (Output.IsEmpty())
		{
			if (IFileManager::Get().FileExists(*DestinationPath) &&
				!IFileManager::Get().Delete(*DestinationPath, false, true))
			{
				OutError = TEXT("Cannot remove empty voxel region");
				return false;
			}
			continue;
		}
		if (!IFileManager::Get().MakeDirectory(*FPaths::GetPath(DestinationPath), true))
		{
			OutError = TEXT("Cannot create voxel region directory");
			return false;
		}
		TArray<uint16> Locals;
		Output.GetKeys(Locals);
		Locals.Sort();
		FVoxelByteWriter Header(HeaderBytes + 512 * IndexEntryBytes);
		Header.U32(RegionMagic);
		Header.U32(VoxelRegionFileVersion);
		Header.U32(Locals.Num());
		Header.U64(Old.Header.RegionRevision + 1);
		uint64 Mask[8] = {};
		for (const uint16 Local : Locals)
		{
			Mask[Local / 64] |= 1ull << (Local % 64);
		}
		for (const uint64 Word : Mask)
		{
			Header.U64(Word);
		}
		uint64 Offset = HeaderBytes + static_cast<uint64>(Locals.Num()) * IndexEntryBytes;
		for (const uint16 Local : Locals)
		{
			const FOutput& Item = Output.FindChecked(Local);
			Header.U16(Local);
			Header.U64(Offset);
			Header.U32(Item.Entry.Size);
			Header.U32(Item.Entry.Crc);
			Offset += Item.Entry.Size;
		}
		TArray<uint8> HeaderBytesData;
		if (!Header.Finish(HeaderBytesData))
		{
			return false;
		}
		TUniquePtr<FArchive> Writer(IFileManager::Get().CreateFileWriter(*DestinationPath));
		if (!Writer)
		{
			OutError = TEXT("Cannot write voxel region");
			return false;
		}
		Writer->Serialize(HeaderBytesData.GetData(), HeaderBytesData.Num());
		TUniquePtr<FArchive> Source;
		for (const uint16 Local : Locals)
		{
			const FOutput& Item = Output.FindChecked(Local);
			TArray<uint8> Copied;
			const TArray<uint8>* Bytes = Item.Replacement.Get();
			if (!Bytes)
			{
				if (!Source)
				{
					Source.Reset(IFileManager::Get().CreateFileReader(*SourcePath));
				}
				if (!Source || !ReadEntry(*Source, Item.Entry, Copied))
				{
					OutError = TEXT("Cannot copy verified voxel region record");
					return false;
				}
				Bytes = &Copied;
			}
			Writer->Serialize(const_cast<uint8*>(Bytes->GetData()), Bytes->Num());
		}
		if (Writer->IsError() || !Writer->Close())
		{
			OutError = TEXT("Voxel region write failed");
			return false;
		}
	}
	return true;
}
