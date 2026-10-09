#include "Voxel/Save/VoxelDeltaCodec.h"

#include "Containers/StringConv.h"
#include "Misc/StringBuilder.h"
#include "Voxel/Save/VoxelBlockEntityCodec.h"
#include "Voxel/Serialization/VoxelBinaryCodec.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"
#include "Voxel/Generation/VoxelGenerationAllocation.h"

namespace
{
	template<typename KeyType, typename ValueType>
	uint64 EmptyMapBytes(const int32 Count)
	{
		const TMap<KeyType, ValueType> Empty;
		return VoxelGenerationAllocation::Private::SetReserveBytes<TPair<KeyType, ValueType>>(Empty, Count);
	}

	bool AcquireDelta(const TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe>& Resources,
		const FVoxelGenerationConsumerKey& Consumer, const EVoxelTerrainResourcePool Pool, const uint64 Bytes,
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& OutLease, FString& OutError)
	{
		if (!Resources) return true;
		FVoxelGenerationRequiredCapacity Required;
		if (!FVoxelGenerationRequiredSourceScope::Collect(*Resources, nullptr, Required, OutError)) return false;
		const auto Limits = Resources->Snapshot().Limits;
		const uint64 PoolLimits[] = {Limits.InputBytes, Limits.WorkingBytes, Limits.StagedBytes,
			Limits.ResidentBytes - Limits.OverlapBytes, Limits.OverlapBytes};
		const auto Admission = Bytes > PoolLimits[int32(Pool)] || Required.Bytes[int32(Pool)] > PoolLimits[int32(Pool)] - Bytes ?
			EVoxelTerrainResourceAdmission::CapacityUnachievable :
			Resources->TryAcquire(Consumer, Pool, Bytes, OutLease, 0, Required.Bytes[0], Required.LeaseCount);
		if (Admission == EVoxelTerrainResourceAdmission::Granted) return true;
		OutError = Admission == EVoxelTerrainResourceAdmission::CapacityBlocked ? TEXT("CapacityBlocked: Voxel codec native layout") :
			Admission == EVoxelTerrainResourceAdmission::CapacityUnachievable ? TEXT("CapacityUnachievable: Voxel codec native layout") :
			TEXT("InvalidRequest: Voxel codec native source");
		return false;
	}

	FString CodecError(const EVoxelTerrainResourceAdmission Admission)
	{
		return Admission == EVoxelTerrainResourceAdmission::CapacityBlocked ? TEXT("CapacityBlocked: Voxel原生codec分配未授予") :
			Admission == EVoxelTerrainResourceAdmission::CapacityUnachievable ? TEXT("CapacityUnachievable: Voxel原生codec必需工作区") :
			Admission == EVoxelTerrainResourceAdmission::InvalidRequest ? TEXT("InvalidRequest: Voxel原生codec资源来源") :
			TEXT("Voxel原生codec或冻结计数失败");
	}
	constexpr uint32 DeltaMagic = 0x34445856;
	constexpr int32 MaxRawBytes = FVoxelDeltaCodec::MaximumRawBytes;
	bool DecodeRaw(TConstArrayView<uint8> InRaw, const FVoxelWorldManifest& InWorld,
		const FVoxelRegistrySnapshot& InRegistry, FVoxelPersistentSection& OutSection);

	struct FVoxelDeltaEncodeMeasure
	{
		int32 RawBytes = 54;
		int32 TypeCount = 0;
		int32 WireBytes = 0;
	};

	struct FVoxelDeltaPaletteIndex
	{
		uint16 Type = 0;
		uint16 Palette = 0;
	};

