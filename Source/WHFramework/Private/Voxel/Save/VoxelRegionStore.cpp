#include "Voxel/Save/VoxelRegionStore.h"

#include "HAL/FileManager.h"
#include "Misc/Crc.h"
#include "Misc/Paths.h"
#include "SaveGame/SaveGameStorage.h"
#include "String/LexFromString.h"
#include "Voxel/Generation/VoxelGenerationMath.h"
#include "Voxel/Serialization/VoxelBinaryCodec.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"
#include "Voxel/Task/VoxelTerrainResources.h"

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
		OutBytes.Reserve(InSize);
		OutBytes.SetNumUninitialized(InSize);
		InArchive.Seek(InOffset);
		if (InSize > 0)
		{
			InArchive.Serialize(OutBytes.GetData(), InSize);
		}
		return !InArchive.IsError();
	}

	EVoxelRegionRead ReadIndex(const FString& InPath, FRegionIndex& OutIndex, FString& OutError,
		const FVoxelRegionFileHeader* InExpectedHeader = nullptr)
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
		if (InExpectedHeader && (Index.Header.EntryCount != InExpectedHeader->EntryCount ||
			Index.Header.RegionRevision != InExpectedHeader->RegionRevision ||
			FMemory::Memcmp(Index.Header.ModifiedMask, InExpectedHeader->ModifiedMask, sizeof(Index.Header.ModifiedMask))))
		{
			OutError = TEXT("Voxel原文件代索引已偏离冻结目录");
			return EVoxelRegionRead::Failed;
		}
		TArray<uint8> IndexData;
		if (!ReadBytes(*File, HeaderBytes, Index.Header.EntryCount * IndexEntryBytes, IndexData))
		{
			OutError = TEXT("Truncated voxel region index");
			return EVoxelRegionRead::Failed;
		}
		FVoxelByteReader IndexReader(IndexData);
		Index.Entries.Reserve(Index.Header.EntryCount);
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
	FSaveGameStorage* InStorage,
	const bool bInLoadAsynchronously)
{
	check(IsInGameThread());
	DetachDirectoryLoad();
	SourceDirectory = InStorage && InSaveId.IsValid() && InGeneration > 0 ?
		InStorage->GetGenerationDir(InSaveId, InGeneration) : FString();
	SourceLease = FSaveGameGenerationReadLease::Acquire(SourceDirectory);
	Directory.Reset();
	DirectoryError.Reset();
	bDirectoryReady = SourceDirectory.IsEmpty();
	DirectoryFirstRunnable = FPlatformTime::Seconds();
	if (!bInLoadAsynchronously) bDirectoryReady = LoadDirectory(DirectoryError);
}

void FVoxelRegionStore::Reset()
{
	DetachDirectoryLoad();
	SourceDirectory.Reset();
	SourceLease.Reset();
	Directory.Reset();
	DirectoryError.Reset();
	bDirectoryReady = true;
}

const FString& FVoxelRegionStore::GetSourceDirectory() const
{
	return SourceDirectory;
}

const TSharedPtr<const FSaveGameGenerationReadLease, ESPMode::ThreadSafe>& FVoxelRegionStore::GetSourceLease() const
{
	return SourceLease;
}

const TSharedPtr<const FVoxelRegionDirectorySnapshot, ESPMode::ThreadSafe>& FVoxelRegionStore::GetDirectorySnapshot() const
{
	return Directory;
}

const FString& FVoxelRegionStore::GetDirectoryError() const
{
	return DirectoryError;
}

