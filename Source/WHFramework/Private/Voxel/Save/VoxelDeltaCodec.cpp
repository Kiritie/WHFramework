#include "Voxel/Save/VoxelDeltaCodec.h"

#include "Containers/StringConv.h"
#include "Misc/Compression.h"
#include "Voxel/Save/VoxelBlockEntityCodec.h"
#include "Voxel/Serialization/VoxelBinaryCodec.h"

namespace
{
	constexpr uint32 DeltaMagic = 0x34445856;
	constexpr int32 MaxRawBytes = 1024 * 1024;
}

uint64 FVoxelDeltaCodec::MaxEncodedBytes(
	const FVoxelRegistrySnapshot& InRegistry,
	const FVoxelPersistentSection& InSection)
{
	if (InSection.Blocks.Num() > 4096 || InSection.Entities.Num() > 256)
	{
		return 0;
	}
	uint64 RawBytes = 64 + static_cast<uint64>(InSection.Blocks.Num()) * 8;
	uint64 EntityBytes = 0;
	TSet<uint16> Types;
	for (const TPair<int32, FVoxelBlockState>& Pair : InSection.Blocks)
	{
		if (Pair.Key < 0 || Pair.Key >= 4096 || !InRegistry.IsValid(Pair.Value))
		{
			return 0;
		}
		if (!Types.Contains(Pair.Value.TypeId))
		{
			Types.Add(Pair.Value.TypeId);
			const FVoxelRuntimeDefinition* Definition = InRegistry.Find(Pair.Value.TypeId);
			if (!Definition)
			{
				return 0;
			}
			FTCHARToUTF8 Name(*Definition->BlockName.ToString());
			if (Name.Length() <= 0 || Name.Length() > 256)
			{
				return 0;
			}
			RawBytes += 4 + Name.Length();
		}
	}
	for (const TPair<int32, FVoxelBlockEntityState>& Pair : InSection.Entities)
	{
		if (Pair.Key < 0 || Pair.Key >= 4096 || !FVoxelBlockEntityCodec::Validate(Pair.Value))
		{
			return 0;
		}
		EntityBytes += Pair.Value.Payload.Num();
		RawBytes += 12 + Pair.Value.Payload.Num();
	}
	if (EntityBytes > 256 * 1024 || RawBytes > MaxRawBytes)
	{
		return 0;
	}
	const int32 Bound = FCompression::CompressMemoryBound(NAME_Zlib, static_cast<int32>(RawBytes));
	const uint64 Result = Bound > 0 ? 16 + static_cast<uint64>(Bound) : 0;
	return Result <= MaxSectionWireBytes ? Result : 0;
}

bool FVoxelDeltaCodec::Encode(
	const FVoxelWorldManifest& InWorld,
	const FVoxelRegistrySnapshot& InRegistry,
	const FVoxelPersistentSection& InSection,
	TArray<uint8>& OutBytes)
{
	if (!MaxEncodedBytes(InRegistry, InSection))
	{
		return false;
	}
	FVoxelByteWriter Writer(MaxRawBytes);
	Writer.U32(DeltaMagic);
	Writer.Guid(InWorld.WorldId);
	Writer.U64(InWorld.RecipeHash);
	Writer.I32(InSection.Section.X);
	Writer.I32(InSection.Section.Y);
	Writer.I32(InSection.Section.Z);
	Writer.U64(InSection.Revision);

	TArray<int32> Cells;
	InSection.Blocks.GetKeys(Cells);
	Cells.Sort();
	TArray<uint16> Types;
	for (const int32 Cell : Cells)
	{
		Types.AddUnique(InSection.Blocks.FindChecked(Cell).TypeId);
	}
	Types.Sort([&InRegistry](const uint16 InA, const uint16 InB)
	{
		return InRegistry.Find(InA)->BlockName.LexicalLess(InRegistry.Find(InB)->BlockName);
	});
	Writer.U16(static_cast<uint16>(Types.Num()));
	for (const uint16 Type : Types)
	{
		Writer.String(InRegistry.Find(Type)->BlockName.ToString(), 256);
	}
	Writer.U16(static_cast<uint16>(Cells.Num()));
	for (const int32 Cell : Cells)
	{
		const FVoxelBlockState& State = InSection.Blocks.FindChecked(Cell);
		Writer.U16(static_cast<uint16>(Cell));
		Writer.U16(static_cast<uint16>(Types.IndexOfByKey(State.TypeId)));
		Writer.U16(State.State);
	}
	Cells.Reset();
	InSection.Entities.GetKeys(Cells);
	Cells.Sort();
	Writer.U16(static_cast<uint16>(Cells.Num()));
	for (const int32 Cell : Cells)
	{
		const FVoxelBlockEntityState& Entity = InSection.Entities.FindChecked(Cell);
		Writer.U16(static_cast<uint16>(Cell));
		Writer.U16(Entity.Kind);
		Writer.U16(Entity.Schema);
		Writer.Blob(Entity.Payload, 32768);
	}
	TArray<uint8> Raw;
	return Writer.Finish(Raw) && VoxelBinary::Compress(Raw, OutBytes, MaxRawBytes);
}