	bool MeasureDeltaEncode(const FVoxelRegistrySnapshot& InRegistry, const FVoxelDeltaEncodeView& InSection,
		FVoxelDeltaEncodeMeasure& OutMeasure)
	{
		if (InSection.Blocks.Num() > 4096 || InSection.Entities.Num() > FVoxelDeltaCodec::MaximumEntityCount) return false;
		FVoxelDeltaEncodeMeasure Measure;
		Measure.RawBytes += InSection.Blocks.Num() * 6;
		uint64 TypeMask[1024] = {};
		for (const auto& Pair : InSection.Blocks)
		{
			if (Pair.Key < 0 || Pair.Key >= 4096 || !InRegistry.IsValid(Pair.Value)) return false;
			const uint64 Bit = 1ull << (Pair.Value.TypeId % 64);
			uint64& Word = TypeMask[Pair.Value.TypeId / 64];
			if (Word & Bit) continue;
			Word |= Bit;
			const auto* Definition = InRegistry.Find(Pair.Value.TypeId);
			if (!Definition || Definition->BlockName.GetStringLength() > 256) return false;
			TUtf8StringBuilder<1025> Name;
			Definition->BlockName.AppendString(Name);
			if (Name.Len() <= 0 || Name.Len() > 256) return false;
			Measure.RawBytes += 4 + Name.Len();
			++Measure.TypeCount;
		}
		int32 EntityBytes = 0;
		for (const auto& Pair : InSection.Entities)
		{
			if (Pair.Key < 0 || Pair.Key >= 4096 || !FVoxelBlockEntityCodec::Validate(Pair.Value)) return false;
			EntityBytes += Pair.Value.Payload.Num();
			Measure.RawBytes += 10 + Pair.Value.Payload.Num();
			if (EntityBytes > 256 * 1024 || Measure.RawBytes > MaxRawBytes) return false;
		}
		if (Measure.RawBytes > MaxRawBytes) return false;
		const int32 Bound = VoxelBinary::CompressionBound(Measure.RawBytes);
		if (Bound <= 0 || int64(Bound) + 16 > FVoxelDeltaCodec::MaxSectionWireBytes) return false;
		Measure.WireBytes = Bound + 16;
		OutMeasure = Measure;
		return true;
	}

	bool EncodeDeltaRaw(const FVoxelWorldManifest& InWorld, const FVoxelRegistrySnapshot& InRegistry,
		const FVoxelDeltaEncodeView& InSection, const FVoxelDeltaEncodeMeasure& InMeasure, TArray<uint8>& OutRaw)
	{
		FVoxelByteWriter Writer(MaxRawBytes);
		if (!Writer.Reserve(InMeasure.RawBytes)) return false;
		Writer.U32(DeltaMagic);
		Writer.Guid(InWorld.WorldId);
		Writer.U64(InWorld.RecipeHash);
		Writer.I32(InSection.Section.X);
		Writer.I32(InSection.Section.Y);
		Writer.I32(InSection.Section.Z);
		Writer.U64(InSection.Revision);
		TArray<int32> Cells;
		Cells.Reserve(FMath::Max(InSection.Blocks.Num(), InSection.Entities.Num()));
		TArray<uint16> Types;
		Types.Reserve(InMeasure.TypeCount);
		uint64 TypeMask[1024] = {};
		for (const auto& Pair : InSection.Blocks)
		{
			Cells.Add(Pair.Key);
			const uint64 Bit = 1ull << (Pair.Value.TypeId % 64);
			uint64& Word = TypeMask[Pair.Value.TypeId / 64];
			if (Word & Bit) continue;
			Word |= Bit;
			Types.Add(Pair.Value.TypeId);
		}
		Cells.Sort();
		Types.Sort([&InRegistry](const uint16 InA, const uint16 InB)
		{
			return InRegistry.Find(InA)->BlockName.LexicalLess(InRegistry.Find(InB)->BlockName);
		});
		TArray<FVoxelDeltaPaletteIndex> Lookup;
		Lookup.Reserve(InMeasure.TypeCount);
		for (int32 Index = 0; Index < Types.Num(); ++Index) Lookup.Add({Types[Index], uint16(Index)});
		Lookup.Sort([](const FVoxelDeltaPaletteIndex& InA, const FVoxelDeltaPaletteIndex& InB)
		{
			return InA.Type < InB.Type;
		});
		Writer.U16(uint16(Types.Num()));
		for (const uint16 Type : Types)
		{
			TUtf8StringBuilder<1025> Name;
			InRegistry.Find(Type)->BlockName.AppendString(Name);
			Writer.Blob(MakeArrayView(reinterpret_cast<const uint8*>(Name.GetData()), Name.Len()), 256);
		}
		Writer.U16(uint16(Cells.Num()));
		for (const int32 Cell : Cells)
		{
			const FVoxelBlockState& State = InSection.Blocks.FindChecked(Cell);
			int32 Lower = 0;
			int32 Upper = Lookup.Num();
			while (Lower < Upper)
			{
				const int32 Middle = Lower + (Upper - Lower) / 2;
				if (Lookup[Middle].Type < State.TypeId) Lower = Middle + 1;
				else Upper = Middle;
			}
			if (!Lookup.IsValidIndex(Lower) || Lookup[Lower].Type != State.TypeId) return false;
			Writer.U16(uint16(Cell));
			Writer.U16(Lookup[Lower].Palette);
			Writer.U16(State.State);
		}
		Cells.Reset();
		for (const auto& Pair : InSection.Entities) Cells.Add(Pair.Key);
		Cells.Sort();
		Writer.U16(uint16(Cells.Num()));
		for (const int32 Cell : Cells)
		{
			const FVoxelBlockEntityState& Entity = InSection.Entities.FindChecked(Cell);
			Writer.U16(uint16(Cell));
			Writer.U16(Entity.Kind);
			Writer.U16(Entity.Schema);
			Writer.Blob(Entity.Payload, FVoxelDeltaCodec::MaximumEntityPayloadBytes);
		}
		return Writer.Finish(OutRaw) && OutRaw.Num() == InMeasure.RawBytes;
	}