bool FVoxelRegionStore::AdvanceSourcePrepared(const FString& InCommittedDirectory,
	TSharedPtr<const FVoxelRegionDirectorySnapshot, ESPMode::ThreadSafe> InDirectory, FString& OutError,
	TSharedPtr<const FSaveGameGenerationReadLease, ESPMode::ThreadSafe> InPreparedLease)
{
	check(IsInGameThread());
	const auto NewLease = InPreparedLease ? MoveTemp(InPreparedLease) : FSaveGameGenerationReadLease::Acquire(InCommittedDirectory);
	if (!InDirectory || (!InCommittedDirectory.IsEmpty() && (!NewLease || !NewLease->Matches(InCommittedDirectory))))
	{
		OutError = TEXT("Voxel保存回执缺少预备目录根或文件代租约");
		return false;
	}
	DetachDirectoryLoad();
	SourceDirectory = InCommittedDirectory;
	SourceLease = NewLease;
	Directory = MoveTemp(InDirectory);
	DirectoryError.Reset();
	bDirectoryReady = true;
	OutError.Reset();
	return true;
}

void FVoxelRegionStore::AdvanceSource(const FString& InCommittedDirectory)
{
	check(IsInGameThread());
	DetachDirectoryLoad();
	const auto NewLease = FSaveGameGenerationReadLease::Acquire(InCommittedDirectory);
	checkf(InCommittedDirectory.IsEmpty() || NewLease, TEXT("Cannot advance voxel source to a retiring generation"));
	SourceDirectory = InCommittedDirectory;
	SourceLease = NewLease;
	bDirectoryReady = LoadDirectory(DirectoryError);
}

FVoxelRegionReadView FVoxelRegionStore::CaptureRead(const FIntVector& InSection) const
{
	check(IsInGameThread());
	FVoxelRegionReadView Read{SourceDirectory, InSection, SourceLease, 0};
	if (const FVoxelRegionRecordDescriptor* Record = Directory ? Directory->Records.Find(InSection) : nullptr)
	{
		Read.ExpectedRegionRevision = Record->RegionRevision;
		Read.ExpectedRecordOffset = Record->Offset;
		Read.ExpectedRecordBytes = Record->Bytes;
		Read.ExpectedRecordCrc = Record->Crc;
	}
	return Read;
}

bool FVoxelRegionStore::CapturePinnedRead(const FIntVector& InSection, const uint64 InExpectedRevision,
	FVoxelRegionReadView& OutRead, FString& OutError) const
{
	check(IsInGameThread());
	if (!bDirectoryReady && DirectoryError.IsEmpty())
	{
		OutError = TEXT("DependencyNotReady: 已修改Section的固定目录尚未就绪");
		return false;
	}
	if (!DirectoryError.IsEmpty() || SourceDirectory.IsEmpty() || !SourceLease || !SourceLease->Matches(SourceDirectory))
	{
		OutError = DirectoryError.IsEmpty() ? TEXT("已修改Section缺少有效不可变文件代") : DirectoryError;
		return false;
	}
	const FVoxelRegionRecordDescriptor* Record = Directory ? Directory->Records.Find(InSection) : nullptr;
	if (!Record)
	{
		OutError = TEXT("已修改Section不在捕获的文件代目录中");
		return false;
	}
	OutRead = CaptureRead(InSection);
	OutRead.ExpectedSectionRevision = InExpectedRevision;
	OutError.Reset();
	return true;
}

void FVoxelRegionStore::ReleaseReadLease(FVoxelRegionReadView& InRead)
{
	InRead.GenerationLease.Reset();
}

bool FVoxelRegionStore::TryRetireFileGeneration(const FString& InDirectory) const
{
	return FSaveGameGenerationReadLease::TryRetireDirectory(InDirectory);
}

EVoxelRegionRead FVoxelRegionStore::ReadSection(
	const FIntVector& InSection,
	TArray<uint8>& OutBytes,
	FString& OutError) const
{
	if (!IsDirectoryReady())
	{
		OutError = DirectoryError.IsEmpty() ? TEXT("DependencyNotReady: 固定文件代目录") : DirectoryError;
		return EVoxelRegionRead::Failed;
	}
	return Read(CaptureRead(InSection), OutBytes, OutError);
}