bool FVoxelDeltaCodec::Decode(
	const TConstArrayView<uint8> InBytes,
	const FVoxelWorldManifest& InWorld,
	const FVoxelRegistrySnapshot& InRegistry,
	FVoxelPersistentSection& OutSection)
{
	TArray<uint8> Raw;
	if (!VoxelBinary::Decompress(InBytes, Raw, MaxRawBytes))
	{
		return false;
	}
	FVoxelByteReader Reader(Raw);
	if (Reader.U32() != DeltaMagic || Reader.Guid() != InWorld.WorldId || Reader.U64() != InWorld.RecipeHash)
	{
		return false;
	}
	FVoxelPersistentSection Section;
	const int32 SectionX = Reader.I32();
	const int32 SectionY = Reader.I32();
	const int32 SectionZ = Reader.I32();
	Section.Section = FIntVector(SectionX, SectionY, SectionZ);
	Section.Revision = Reader.U64();
	const uint16 PaletteCount = Reader.U16();
	if (PaletteCount > 4096)
	{
		return false;
	}
	TArray<uint16> Palette;
	TSet<FName> Names;
	for (uint16 Index = 0; Index < PaletteCount; ++Index)
	{
		const FName Name(*Reader.String(256));
		const FVoxelRuntimeDefinition* Definition = InRegistry.Find(Name);
		if (!Reader.IsValid() || !Definition || Names.Contains(Name))
		{
			return false;
		}
		Names.Add(Name);
		Palette.Add(Definition->TypeId);
	}
	const uint16 BlockCount = Reader.U16();
	for (uint16 Index = 0; Index < BlockCount; ++Index)
	{
		const int32 Cell = Reader.U16();
		const uint16 PaletteIndex = Reader.U16();
		const uint16 StateValue = Reader.U16();
		if (Cell >= 4096 || PaletteIndex >= Palette.Num() || Section.Blocks.Contains(Cell))
		{
			return false;
		}
		const FVoxelBlockState State(Palette[PaletteIndex], StateValue);
		if (!InRegistry.IsValid(State))
		{
			return false;
		}
		Section.Blocks.Add(Cell, State);
	}
	const uint16 EntityCount = Reader.U16();
	uint64 EntityBytes = 0;
	for (uint16 Index = 0; Index < EntityCount; ++Index)
	{
		const int32 Cell = Reader.U16();
		FVoxelBlockEntityState Entity;
		Entity.Kind = Reader.U16();
		Entity.Schema = Reader.U16();
		Entity.Payload = Reader.Blob(32768);
		EntityBytes += Entity.Payload.Num();
		if (Cell >= 4096 ||
			Section.Entities.Contains(Cell) ||
			EntityBytes > 256 * 1024 ||
			!FVoxelBlockEntityCodec::Validate(Entity))
		{
			return false;
		}
		Section.Entities.Add(Cell, MoveTemp(Entity));
	}
	if (!Reader.End() || !MaxEncodedBytes(InRegistry, Section))
	{
		return false;
	}
	OutSection = MoveTemp(Section);
	return true;
}