	bool MeasureDecodedStorage(TConstArrayView<uint8> InRaw, const FVoxelWorldManifest& InWorld,
		uint64& OutBytes, uint64& OutScratchBytes)
	{
		FVoxelByteReader Reader(InRaw);
		if (Reader.U32() != DeltaMagic || Reader.Guid() != InWorld.WorldId || Reader.U64() != InWorld.RecipeHash) return false;
		Reader.I32(); Reader.I32(); Reader.I32(); Reader.U64();
		const uint16 PaletteCount = Reader.U16();
		if (PaletteCount > 4096) return false;
		for (uint16 Index = 0; Index < PaletteCount; ++Index)
		{
			const auto Name = Reader.BlobView(256);
			if (!Reader.IsValid() || Name.IsEmpty()) return false;
		}
		const uint16 BlockCount = Reader.U16();
		if (BlockCount > 4096 || !Reader.IsValid()) return false;
		Reader.View(int32(BlockCount) * 6);
		const uint16 EntityCount = Reader.U16();
		if (EntityCount > FVoxelDeltaCodec::MaximumEntityCount || !Reader.IsValid()) return false;
		uint64 PayloadBytes = 0;
		uint64 PayloadStorage = 0;
		for (uint16 Index = 0; Index < EntityCount; ++Index)
		{
			Reader.U16(); Reader.U16(); Reader.U16();
			const auto Payload = Reader.BlobView(FVoxelDeltaCodec::MaximumEntityPayloadBytes);
			PayloadBytes += Payload.Num();
			if (!Reader.IsValid() || PayloadBytes > 256 * 1024) return false;
			PayloadStorage += VoxelGenerationAllocation::Private::ArrayReserveBytes<uint8, FDefaultAllocator>(Payload.Num());
		}
		if (!Reader.End()) return false;
		OutBytes = VoxelBoundedAllocation::WorkerSharedBytes<FVoxelPersistentSection>() +
			EmptyMapBytes<int32, FVoxelBlockState>(BlockCount) + EmptyMapBytes<int32, FVoxelBlockEntityState>(EntityCount) + PayloadStorage;
		OutScratchBytes = VoxelGenerationAllocation::Private::ArrayReserveBytes<uint16, FDefaultAllocator>(PaletteCount);
		return true;
	}
}

uint64 FVoxelDeltaCodec::MaxEncodedBytes(
	const FVoxelRegistrySnapshot& InRegistry,
	const FVoxelPersistentSection& InSection)
{
	return MaxEncodedBytes(InRegistry, FVoxelDeltaEncodeView{InSection.Section, InSection.Revision, InSection.Blocks, InSection.Entities});
}

uint64 FVoxelDeltaCodec::MaxEncodedBytes(
	const FVoxelRegistrySnapshot& InRegistry,
	const FVoxelDeltaEncodeView& InSection)
{
	FVoxelDeltaEncodeMeasure Measure;
	return MeasureDeltaEncode(InRegistry, InSection, Measure) ? uint64(Measure.WireBytes) : 0;
}

bool FVoxelDeltaCodec::Encode(
	const FVoxelWorldManifest& InWorld,
	const FVoxelRegistrySnapshot& InRegistry,
	const FVoxelPersistentSection& InSection,
	TArray<uint8>& OutBytes)
{
	return Encode(InWorld, InRegistry,
		FVoxelDeltaEncodeView{InSection.Section, InSection.Revision, InSection.Blocks, InSection.Entities}, OutBytes);
}

bool FVoxelDeltaCodec::Encode(
	const FVoxelWorldManifest& InWorld,
	const FVoxelRegistrySnapshot& InRegistry,
	const FVoxelDeltaEncodeView& InSection,
	TArray<uint8>& OutBytes)
{
	FVoxelDeltaEncodeMeasure Measure;
	TArray<uint8> Raw;
	return MeasureDeltaEncode(InRegistry, InSection, Measure) &&
		EncodeDeltaRaw(InWorld, InRegistry, InSection, Measure, Raw) &&
		VoxelBinary::Compress(Raw, OutBytes, MaxRawBytes);
}