bool FVoxelRegionStore::ReadChangeHeader(
	const FIntVector& InRegion,
	uint64& OutRevision,
	TArray<uint64>& OutModifiedMask,
	FString& OutError) const
{
	if (!DirectoryError.IsEmpty())
	{
		OutError = DirectoryError;
		return false;
	}
	OutModifiedMask.SetNumZeroed(8);
	OutRevision = 0;
	if (const FVoxelRegionFileHeader* Header = Directory ? Directory->Regions.Find(InRegion) : nullptr)
	{
		OutRevision = Header->RegionRevision;
		for (int32 Word = 0; Word < 8; ++Word)
		{
			OutModifiedMask[Word] = Header->ModifiedMask[Word];
		}
	}
	return true;
}

bool FVoxelRegionStore::ScanChangeHeaders(
	TFunctionRef<void(const FIntVector&, uint64, const TArray<uint64>&)> InVisit,
	FString& OutError) const
{
	if (!DirectoryError.IsEmpty())
	{
		OutError = DirectoryError;
		return false;
	}
	if (!Directory) return true;
	for (const FIntVector& Region : Directory->RegionOrder)
	{
		const FVoxelRegionFileHeader& Header = Directory->Regions.FindChecked(Region);
		TArray<uint64> Mask;
		Mask.Append(Header.ModifiedMask, 8);
		InVisit(Region, Header.RegionRevision, Mask);
	}
	OutError.Reset();
	return true;
}

bool FVoxelRegionStore::LoadDirectory(FString& OutError)
{
	check(IsInGameThread());
	Directory.Reset();
	if (!SourceDirectory.IsEmpty() && (!SourceLease || !SourceLease->Matches(SourceDirectory)))
	{
		OutError = TEXT("Voxel目录根缺少文件代租约");
		return false;
	}
	return BuildDirectory(SourceDirectory, nullptr, {}, Directory, OutError);
}

