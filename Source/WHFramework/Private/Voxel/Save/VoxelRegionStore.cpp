#include "Voxel/Save/VoxelRegionStore.h"

#include "HAL/FileManager.h"
#include "Misc/Compression.h"
#include "Misc/Crc.h"
#include "Misc/Paths.h"
#include "SaveGame/SaveDataSerializer.h"
#include "SaveGame/SaveGameStorage.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

void FVoxelRegionStore::SetSource(const FGuid& SaveId, int32 Generation, FSaveGameStorage* Storage)
{
	SourceSaveId = SaveId;
	SourceGeneration = Generation;
	SourceStorage = Storage;
	PendingReplacements.Reset();
}

void FVoxelRegionStore::Reset()
{
	SourceSaveId.Invalidate();
	SourceGeneration = 0;
	SourceStorage = nullptr;
	PendingReplacements.Reset();
}

int32 FVoxelRegionStore::FloorDiv(int32 Value, int32 Divisor)
{
	check(Divisor > 0);
	int32 Quotient = Value / Divisor;
	const int32 Remainder = Value % Divisor;
	if(Remainder != 0 && Value < 0)
	{
		--Quotient;
	}
	return Quotient;
}

FVoxelRegionCoord FVoxelRegionStore::GetRegionCoord(const FIndex& ChunkIndex)
{
	return { FloorDiv(ChunkIndex.X, RegionSize), FloorDiv(ChunkIndex.Y, RegionSize), FloorDiv(ChunkIndex.Z, RegionSize) };
}

FString FVoxelRegionStore::GetRegionFileName(const FVoxelRegionCoord& Coord)
{
	return FString::Printf(TEXT("r_%d_%d_%d.bin"), Coord.X, Coord.Y, Coord.Z);
}

FString FVoxelRegionStore::GetSourceRegionPath(const FVoxelRegionCoord& Coord) const
{
	return SourceStorage ? FPaths::Combine(SourceStorage->GetGenerationDir(SourceSaveId, SourceGeneration), TEXT("voxel"), TEXT("regions"), GetRegionFileName(Coord)) : FString();
}

bool FVoxelRegionStore::HasChunk(const FIndex& ChunkIndex) const
{
	FParameter Data;
	return LoadChunk(ChunkIndex, Data);
}

bool FVoxelRegionStore::LoadChunk(const FIndex& ChunkIndex, FParameter& OutData) const
{
	const FVoxelRegionCoord Coord = GetRegionCoord(ChunkIndex);
	if(const TMap<FIndex, FParameter>* Pending = PendingReplacements.Find(Coord))
	{
		if(const FParameter* Data = Pending->Find(ChunkIndex))
		{
			OutData = *Data;
			return true;
		}
	}
	TMap<FIndex, FParameter> Chunks;
	if(!ReadRegion(GetSourceRegionPath(Coord), Chunks))
	{
		return false;
	}
	if(const FParameter* Data = Chunks.Find(ChunkIndex))
	{
		OutData = *Data;
		return true;
	}
	return false;
}

void FVoxelRegionStore::StageChunk(const FIndex& ChunkIndex, const FParameter& Data)
{
	if(Data.HasValue())
	{
		PendingReplacements.FindOrAdd(GetRegionCoord(ChunkIndex)).Add(ChunkIndex, Data);
	}
}

bool FVoxelRegionStore::WritePendingRegions(const FString& TempGenerationDir)
{
	for(const TPair<FVoxelRegionCoord, TMap<FIndex, FParameter>>& Pending : PendingReplacements)
	{
		TMap<FIndex, FParameter> Chunks;
		ReadRegion(FPaths::Combine(TempGenerationDir, TEXT("voxel"), TEXT("regions"), GetRegionFileName(Pending.Key)), Chunks);
		for(const TPair<FIndex, FParameter>& Replacement : Pending.Value)
		{
			Chunks.Add(Replacement.Key, Replacement.Value);
		}
		if(!WriteRegion(FPaths::Combine(TempGenerationDir, TEXT("voxel"), TEXT("regions"), GetRegionFileName(Pending.Key)), Chunks))
		{
			return false;
		}
	}
	return true;
}

void FVoxelRegionStore::CommitPending()
{
	PendingReplacements.Reset();
}

void FVoxelRegionStore::AbortPending()
{
}

bool FVoxelRegionStore::ReadRegion(const FString& Path, TMap<FIndex, FParameter>& OutChunks) const
{
	TArray<uint8> Bytes;
	if(Path.IsEmpty() || !FFileHelper::LoadFileToArray(Bytes, *Path))
	{
		return false;
	}
	FMemoryReader Reader(Bytes, true);
	uint32 Magic = 0;
	int32 Version = 0;
	int32 Count = 0;
	Reader << Magic << Version << Count;
	if(Magic != MagicValue || Version != 1 || Count < 0)
	{
		return false;
	}
	for(int32 Index = 0; Index < Count; ++Index)
	{
		FIndex ChunkIndex;
		int32 CompressedSize = 0;
		int32 RawSize = 0;
		uint32 Crc = 0;
		Reader << ChunkIndex.X << ChunkIndex.Y << ChunkIndex.Z << CompressedSize << RawSize << Crc;
		if(CompressedSize <= 0 || RawSize <= 0 || CompressedSize > Reader.TotalSize() - Reader.Tell())
		{
			return false;
		}
		TArray<uint8> Compressed;
		Compressed.SetNumUninitialized(CompressedSize);
		Reader.Serialize(Compressed.GetData(), CompressedSize);
		TArray<uint8> Raw;
		Raw.SetNumUninitialized(RawSize);
		if(!FCompression::UncompressMemory(NAME_Zlib, Raw.GetData(), RawSize, Compressed.GetData(), CompressedSize) || FCrc::MemCrc32(Raw.GetData(), Raw.Num()) != Crc)
		{
			return false;
		}
		FParameter Data;
		if(!FSaveDataSerializer::DeserializeParameter(Raw, Data))
		{
			return false;
		}
		OutChunks.Add(ChunkIndex, MoveTemp(Data));
	}
	return !Reader.IsError();
}

bool FVoxelRegionStore::WriteRegion(const FString& Path, const TMap<FIndex, FParameter>& Chunks) const
{
	TArray<uint8> Bytes;
	FMemoryWriter Writer(Bytes, true);
	uint32 Magic = MagicValue;
	int32 Version = 1;
	int32 Count = Chunks.Num();
	Writer << Magic << Version << Count;
	for(const TPair<FIndex, FParameter>& Chunk : Chunks)
	{
		TArray<uint8> Raw;
		if(!FSaveDataSerializer::SerializeParameter(Chunk.Value, Raw))
		{
			return false;
		}
		const int32 Bound = FCompression::CompressMemoryBound(NAME_Zlib, Raw.Num());
		TArray<uint8> Compressed;
		Compressed.SetNumUninitialized(Bound);
		int32 CompressedSize = Bound;
		if(!FCompression::CompressMemory(NAME_Zlib, Compressed.GetData(), CompressedSize, Raw.GetData(), Raw.Num()))
		{
			return false;
		}
		Compressed.SetNum(CompressedSize);
		FIndex ChunkIndex = Chunk.Key;
		int32 RawSize = Raw.Num();
		uint32 Crc = FCrc::MemCrc32(Raw.GetData(), Raw.Num());
		Writer << ChunkIndex.X << ChunkIndex.Y << ChunkIndex.Z << CompressedSize << RawSize << Crc;
		Writer.Serialize(Compressed.GetData(), Compressed.Num());
	}
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	return !Writer.IsError() && FFileHelper::SaveArrayToFile(Bytes, *Path);
}