bool FVoxelDeltaCodec::EncodeGranted(const FVoxelWorldManifest& InWorld, const FVoxelRegistrySnapshot& InRegistry,
	const FVoxelDeltaEncodeView& InSection,
	const TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe>& InResources,
	const FVoxelGenerationConsumerKey& InConsumer, FVoxelBinaryBuffer& OutBuffer, FString& OutError)
{
	FVoxelDeltaEncodeMeasure Measure;
	if (!MeasureDeltaEncode(InRegistry, InSection, Measure))
	{
		OutError = TEXT("Voxel固定增量计数或实体校验失败");
		return false;
	}
	const uint64 ScratchBytes = VoxelGenerationAllocation::Private::ArrayReserveBytes<uint8, FDefaultAllocator>(Measure.RawBytes) +
		VoxelGenerationAllocation::Private::ArrayReserveBytes<int32, FDefaultAllocator>(FMath::Max(InSection.Blocks.Num(), InSection.Entities.Num())) +
		VoxelGenerationAllocation::Private::ArrayReserveBytes<uint16, FDefaultAllocator>(Measure.TypeCount) +
		VoxelGenerationAllocation::Private::ArrayReserveBytes<FVoxelDeltaPaletteIndex, FDefaultAllocator>(Measure.TypeCount);
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ScratchLease;
	if (!AcquireDelta(InResources, InConsumer, EVoxelTerrainResourcePool::Working, ScratchBytes, ScratchLease, OutError)) return false;
	const FVoxelGenerationRequiredSourceScope ScratchSource(InResources.Get(), ScratchLease.Get());
	TArray<uint8> Raw;
	if (!EncodeDeltaRaw(InWorld, InRegistry, InSection, Measure, Raw))
	{
		OutError = TEXT("Voxel固定增量编码与冻结计数不一致");
		return false;
	}
	if (ScratchLease)
	{
		check(VoxelGenerationAllocation::AllocatedBytes(Raw) <= ScratchLease->Bytes());
		verify(ScratchLease->Resize(VoxelGenerationAllocation::AllocatedBytes(Raw)));
	}
	bool bCapacityBlocked = false;
	EVoxelTerrainResourceAdmission Admission;
	if (!VoxelBinary::CompressGranted(Raw, OutBuffer, InResources, InConsumer, MaximumRawBytes,
		int32(MaxSectionWireBytes), bCapacityBlocked, &Admission))
	{
		OutError = CodecError(Admission);
		return false;
	}
	OutError.Reset();
	return true;
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
	return DecodeRaw(Raw, InWorld, InRegistry, OutSection);
}

namespace
{
bool DecodeRaw(const TConstArrayView<uint8> InRaw, const FVoxelWorldManifest& InWorld,
	const FVoxelRegistrySnapshot& InRegistry, FVoxelPersistentSection& OutSection)
{
	FVoxelByteReader Reader(InRaw);
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
	uint64 TypeMask[1024] = {};
	Palette.Reserve(PaletteCount);
	for (uint16 Index = 0; Index < PaletteCount; ++Index)
	{
		const auto NameBytes = Reader.StringView(256);
		if (!Reader.IsValid() || NameBytes.IsEmpty()) return false;
		TCHAR NameCharacters[257];
		const auto* Utf8 = reinterpret_cast<const ANSICHAR*>(NameBytes.GetData());
		const int32 CharacterCount = FUTF8ToTCHAR_Convert::ConvertedLength(Utf8, NameBytes.Num());
		if (CharacterCount <= 0 || CharacterCount > 256) return false;
		FUTF8ToTCHAR_Convert::Convert(NameCharacters, UE_ARRAY_COUNT(NameCharacters), Utf8, NameBytes.Num());
		NameCharacters[CharacterCount] = 0;
		const FName Name(CharacterCount, NameCharacters, FNAME_Find);
		const FVoxelRuntimeDefinition* Definition = InRegistry.Find(Name);
		if (Name.IsNone() || !Definition || (TypeMask[Definition->TypeId / 64] & (1ull << (Definition->TypeId % 64))))
		{
			return false;
		}
		TypeMask[Definition->TypeId / 64] |= 1ull << (Definition->TypeId % 64);
		Palette.Add(Definition->TypeId);
	}
	const uint16 BlockCount = Reader.U16();
	if (BlockCount > 4096) return false;
	Section.Blocks.Reserve(BlockCount);
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
	if (EntityCount > FVoxelDeltaCodec::MaximumEntityCount) return false;
	Section.Entities.Reserve(EntityCount);
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
	if (!Reader.End() || !FVoxelDeltaCodec::MaxEncodedBytes(InRegistry, Section))
	{
		return false;
	}
	OutSection = MoveTemp(Section);
	return true;
}
}