bool FVoxelRegionStore::BuildDirectory(const FString& InDirectory,
	const TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe>& InResources,
	const FVoxelGenerationConsumerKey& InConsumer,
	TSharedPtr<const FVoxelRegionDirectorySnapshot, ESPMode::ThreadSafe>& OutDirectory, FString& OutError,
	const TAtomic<bool>* InCancel, const TAtomic<bool>* InDetached)
{
	OutError.Reset();
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ScratchLease;
	const uint64 ScratchBound = 128ull * 1024 +
		4 * VoxelBoundedAllocation::ArrayBytes<TCHAR>(InDirectory.Len() + 256);
	if (InResources && !InResources->Acquire(InConsumer, EVoxelTerrainResourcePool::Working, ScratchBound, ScratchLease))
	{
		OutError = TEXT("CapacityBlocked: Region目录索引读取峰值未授予");
		return false;
	}
	const FString RegionDirectory = FPaths::Combine(InDirectory, TEXT("voxel"), TEXT("regions"));
	int64 RecordCount = 0;
	int64 RegionCount = 0;
	int64 SceneColumnCount = 0;
	const FString SceneDirectory = FPaths::Combine(InDirectory, TEXT("voxel"), TEXT("actors"));
	auto ScanSceneColumns = [&](TFunctionRef<bool(const FIntPoint&, uint32)> InVisit)
	{
		if (InDirectory.IsEmpty() || !IFileManager::Get().DirectoryExists(*SceneDirectory)) return true;
		bool bValid = true;
		const bool bWalked = IFileManager::Get().IterateDirectory(*SceneDirectory,
			[&](const TCHAR* InFilename, const bool bInDirectory)
			{
				if ((InCancel && InCancel->Load()) || (InDetached && InDetached->Load()))
				{
					OutError = TEXT("Canceled");
					bValid = false;
					return false;
				}
				if (bInDirectory || !FPaths::GetExtension(InFilename).Equals(TEXT("bin"))) return true;
				const FString Filename = FPaths::GetCleanFilename(InFilename);
				TArray<FString> Parts;
				FPaths::GetBaseFilename(Filename).ParseIntoArray(Parts, TEXT("_"), false);
				FIntPoint Column;
				if (Filename.Len() > 64 || Parts.Num() != 3 || Parts[0] != TEXT("c") ||
					!LexTryParseString(Column.X, *Parts[1]) || !LexTryParseString(Column.Y, *Parts[2]) ||
					Column.X < MIN_int32 / 8 || Column.X > MAX_int32 / 8 ||
					Column.Y < MIN_int32 / 8 || Column.Y > MAX_int32 / 8 ||
					Filename != FString::Printf(TEXT("c_%d_%d.bin"), Column.X, Column.Y))
				{
					OutError = TEXT("Invalid Scene column filename");
					bValid = false;
					return false;
				}
				const int64 Bytes = IFileManager::Get().FileSize(InFilename);
				if (Bytes < 10 || Bytes > 8 * 1024 * 1024 || !InVisit(Column, uint32(Bytes)))
				{
					if (OutError.IsEmpty()) OutError = TEXT("Invalid Scene column size/directory");
					bValid = false;
					return false;
				}
				return true;
			});
		if (!bWalked && bValid) OutError = TEXT("Scene column directory walk failed");
		return bWalked && bValid;
	};
	if (!ScanSceneColumns([&](const FIntPoint&, const uint32)
		{
			if (++SceneColumnCount <= MAX_int32) return true;
			OutError = TEXT("Scene column directory count overflow");
			return false;
		})) return false;
	auto Scan = [&](TFunctionRef<bool(const FIntVector&, const FRegionIndex&)> InVisit)
	{
		if (InDirectory.IsEmpty() || !IFileManager::Get().DirectoryExists(*RegionDirectory)) return true;
		bool bValid = true;
		const bool bWalked = IFileManager::Get().IterateDirectory(*RegionDirectory,
			[&](const TCHAR* InFilename, const bool bInDirectory)
			{
				if ((InCancel && InCancel->Load()) || (InDetached && InDetached->Load()))
				{
					OutError = TEXT("Canceled");
					bValid = false;
					return false;
				}
				if (bInDirectory || !FPaths::GetCleanFilename(InFilename).StartsWith(TEXT("r_")) ||
					!FPaths::GetExtension(InFilename).Equals(TEXT("bin"))) return true;
				const FString Name = FPaths::GetBaseFilename(InFilename);
				TArray<FString> Parts;
				Name.ParseIntoArray(Parts, TEXT("_"), true);
				FIntVector Region;
				if (Name.Len() > 128 || Parts.Num() != 4 || Parts[0] != TEXT("r") ||
					!LexTryParseString(Region.X, *Parts[1]) || !LexTryParseString(Region.Y, *Parts[2]) ||
					!LexTryParseString(Region.Z, *Parts[3]) ||
					Region.X < MIN_int32 / 8 || Region.X > MAX_int32 / 8 ||
					Region.Y < MIN_int32 / 8 || Region.Y > MAX_int32 / 8 ||
					Region.Z < MIN_int32 / 8 || Region.Z > MAX_int32 / 8)
				{
					OutError = TEXT("Invalid voxel region filename");
					bValid = false;
					return false;
				}
				FRegionIndex Index;
				if (ReadIndex(RegionPath(InDirectory, Region), Index, OutError) != EVoxelRegionRead::Loaded || !InVisit(Region, Index))
				{
					bValid = false;
					return false;
				}
				return true;
			});
		if (!bWalked && bValid) OutError = TEXT("Voxel目录遍历失败");
		return bWalked && bValid;
	};
	if (!Scan([&](const FIntVector&, const FRegionIndex& InIndex)
		{
			RecordCount += InIndex.Entries.Num();
			++RegionCount;
			if (RecordCount > MAX_int32 || RegionCount > MAX_int32)
			{
				OutError = TEXT("Voxel目录计数溢出");
				return false;
			}
			return true;
		})) return false;
	if (RegionCount > (MAX_int32 - 1) / 32)
	{
		OutError = TEXT("Voxel目录空间索引计数溢出");
		return false;
	}
	const int32 SpatialCount = RegionCount > 0 ? static_cast<int32>(1 + RegionCount * 32) : 0;
	const uint64 MetadataBytes = VoxelBoundedAllocation::SharedBytes<FVoxelRegionDirectorySnapshot>() +
		VoxelBoundedAllocation::MapBytes<FIntVector, FVoxelRegionRecordDescriptor>(static_cast<int32>(RecordCount)) +
		VoxelBoundedAllocation::MapBytes<FIntVector, FVoxelRegionFileHeader>(static_cast<int32>(RegionCount)) +
		VoxelBoundedAllocation::MapBytes<FIntPoint, uint32>(int32(SceneColumnCount)) +
		VoxelBoundedAllocation::ArrayBytes<FIntVector>(static_cast<int32>(RegionCount)) +
		VoxelBoundedAllocation::ArrayBytes<FVoxelRegionDirectorySnapshot::FSpatialNode>(SpatialCount);
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> MetadataLease;
	if (InResources && !InResources->Acquire(InConsumer, EVoxelTerrainResourcePool::Resident, MetadataBytes, MetadataLease))
	{
		OutError = TEXT("CapacityBlocked: 不可变Region目录根未授予");
		return false;
	}
	auto Snapshot = MakeShared<FVoxelRegionDirectorySnapshot, ESPMode::ThreadSafe>();
	Snapshot->Records.Reserve(static_cast<int32>(RecordCount));
	Snapshot->Regions.Reserve(static_cast<int32>(RegionCount));
	Snapshot->SceneColumns.Reserve(int32(SceneColumnCount));
	if (!ScanSceneColumns([&](const FIntPoint& InColumn, const uint32 InBytes)
		{
			if (Snapshot->SceneColumns.Num() >= SceneColumnCount || Snapshot->SceneColumns.Contains(InColumn))
			{
				OutError = TEXT("Scene column directory changed during its count/build");
				return false;
			}
			Snapshot->SceneColumns.Add(InColumn, InBytes);
			return true;
		}) || Snapshot->SceneColumns.Num() != SceneColumnCount)
	{
		if (OutError.IsEmpty()) OutError = TEXT("Scene column directory count/build mismatch");
		return false;
	}
	Snapshot->RegionOrder.Reserve(static_cast<int32>(RegionCount));
	Snapshot->SpatialNodes.Reserve(SpatialCount);
	if (SpatialCount > 0) Snapshot->SpatialNodes.AddDefaulted();
	if (!Scan([&](const FIntVector& InRegion, const FRegionIndex& InIndex)
		{
			if (Snapshot->Regions.Num() >= RegionCount || Snapshot->Regions.Contains(InRegion) ||
				Snapshot->Records.Num() + InIndex.Entries.Num() > RecordCount)
			{
				OutError = TEXT("Voxel目录在预检和构建期间改变");
				return false;
			}
			Snapshot->Regions.Add(InRegion, InIndex.Header);
			Snapshot->RegionOrder.Add(InRegion);
			int32 Node = 0;
			for (int32 Depth = 0; Depth < 32; ++Depth)
			{
				const int32 Shift = 31 - Depth;
				const int32 Child = (((static_cast<uint32>(InRegion.X) ^ 0x80000000u) >> Shift) & 1u) |
					((((static_cast<uint32>(InRegion.Y) ^ 0x80000000u) >> Shift) & 1u) << 1) |
					((((static_cast<uint32>(InRegion.Z) ^ 0x80000000u) >> Shift) & 1u) << 2);
				int32 Next = Snapshot->SpatialNodes[Node].Children[Child];
				if (Next == INDEX_NONE)
				{
					check(Snapshot->SpatialNodes.Num() < SpatialCount);
					Next = Snapshot->SpatialNodes.AddDefaulted();
					Snapshot->SpatialNodes[Node].Children[Child] = Next;
				}
				Node = Next;
			}
			Snapshot->SpatialNodes[Node].Region = InRegion;
			for (const FEntry& Entry : InIndex.Entries)
			{
				const FIntVector Section = InRegion * 8 + FIntVector(Entry.Local % 8, (Entry.Local / 8) % 8, Entry.Local / 64);
				Snapshot->Records.Add(Section, FVoxelRegionRecordDescriptor{InIndex.Header.RegionRevision, Entry.Offset, Entry.Size, Entry.Crc});
			}
			return true;
		})) return false;
	if (Snapshot->Records.Num() != RecordCount || Snapshot->Regions.Num() != RegionCount)
	{
		OutError = TEXT("Voxel目录构建数量与预检不符");
		return false;
	}
	Snapshot->RegionOrder.Sort([](const FIntVector& Left, const FIntVector& Right)
	{
		if (Left.X != Right.X) return Left.X < Right.X;
		if (Left.Y != Right.Y) return Left.Y < Right.Y;
		return Left.Z < Right.Z;
	});
	Snapshot->ResourceLease = MoveTemp(MetadataLease);
	if (Snapshot->ResourceLease)
	{
		const uint64 Actual = VoxelBoundedAllocation::SharedBytes<FVoxelRegionDirectorySnapshot>() +
			Snapshot->Records.GetAllocatedSize() + Snapshot->Regions.GetAllocatedSize() + Snapshot->RegionOrder.GetAllocatedSize() +
			Snapshot->SpatialNodes.GetAllocatedSize() + Snapshot->SceneColumns.GetAllocatedSize();
		check(Actual <= MetadataBytes);
		verify(Snapshot->ResourceLease->Resize(Actual));
	}
	OutDirectory = MoveTemp(Snapshot);
	return true;
}