bool FVoxelDeltaDecoder::Advance(const TConstArrayView<uint8> InBytes, const FVoxelWorldManifest& InWorld,
	const FVoxelRegistrySnapshot& InRegistry, TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
	const FVoxelGenerationConsumerKey& InConsumer, const EVoxelTerrainResourcePool InOutputPool,
	TSharedPtr<FVoxelPersistentSection, ESPMode::ThreadSafe>& OutSection, FString& OutError)
{
	if (Raw.IsEmpty())
	{
		FVoxelByteReader Header(InBytes);
		const uint32 Magic = Header.U32();
		const uint32 RawSize = Header.U32();
		Header.U32();
		const auto Compressed = Header.BlobView(FVoxelDeltaCodec::MaximumRawBytes + 65536);
		if (!Header.End() || Magic != 0x315a5856 || !RawSize || RawSize > FVoxelDeltaCodec::MaximumRawBytes || Compressed.IsEmpty())
		{
			OutError = TEXT("Voxel压缩增量头无效");
			return false;
		}
		const uint64 ScratchBytes = VoxelGenerationAllocation::Private::ArrayReserveBytes<uint8, FDefaultAllocator>(int32(RawSize));
		if (!AcquireDelta(InResources, InConsumer, EVoxelTerrainResourcePool::Working, ScratchBytes, WorkingLease, OutError)) return false;
		const FVoxelGenerationRequiredSourceScope WorkingSource(&InResources.Get(), WorkingLease.Get());
		bool bCapacityBlocked = false;
		EVoxelTerrainResourceAdmission Admission;
		if (!VoxelBinary::DecompressGranted(InBytes, Raw, InResources, InConsumer, FVoxelDeltaCodec::MaximumRawBytes,
			bCapacityBlocked, &Admission) || !MeasureDecodedStorage(Raw, InWorld, OutputBytes, DecodeWorkingBytes))
		{
			Raw.Empty();
			WorkingLease.Reset();
			OutError = CodecError(Admission);
			return false;
		}
	}
	const FVoxelGenerationRequiredSourceScope WorkingSource(&InResources.Get(), WorkingLease.Get());
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> DecodeLease;
	if (DecodeWorkingBytes && !AcquireDelta(InResources, InConsumer, EVoxelTerrainResourcePool::Working,
		DecodeWorkingBytes, DecodeLease, OutError)) return false;
	const FVoxelGenerationRequiredSourceScope DecodeSource(&InResources.Get(), DecodeLease.Get());
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> OutputLease;
	if (!AcquireDelta(InResources, InConsumer, InOutputPool, OutputBytes, OutputLease, OutError)) return false;
	auto Candidate = VoxelBoundedAllocation::MakeWorkerShared<FVoxelPersistentSection>(OutputLease);
	if (!DecodeRaw(Raw, InWorld, InRegistry, *Candidate))
	{
		OutError = TEXT("Voxel增量内容校验失败");
		return false;
	}
	Candidate->ResourceLease = MoveTemp(OutputLease);
	uint64 Actual = VoxelBoundedAllocation::WorkerSharedBytes<FVoxelPersistentSection>() +
		VoxelGenerationAllocation::AllocatedBytes(Candidate->Blocks) + VoxelGenerationAllocation::AllocatedBytes(Candidate->Entities);
	for (const auto& Pair : Candidate->Entities) Actual += VoxelGenerationAllocation::AllocatedBytes(Pair.Value.Payload);
	check(Actual <= Candidate->ResourceLease->Bytes());
	verify(Candidate->ResourceLease->Resize(Actual));
	OutSection = MoveTemp(Candidate);
	Raw.Empty();
	WorkingLease.Reset();
	OutputBytes = 0;
	DecodeWorkingBytes = 0;
	OutError.Reset();
	return true;
}

uint64 FVoxelDeltaDecoder::GetRetainedWorkingBytes() const
{
	return VoxelGenerationAllocation::AllocatedBytes(Raw);
}