bool FVoxelRegionDirectorySnapshot::VisitRegions(const FIntVector& InMinimum, const FIntVector& InMaximum,
	TFunctionRef<void(const FIntVector&, const FVoxelRegionFileHeader&)> InVisit, const TAtomic<bool>* InCancel) const
{
	if (SpatialNodes.IsEmpty()) return true;
	auto Walk = [&](auto&& InWalk, const int32 InNode, const int32 InDepth,
		const uint64 InX, const uint64 InY, const uint64 InZ) -> bool
	{
		if (InCancel && InCancel->Load()) return false;
		const uint64 Span = 1ull << (32 - InDepth);
		auto Encode = [](const int32 InValue) { return static_cast<uint32>(InValue) ^ 0x80000000u; };
		if (InX > Encode(InMaximum.X) || InX + Span <= Encode(InMinimum.X) ||
			InY > Encode(InMaximum.Y) || InY + Span <= Encode(InMinimum.Y) ||
			InZ > Encode(InMaximum.Z) || InZ + Span <= Encode(InMinimum.Z)) return true;
		const FSpatialNode& Node = SpatialNodes[InNode];
		if (InDepth == 32)
		{
			InVisit(Node.Region, Regions.FindChecked(Node.Region));
			return true;
		}
		for (int32 Child = 0; Child < 8; ++Child)
		{
			const int32 Next = Node.Children[Child];
			if (Next != INDEX_NONE && !InWalk(InWalk, Next, InDepth + 1,
				InX + ((Child & 1) ? Span / 2 : 0), InY + ((Child & 2) ? Span / 2 : 0),
				InZ + ((Child & 4) ? Span / 2 : 0))) return false;
		}
		return true;
	};
	return Walk(Walk, 0, 0, 0, 0, 0);
}
uint64 FVoxelRegionStore::ReadWorkingBytes(const FVoxelRegionReadView& InView)
{
	const int32 RecordBytes = InView.ExpectedRecordBytes ? int32(InView.ExpectedRecordBytes) : int32(MaxRecordBytes);
	return VoxelBoundedAllocation::InitialGrowBytes<uint8>(RecordBytes) +
		VoxelBoundedAllocation::InitialGrowBytes<uint8>(HeaderBytes) +
		VoxelBoundedAllocation::InitialGrowBytes<uint8>(512 * IndexEntryBytes) +
		VoxelBoundedAllocation::ArrayBytes<FEntry>(512) +
		4 * VoxelBoundedAllocation::ArrayBytes<TCHAR>(InView.SourceDirectory.Len() + 96);
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
	if (!InView.GenerationLease || !InView.GenerationLease->Matches(InView.SourceDirectory))
	{
		OutError = TEXT("Voxel region read requires its immutable file generation lease");
		return EVoxelRegionRead::Failed;
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
	if (InView.ExpectedRecordBytes > 0 &&
		(Index.Header.RegionRevision != InView.ExpectedRegionRevision || Entry->Offset != InView.ExpectedRecordOffset ||
		 Entry->Size != InView.ExpectedRecordBytes || Entry->Crc != InView.ExpectedRecordCrc))
	{
		OutError = TEXT("Voxel记录不属于捕获的目录代或完整性描述");
		return EVoxelRegionRead::Failed;
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

bool FVoxelRegionStore::WritePendingRegion(const FIntVector& InRegion, const FString& InSourceDirectory,
	const FString& InTemporaryGenerationDirectory,
	const TSharedPtr<const FVoxelRegionDirectorySnapshot, ESPMode::ThreadSafe>& InSourceDirectoryRoot,
	const TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe>& InResources,
	const FVoxelGenerationConsumerKey& InConsumer,
	TFunctionRef<bool(const FIntVector&)> InHasReplacement,
	TFunctionRef<bool(const FIntVector&, FVoxelBinaryBuffer&, FString&)> InEncodeReplacement,
	FString& OutError)
{
	if (InTemporaryGenerationDirectory.IsEmpty())
	{
		OutError = TEXT("Voxel流式Region保存缺少目标文件代");
		return false;
	}
	if (!InSourceDirectory.IsEmpty() && !InSourceDirectoryRoot)
	{
		OutError = TEXT("Voxel流式保存缺少捕获的原代目录");
		return false;
	}
	const FVoxelRegionFileHeader EmptyHeader;
	const auto* FrozenHeader = InSourceDirectoryRoot ? InSourceDirectoryRoot->Regions.Find(InRegion) : nullptr;
	if (!FrozenHeader) FrozenHeader = &EmptyHeader;
	if (FrozenHeader->EntryCount > 512)
	{
		OutError = TEXT("Voxel冻结Region目录计数无效");
		return false;
	}
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> IndexLease;
	const uint64 IndexBytes = VoxelBoundedAllocation::ArrayBytes<uint8>(HeaderBytes) +
		VoxelBoundedAllocation::ArrayBytes<uint8>(FrozenHeader->EntryCount * IndexEntryBytes) +
		VoxelBoundedAllocation::ArrayBytes<FEntry>(FrozenHeader->EntryCount);
	if (InResources && !InResources->Acquire(InConsumer, EVoxelTerrainResourcePool::Working, IndexBytes, IndexLease))
	{
		OutError = TEXT("CapacityBlocked: 原代Region索引未授予");
		return false;
	}
	const FString SourcePath = RegionPath(InSourceDirectory, InRegion);
	const FString DestinationPath = RegionPath(InTemporaryGenerationDirectory, InRegion);
	FRegionIndex Old;
	if (!InSourceDirectory.IsEmpty())
	{
		const auto ReadStatus = ReadIndex(SourcePath, Old, OutError, FrozenHeader);
		if (ReadStatus == EVoxelRegionRead::Failed || (ReadStatus == EVoxelRegionRead::Missing && FrozenHeader->EntryCount != 0))
		{
			if (OutError.IsEmpty()) OutError = TEXT("Voxel冻结原代Region文件已丢失");
			return false;
		}
	}
	if (Old.Header.RegionRevision == MAX_uint64)
	{
		OutError = TEXT("Voxel Region内容版本耗尽");
		return false;
	}
	const FEntry* Existing[512] = {};
	for (const FEntry& Entry : Old.Entries) Existing[Entry.Local] = &Entry;
	bool Replacements[512] = {};
	uint64 Mask[8] = {};
	int32 Count = 0;
	for (int32 Local = 0; Local < 512; ++Local)
	{
		const FIntVector Section = InRegion * 8 + FIntVector(Local % 8, (Local / 8) % 8, Local / 64);
		Replacements[Local] = InHasReplacement(Section);
		if (Existing[Local] || Replacements[Local])
		{
			Mask[Local / 64] |= 1ull << (Local % 64);
			++Count;
		}
	}
	if (Count == 0)
	{
		OutError = TEXT("Voxel流式保存Region没有已提交记录");
		return false;
	}
	const int32 PayloadOffset = HeaderBytes + Count * IndexEntryBytes;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> HeaderLease;
	if (InResources && !InResources->Acquire(InConsumer, EVoxelTerrainResourcePool::Working,
		2 * VoxelBoundedAllocation::ArrayBytes<uint8>(PayloadOffset), HeaderLease))
	{
		OutError = TEXT("CapacityBlocked: 实际Region输出索引和占位区未授予");
		return false;
	}
	if (!IFileManager::Get().MakeDirectory(*FPaths::GetPath(DestinationPath), true))
	{
		OutError = TEXT("无法创建Voxel流式Region目录");
		return false;
	}
	TUniquePtr<FArchive> Writer(IFileManager::Get().CreateFileWriter(*DestinationPath));
	if (!Writer)
	{
		OutError = TEXT("无法写入Voxel流式Region");
		return false;
	}
	TArray<uint8> Placeholder;
	Placeholder.Reserve(PayloadOffset);
	Placeholder.SetNumZeroed(PayloadOffset);
	Writer->Serialize(Placeholder.GetData(), Placeholder.Num());
	TUniquePtr<FArchive> Source;
	FVoxelByteWriter Header(HeaderBytes + 512 * IndexEntryBytes);
	if (!Header.Reserve(PayloadOffset)) return false;
	Header.U32(RegionMagic);
	Header.U32(VoxelRegionFileVersion);
	Header.U32(Count);
	Header.U64(Old.Header.RegionRevision + 1);
	for (const uint64 Word : Mask) Header.U64(Word);
	for (int32 Local = 0; Local < 512; ++Local)
	{
		if (!Existing[Local] && !Replacements[Local]) continue;
		FVoxelBinaryBuffer Record;
		if (Replacements[Local])
		{
			const FIntVector Section = InRegion * 8 + FIntVector(Local % 8, (Local / 8) % 8, Local / 64);
			if (!InEncodeReplacement(Section, Record, OutError) || Record.Bytes.IsEmpty() ||
				Record.Bytes.Num() > static_cast<int32>(MaxRecordBytes) || (InResources && !Record.ResourceLease))
			{
				if (OutError.IsEmpty()) OutError = TEXT("Voxel流式增量编码无效");
				return false;
			}
		}
		else
		{
			if (InResources && !InResources->Acquire(InConsumer, EVoxelTerrainResourcePool::Working,
				VoxelBoundedAllocation::ArrayBytes<uint8>(Existing[Local]->Size), Record.ResourceLease))
			{
				OutError = TEXT("CapacityBlocked: 原代单记录复制未授予");
				return false;
			}
			if (!Source) Source.Reset(IFileManager::Get().CreateFileReader(*SourcePath));
			if (!Source || !ReadEntry(*Source, *Existing[Local], Record.Bytes))
			{
				OutError = TEXT("Voxel流式保存无法复制原代已验证记录");
				return false;
			}
		}
		Header.U16(Local);
		Header.U64(Writer->Tell());
		Header.U32(Record.Bytes.Num());
		Header.U32(FCrc::MemCrc32(Record.Bytes.GetData(), Record.Bytes.Num()));
		Writer->Serialize(Record.Bytes.GetData(), Record.Bytes.Num());
	}
	TArray<uint8> HeaderData;
	if (!Header.Finish(HeaderData) || HeaderData.Num() != PayloadOffset)
	{
		OutError = TEXT("Voxel流式Region索引长度不一致");
		return false;
	}
	Writer->Seek(0);
	Writer->Serialize(HeaderData.GetData(), HeaderData.Num());
	if (Writer->IsError() || !Writer->Close())
	{
		OutError = TEXT("Voxel流式Region写入失败");
		return false;
	}
	OutError.Reset();
	return true;
}
