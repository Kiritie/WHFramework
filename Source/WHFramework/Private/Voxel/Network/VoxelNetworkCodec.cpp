#include "Voxel/Network/VoxelNetworkCodec.h"
#include "Voxel/Save/InventoryVoxelEntityCodec.h"
#include "Voxel/Network/VoxelGeometryWire.h"
#include "Voxel/Generation/VoxelManifestCodec.h"

namespace
{
	constexpr uint32 WireMagic = 0x34544E56;
	constexpr int32 MaxEntityPayloadBytes = 256 * 1024;

	bool IsMessageValue(const uint8 InValue)
	{
		return InValue >= static_cast<uint8>(EVoxelMessage::Hello) &&
			InValue <= static_cast<uint8>(EVoxelMessage::ResyncRequest);
	}
}

bool FVoxelNetworkCodec::Encode(
	const EVoxelMessage InKind,
	const FGuid& InSession,
	TConstArrayView<uint8> InPayload,
	TArray<uint8>& OutBytes)
{
	if (!IsMessageValue(static_cast<uint8>(InKind)) || InPayload.Num() > MaxWireBytes - 64)
	{
		return false;
	}
	FVoxelByteWriter Writer(MaxWireBytes);
	Writer.Reserve(InPayload.Num() + 29);
	Writer.U32(WireMagic);
	Writer.U32(VoxelProtocolVersion);
	Writer.U8(static_cast<uint8>(InKind));
	Writer.Guid(InSession);
	Writer.Blob(InPayload, MaxWireBytes - 64);
	return Writer.Finish(OutBytes);
}

bool FVoxelNetworkCodec::Decode(
	TConstArrayView<uint8> InBytes,
	FVoxelWireMessage& OutMessage)
{
	FVoxelWireMessage Value;
	TConstArrayView<uint8> Payload;
	if (!DecodeView(InBytes, Value.Kind, Value.Session, Payload)) return false;
	Value.Payload.Reserve(Payload.Num());
	Value.Payload.Append(Payload.GetData(), Payload.Num());
	OutMessage = MoveTemp(Value);
	return true;
}

bool FVoxelNetworkCodec::DecodeView(TConstArrayView<uint8> InBytes, EVoxelMessage& OutKind,
	FGuid& OutSession, TConstArrayView<uint8>& OutPayload)
{
	if (InBytes.Num() > MaxWireBytes)
	{
		return false;
	}
	FVoxelByteReader Reader(InBytes);
	if (Reader.U32() != WireMagic || Reader.U32() != VoxelProtocolVersion)
	{
		return false;
	}
	const uint8 Kind = Reader.U8();
	if (!IsMessageValue(Kind))
	{
		return false;
	}
	const FGuid Session = Reader.Guid();
	const auto Payload = Reader.BlobView(MaxWireBytes - 64);
	if (!Reader.End())
	{
		return false;
	}
	OutKind = static_cast<EVoxelMessage>(Kind);
	OutSession = Session;
	OutPayload = Payload;
	return true;
}

void FVoxelNetworkCodec::WriteVector(
	FVoxelByteWriter& InWriter,
	const FIntVector& InValue)
{
	InWriter.I32(InValue.X);
	InWriter.I32(InValue.Y);
	InWriter.I32(InValue.Z);
}

FIntVector FVoxelNetworkCodec::ReadVector(FVoxelByteReader& InReader)
{
	const int32 X = InReader.I32();
	const int32 Y = InReader.I32();
	const int32 Z = InReader.I32();
	return FIntVector(X, Y, Z);
}

bool FVoxelNetworkCodec::EncodeHello(
	const FVoxelNetworkHello& InValue,
	TArray<uint8>& OutBytes)
{
	if (InValue.ProtocolVersion != VoxelProtocolVersion || InValue.Nonce == 0 || InValue.RegistryHash == 0)
	{
		return false;
	}
	FVoxelByteWriter Writer(32);
	Writer.Reserve(20);
	Writer.U32(InValue.ProtocolVersion);
	Writer.U64(InValue.Nonce);
	Writer.U64(InValue.RegistryHash);
	return Writer.Finish(OutBytes);
}

bool FVoxelNetworkCodec::DecodeHello(
	TConstArrayView<uint8> InBytes,
	FVoxelNetworkHello& OutValue)
{
	FVoxelByteReader Reader(InBytes);
	FVoxelNetworkHello Value;
	Value.ProtocolVersion = Reader.U32();
	Value.Nonce = Reader.U64();
	Value.RegistryHash = Reader.U64();
	if (!Reader.End() ||
		Value.ProtocolVersion != VoxelProtocolVersion ||
		Value.Nonce == 0 ||
		Value.RegistryHash == 0)
	{
		return false;
	}
	OutValue = Value;
	return true;
}

bool FVoxelNetworkCodec::EncodeWelcome(
	const FVoxelNetworkWelcome& InValue,
	TArray<uint8>& OutBytes)
{
	TArray<uint8> ManifestBytes;
	if (InValue.EchoNonce == 0 ||
		InValue.AuthorityWorldEpoch == 0 ||
		!InValue.Session.IsValid() ||
		!FVoxelManifestCodec::Encode(InValue.Manifest, ManifestBytes))
	{
		return false;
	}
	FVoxelByteWriter Writer(FVoxelManifestCodec::MaxBytes + 64);
	Writer.Reserve(FVoxelManifestCodec::MaxBytes + 64);
	Writer.U64(InValue.EchoNonce);
	Writer.Guid(InValue.Session);
	Writer.U64(InValue.AuthorityWorldEpoch);
	Writer.Blob(ManifestBytes, FVoxelManifestCodec::MaxBytes);
	return Writer.Finish(OutBytes);
}

bool FVoxelNetworkCodec::DecodeWelcome(
	const TConstArrayView<uint8> InBytes,
	FVoxelNetworkWelcome& OutValue)
{
	FVoxelByteReader Reader(InBytes);
	FVoxelNetworkWelcome Value;
	Value.EchoNonce = Reader.U64();
	Value.Session = Reader.Guid();
	Value.AuthorityWorldEpoch = Reader.U64();
	const auto ManifestBytes = Reader.BlobView(FVoxelManifestCodec::MaxBytes);
	if (!Reader.End() ||
		Value.EchoNonce == 0 ||
		Value.AuthorityWorldEpoch == 0 ||
		!Value.Session.IsValid() ||
		!FVoxelManifestCodec::Decode(ManifestBytes, Value.Manifest))
	{
		return false;
	}
	OutValue = MoveTemp(Value);
	return true;
}

bool FVoxelNetworkCodec::EncodeReady(
	const FVoxelNetworkReady& InValue,
	TArray<uint8>& OutBytes)
{
	if (InValue.RecipeHash == 0 || InValue.RegistryHash == 0 || InValue.BaseSampleHash == 0)
	{
		return false;
	}
	FVoxelByteWriter Writer(32);
	Writer.Reserve(24);
	Writer.U64(InValue.RecipeHash);
	Writer.U64(InValue.RegistryHash);
	Writer.U64(InValue.BaseSampleHash);
	return Writer.Finish(OutBytes);
}

bool FVoxelNetworkCodec::DecodeReady(
	TConstArrayView<uint8> InBytes,
	FVoxelNetworkReady& OutValue)
{
	FVoxelByteReader Reader(InBytes);
	FVoxelNetworkReady Value;
	Value.RecipeHash = Reader.U64();
	Value.RegistryHash = Reader.U64();
	Value.BaseSampleHash = Reader.U64();
	if (!Reader.End() ||
		Value.RecipeHash == 0 ||
		Value.RegistryHash == 0 ||
		Value.BaseSampleHash == 0)
	{
		return false;
	}
	OutValue = Value;
	return true;
}

bool FVoxelNetworkCodec::EncodeChangeSummary(
	const FVoxelRegionChangeSummary& InValue,
	TArray<uint8>& OutBytes,
	FString& OutError)
{
	if (InValue.ModifiedMask.Num() != 8)
	{
		OutError = TEXT("Voxel change summary mask must contain exactly 8 uint64 values");
		return false;
	}
	FVoxelByteWriter Writer(96);
	WriteVector(Writer, InValue.Region);
	Writer.U64(InValue.Revision);
	for (const uint64 Word : InValue.ModifiedMask)
	{
		Writer.U64(Word);
	}
	if (!Writer.Finish(OutBytes))
	{
		OutError = TEXT("Voxel change summary encoding failed");
		return false;
	}
	OutError.Reset();
	return true;
}

bool FVoxelNetworkCodec::DecodeChangeSummary(
	TConstArrayView<uint8> InBytes,
	FVoxelRegionChangeSummary& OutValue,
	FString& OutError)
{
	FVoxelByteReader Reader(InBytes);
	FVoxelRegionChangeSummary Value;
	Value.Region = ReadVector(Reader);
	Value.Revision = Reader.U64();
	Value.ModifiedMask.SetNumUninitialized(8);
	for (uint64& Word : Value.ModifiedMask)
	{
		Word = Reader.U64();
	}
	if (!Reader.End())
	{
		OutError = TEXT("Voxel change summary payload is invalid");
		return false;
	}
	OutValue = MoveTemp(Value);
	OutError.Reset();
	return true;
}

bool FVoxelNetworkCodec::EncodeSectionState(
	const FVoxelSectionState& InValue,
	TArray<uint8>& OutBytes)
{
	if (InValue.State == EVoxelSectionWireState::Unknown ||
		(InValue.State == EVoxelSectionWireState::Natural && InValue.Revision != 0) ||
		(InValue.State == EVoxelSectionWireState::Modified && InValue.Revision == 0))
	{
		return false;
	}
	FVoxelByteWriter Writer(32);
	WriteVector(Writer, InValue.Section);
	Writer.U8(static_cast<uint8>(InValue.State));
	Writer.U64(InValue.Revision);
	return Writer.Finish(OutBytes);
}

bool FVoxelNetworkCodec::DecodeSectionState(
	TConstArrayView<uint8> InBytes,
	FVoxelSectionState& OutValue)
{
	FVoxelByteReader Reader(InBytes);
	FVoxelSectionState Value;
	Value.Section = ReadVector(Reader);
	Value.State = static_cast<EVoxelSectionWireState>(Reader.U8());
	Value.Revision = Reader.U64();
	if (!Reader.End() ||
		Value.State == EVoxelSectionWireState::Unknown ||
		static_cast<uint8>(Value.State) > static_cast<uint8>(EVoxelSectionWireState::Modified) ||
		(Value.State == EVoxelSectionWireState::Natural && Value.Revision != 0) ||
		(Value.State == EVoxelSectionWireState::Modified && Value.Revision == 0))
	{
		return false;
	}
	OutValue = Value;
	return true;
}

bool FVoxelNetworkCodec::EncodeSectionSnapshot(
	const FVoxelNetworkSectionSnapshot& InValue,
	TArray<uint8>& OutBytes,
	FString& OutError)
{
	if (InValue.Revision == 0 ||
		InValue.Blocks.Num() > MaxPatchWrites ||
		InValue.Entities.Num() > MaxPatchWrites)
	{
		OutError = TEXT("Voxel section snapshot size or revision is invalid");
		return false;
	}
	FVoxelByteWriter Writer(MaxRepresentationBytes);
	WriteVector(Writer, InValue.Section);
	Writer.U64(InValue.Revision);
	Writer.U16(static_cast<uint16>(InValue.Blocks.Num()));
	for (const FVoxelSectionCellEdit& Edit : InValue.Blocks)
	{
		WriteCellEdit(Writer, Edit);
	}
	Writer.U16(static_cast<uint16>(InValue.Entities.Num()));
	for (const FVoxelEntityWrite& Edit : InValue.Entities)
	{
		WriteEntity(Writer, Edit);
	}
	if (!Writer.Finish(OutBytes))
	{
		OutError = TEXT("Voxel section snapshot encoding failed");
		return false;
	}
	OutError.Reset();
	return true;
}

bool FVoxelNetworkCodec::DecodeSectionSnapshot(
	TConstArrayView<uint8> InBytes,
	FVoxelNetworkSectionSnapshot& OutValue,
	FString& OutError)
{
	if (InBytes.Num() > MaxRepresentationBytes)
	{
		OutError = TEXT("Voxel section snapshot payload is too large");
		return false;
	}
	FVoxelByteReader Reader(InBytes);
	FVoxelNetworkSectionSnapshot Value;
	Value.Section = ReadVector(Reader);
	Value.Revision = Reader.U64();
	const uint16 BlockCount = Reader.U16();
	if (BlockCount > MaxPatchWrites)
	{
		OutError = TEXT("Voxel section snapshot has too many block writes");
		return false;
	}
	Value.Blocks.SetNum(BlockCount);
	for (FVoxelSectionCellEdit& Edit : Value.Blocks)
	{
		if (!ReadCellEdit(Reader, Edit))
		{
			OutError = TEXT("Voxel section snapshot contains an invalid block write");
			return false;
		}
	}
	const uint16 EntityCount = Reader.U16();
	if (EntityCount > MaxPatchWrites)
	{
		OutError = TEXT("Voxel section snapshot has too many entity writes");
		return false;
	}
	Value.Entities.SetNum(EntityCount);
	for (FVoxelEntityWrite& Edit : Value.Entities)
	{
		if (!ReadEntity(Reader, Edit))
		{
			OutError = TEXT("Voxel section snapshot contains an invalid entity write");
			return false;
		}
	}
	if (!Reader.End() || Value.Revision == 0)
	{
		OutError = TEXT("Voxel section snapshot payload is invalid");
		return false;
	}
	OutValue = MoveTemp(Value);
	OutError.Reset();
	return true;
}

bool FVoxelNetworkCodec::EncodePatchBatch(
	const FVoxelNetworkPatchBatch& InValue,
	TArray<uint8>& OutBytes,
	FString& OutError)
{
	if (!InValue.TransactionId.IsValid() ||
		InValue.Sections.IsEmpty() ||
		InValue.Sections.Num() > MaxPatchSections)
	{
		OutError = TEXT("Voxel patch batch identity or section count is invalid");
		return false;
	}
	FVoxelByteWriter Writer(MaxWireBytes - 64);
	Writer.Guid(InValue.TransactionId);
	Writer.U8(static_cast<uint8>(InValue.Sections.Num()));
	TSet<FIntVector> SeenSections;
	for (const FVoxelSectionPatch& Patch : InValue.Sections)
	{
		if (SeenSections.Contains(Patch.Section) ||
			Patch.ToRevision != Patch.FromRevision + 1 ||
			Patch.Edits.Num() > MaxPatchWrites ||
			Patch.Entities.Num() > MaxPatchWrites)
		{
			OutError = TEXT("Voxel patch batch contains an invalid section patch");
			return false;
		}
		SeenSections.Add(Patch.Section);
		WriteVector(Writer, Patch.Section);
		Writer.U64(Patch.FromRevision);
		Writer.U64(Patch.ToRevision);
		Writer.U16(static_cast<uint16>(Patch.Edits.Num()));
		for (const FVoxelSectionCellEdit& Edit : Patch.Edits)
		{
			WriteCellEdit(Writer, Edit);
		}
		Writer.U16(static_cast<uint16>(Patch.Entities.Num()));
		for (const FVoxelEntityWrite& Edit : Patch.Entities)
		{
			WriteEntity(Writer, Edit);
		}
	}
	if (!Writer.Finish(OutBytes))
	{
		OutError = TEXT("Voxel patch batch encoding failed");
		return false;
	}
	OutError.Reset();
	return true;
}

bool FVoxelNetworkCodec::DecodePatchBatch(
	TConstArrayView<uint8> InBytes,
	FVoxelNetworkPatchBatch& OutValue,
	FString& OutError)
{
	if (InBytes.Num() > MaxWireBytes - 64)
	{
		OutError = TEXT("Voxel patch batch payload is too large");
		return false;
	}
	FVoxelByteReader Reader(InBytes);
	FVoxelNetworkPatchBatch Value;
	Value.TransactionId = Reader.Guid();
	const uint8 SectionCount = Reader.U8();
	if (!Value.TransactionId.IsValid() || SectionCount == 0 || SectionCount > MaxPatchSections)
	{
		OutError = TEXT("Voxel patch batch header is invalid");
		return false;
	}
	TSet<FIntVector> SeenSections;
	for (uint8 SectionIndex = 0; SectionIndex < SectionCount; ++SectionIndex)
	{
		FVoxelSectionPatch Patch;
		Patch.Section = ReadVector(Reader);
		Patch.FromRevision = Reader.U64();
		Patch.ToRevision = Reader.U64();
		if (SeenSections.Contains(Patch.Section) || Patch.ToRevision != Patch.FromRevision + 1)
		{
			OutError = TEXT("Voxel patch batch section identity or revision is invalid");
			return false;
		}
		SeenSections.Add(Patch.Section);
		const uint16 BlockCount = Reader.U16();
		if (BlockCount > MaxPatchWrites)
		{
			OutError = TEXT("Voxel patch batch contains too many block writes");
			return false;
		}
		Patch.Edits.SetNum(BlockCount);
		for (FVoxelSectionCellEdit& Edit : Patch.Edits)
		{
			if (!ReadCellEdit(Reader, Edit))
			{
				OutError = TEXT("Voxel patch batch contains an invalid block write");
				return false;
			}
		}
		const uint16 EntityCount = Reader.U16();
		if (EntityCount > MaxPatchWrites)
		{
			OutError = TEXT("Voxel patch batch contains too many entity writes");
			return false;
		}
		Patch.Entities.SetNum(EntityCount);
		for (FVoxelEntityWrite& Edit : Patch.Entities)
		{
			if (!ReadEntity(Reader, Edit))
			{
				OutError = TEXT("Voxel patch batch contains an invalid entity write");
				return false;
			}
		}
		Value.Sections.Add(MoveTemp(Patch));
	}
	if (!Reader.End())
	{
		OutError = TEXT("Voxel patch batch payload has trailing or truncated data");
		return false;
	}
	OutValue = MoveTemp(Value);
	OutError.Reset();
	return true;
}

bool FVoxelNetworkCodec::EncodeRepresentationRequest(
	const FVoxelRepresentationRequest& InValue,
	TArray<uint8>& OutBytes)
{
	if (InValue.Type == EVoxelRepresentationWireType::None ||
		static_cast<uint8>(InValue.Type) > static_cast<uint8>(EVoxelRepresentationWireType::MacroTerrain) ||
		InValue.Serial == 0 ||
		!ValidateWireKey(InValue.Key))
	{
		return false;
	}
	TArray<uint8> GeometryBytes;
	FString GeometryError;
	if (!VoxelGeometryWire::Encode({InValue.Geometry, InValue.Serial, InValue.KnownRevision}, GeometryBytes, GeometryError)) return false;
	FVoxelByteWriter Writer(128);
	Writer.U8(static_cast<uint8>(InValue.Type));
	WriteVector(Writer, InValue.Key.Coordinate);
	Writer.U8(InValue.Key.Level);
	Writer.U64(InValue.KnownRevision);
	Writer.U64(InValue.Serial);
	Writer.Raw(GeometryBytes);
	return Writer.Finish(OutBytes);
}

bool FVoxelNetworkCodec::DecodeRepresentationRequest(
	TConstArrayView<uint8> InBytes,
	FVoxelRepresentationRequest& OutValue)
{
	FVoxelByteReader Reader(InBytes);
	FVoxelRepresentationRequest Value;
	Value.Type = static_cast<EVoxelRepresentationWireType>(Reader.U8());
	Value.Key.Coordinate = ReadVector(Reader);
	Value.Key.Level = Reader.U8();
	Value.KnownRevision = Reader.U64();
	Value.Serial = Reader.U64();
	TArray<uint8> GeometryBytes;
	FVoxelGeometryWireHeader Header;
	FString GeometryError;
	if (!Reader.Raw(VoxelGeometryWire::HeaderBytes, GeometryBytes) ||
		!VoxelGeometryWire::Decode(GeometryBytes, Header, GeometryError) ||
		Header.RequestId != Value.Serial || Header.ContentRevision != Value.KnownRevision ||
		Header.Geometry.Key.Coordinate != Value.Key.Coordinate || Header.Geometry.Key.Level != Value.Key.Level ||
		static_cast<uint8>(Header.Geometry.Key.Representation) != static_cast<uint8>(Value.Type) + 1) return false;
	Value.Geometry = Header.Geometry;
	if (!Reader.End() ||
		Value.Type == EVoxelRepresentationWireType::None ||
		static_cast<uint8>(Value.Type) > static_cast<uint8>(EVoxelRepresentationWireType::MacroTerrain) ||
		Value.Serial == 0 ||
		!ValidateWireKey(Value.Key))
	{
		return false;
	}
	OutValue = Value;
	return true;
}

bool FVoxelNetworkCodec::EncodeRepresentationReply(
	const FVoxelRepresentationReply& InValue,
	TArray<uint8>& OutBytes,
	FString& OutError)
{
	if (InValue.Type == EVoxelRepresentationWireType::None ||
		static_cast<uint8>(InValue.Type) > static_cast<uint8>(EVoxelRepresentationWireType::MacroTerrain) ||
		InValue.Serial == 0 ||
		!ValidateWireKey(InValue.Key) ||
		static_cast<uint8>(InValue.Kind) > static_cast<uint8>(EVoxelRepresentationReplyKind::Rejected) ||
		(InValue.Kind != EVoxelRepresentationReplyKind::Data && !InValue.DataView().IsEmpty()) ||
		InValue.DataView().Num() > MaxRepresentationBytes)
	{
		OutError = TEXT("Voxel representation reply is invalid");
		return false;
	}
	FVoxelByteWriter Writer(MaxRepresentationBytes + 64);
	Writer.Reserve(InValue.DataView().Num() + 35);
	Writer.U8(static_cast<uint8>(InValue.Type));
	WriteVector(Writer, InValue.Key.Coordinate);
	Writer.U8(InValue.Key.Level);
	Writer.U64(InValue.Serial);
	Writer.U64(InValue.Revision);
	Writer.U8(static_cast<uint8>(InValue.Kind));
	Writer.Blob(InValue.DataView(), MaxRepresentationBytes);
	if (!Writer.Finish(OutBytes))
	{
		OutError = TEXT("Voxel representation reply encoding failed");
		return false;
	}
	OutError.Reset();
	return true;
}

bool FVoxelNetworkCodec::DecodeRepresentationReply(
	TConstArrayView<uint8> InBytes,
	FVoxelRepresentationReply& OutValue,
	FString& OutError)
{
	FVoxelRepresentationReply Value;
	TConstArrayView<uint8> Data;
	if (!DecodeRepresentationReplyView(InBytes, Value, Data, OutError)) return false;
	Value.Data.Reserve(Data.Num());
	Value.Data.Append(Data.GetData(), Data.Num());
	OutValue = MoveTemp(Value);
	return true;
}

bool FVoxelNetworkCodec::DecodeRepresentationReplyView(TConstArrayView<uint8> InBytes,
	FVoxelRepresentationReply& OutValue, TConstArrayView<uint8>& OutData, FString& OutError)
{
	if (InBytes.Num() > MaxRepresentationBytes + 64)
	{
		OutError = TEXT("Voxel representation reply payload is too large");
		return false;
	}
	FVoxelByteReader Reader(InBytes);
	FVoxelRepresentationReply Value;
	Value.Type = static_cast<EVoxelRepresentationWireType>(Reader.U8());
	Value.Key.Coordinate = ReadVector(Reader);
	Value.Key.Level = Reader.U8();
	Value.Serial = Reader.U64();
	Value.Revision = Reader.U64();
	Value.Kind = static_cast<EVoxelRepresentationReplyKind>(Reader.U8());
	const auto Data = Reader.BlobView(MaxRepresentationBytes);
	if (!Reader.End() ||
		Value.Type == EVoxelRepresentationWireType::None ||
		static_cast<uint8>(Value.Type) > static_cast<uint8>(EVoxelRepresentationWireType::MacroTerrain) ||
		Value.Serial == 0 ||
		!ValidateWireKey(Value.Key) ||
		static_cast<uint8>(Value.Kind) > static_cast<uint8>(EVoxelRepresentationReplyKind::Rejected) ||
		(Value.Kind != EVoxelRepresentationReplyKind::Data && !Data.IsEmpty()))
	{
		OutError = TEXT("Voxel representation reply payload is invalid");
		return false;
	}
	OutValue = MoveTemp(Value);
	OutData = Data;
	OutError.Reset();
	return true;
}

bool FVoxelNetworkCodec::EncodeRepresentationInvalidate(
	const FVoxelRepresentationInvalidate& InValue, TArray<uint8>& OutBytes, FString& OutError)
{
	if (InValue.Entries.IsEmpty() || InValue.Entries.Num() > 256)
	{
		OutError = TEXT("Voxel representation invalidation is invalid");
		return false;
	}
	FVoxelByteWriter Writer(2 + 256 * VoxelGeometryWire::HeaderBytes);
	Writer.Reserve(2 + InValue.Entries.Num() * VoxelGeometryWire::HeaderBytes);
	Writer.U16(InValue.Entries.Num());
	for (const auto& Entry : InValue.Entries)
	{
		TArray<uint8> Header;
		if (Entry.Revision == 0 || Entry.Geometry.Key.Representation == EVoxelTerrainRepresentation::Fine)
		{ OutError = TEXT("Voxel representation invalidation identity is invalid"); return false; }
		if (!VoxelGeometryWire::Encode({Entry.Geometry, Entry.Revision, Entry.Revision}, Header, OutError)) return false;
		Writer.Raw(Header);
	}
	if (!Writer.Finish(OutBytes))
	{
		OutError = TEXT("Voxel representation invalidation encoding failed");
		return false;
	}
	OutError.Reset();
	return true;
}

bool FVoxelNetworkCodec::DecodeRepresentationInvalidate(
	TConstArrayView<uint8> InBytes, FVoxelRepresentationInvalidate& OutValue, FString& OutError)
{
	FVoxelByteReader Reader(InBytes);
	const uint16 Count = Reader.U16();
	if (Count == 0 || Count > 256 || InBytes.Num() != 2 + Count * VoxelGeometryWire::HeaderBytes)
	{
		OutError = TEXT("Voxel representation invalidation header is invalid");
		return false;
	}
	FVoxelRepresentationInvalidate Value;
	Value.Entries.Reserve(Count);
	for (uint16 Index = 0; Index < Count; ++Index)
	{
		FVoxelGeometryWireHeader Header;
		if (!VoxelGeometryWire::Decode(Reader.View(VoxelGeometryWire::HeaderBytes), Header, OutError)) return false;
		if (Header.ContentRevision == 0 || Header.RequestId != Header.ContentRevision ||
			Header.Geometry.Key.Representation == EVoxelTerrainRepresentation::Fine)
		{ OutError = TEXT("Voxel representation invalidation identity is invalid"); return false; }
		Value.Entries.Add({Header.Geometry, Header.ContentRevision});
	}
	if (!Reader.End())
	{
		OutError = TEXT("Voxel representation invalidation payload is malformed");
		return false;
	}
	OutValue = MoveTemp(Value);
	OutError.Reset();
	return true;
}

bool FVoxelNetworkCodec::EncodeIntent(
	const FVoxelEditIntent& InIntent,
	TArray<uint8>& OutBytes)
{
	if (InIntent.RequestId == 0 || !InIntent.InventoryStoreId.IsValid() ||
		InIntent.PlacementQuarterTurns > 3 ||
		static_cast<uint8>(InIntent.Action) > static_cast<uint8>(EVoxelEditAction::Use) ||
		InIntent.Origin.ContainsNaN() ||
		InIntent.Direction.ContainsNaN())
	{
		return false;
	}
	FVoxelByteWriter Writer(1024);
	Writer.U64(InIntent.RequestId);
	Writer.Guid(InIntent.InventoryStoreId);
	Writer.U8(static_cast<uint8>(InIntent.Action));
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		if (FMath::Abs(InIntent.Origin[Axis]) > 2e9)
		{
			return false;
		}
		Writer.I32(FMath::RoundToInt(InIntent.Origin[Axis]));
	}
	const FVector Direction = InIntent.Direction.GetSafeNormal();
	if (Direction.IsNearlyZero())
	{
		return false;
	}
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		Writer.I32(FMath::RoundToInt(Direction[Axis] * 1000000));
	}
	WriteVector(Writer, InIntent.ExpectedTarget);
	Writer.String(InIntent.ExpectedItemID.IsValid() ? InIntent.ExpectedItemID.ToString() : FString(), 256);
	Writer.Guid(InIntent.ExpectedInventoryId);
	Writer.String(InIntent.InventorySlot.Group.IsValid() ? InIntent.InventorySlot.Group.ToString() : FString(), 256);
	Writer.I32(InIntent.InventorySlot.Index);
	Writer.U64(InIntent.ExpectedInventoryRevision);
	Writer.U32(InIntent.ExpectedInventoryGeneration);
	Writer.U64(InIntent.ExpectedRevision);
	Writer.U8(InIntent.PlacementQuarterTurns);
	return Writer.Finish(OutBytes);
}

bool FVoxelNetworkCodec::DecodeIntent(
	TConstArrayView<uint8> InBytes,
	FVoxelEditIntent& OutIntent)
{
	if (InBytes.Num() > 1024)
	{
		return false;
	}
	FVoxelByteReader Reader(InBytes);
	FVoxelEditIntent Value;
	Value.RequestId = Reader.U64();
	Value.InventoryStoreId = Reader.Guid();
	const uint8 Action = Reader.U8();
	if (Value.RequestId == 0 || !Value.InventoryStoreId.IsValid() || Action > static_cast<uint8>(EVoxelEditAction::Use))
	{
		return false;
	}
	Value.Action = static_cast<EVoxelEditAction>(Action);
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		Value.Origin[Axis] = Reader.I32();
	}
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		Value.Direction[Axis] = static_cast<double>(Reader.I32()) / 1000000.0;
	}
	Value.ExpectedTarget = ReadVector(Reader);
	const FString ItemId = Reader.String(256);
	Value.ExpectedItemID = ItemId.IsEmpty() ? FPrimaryAssetId() : FPrimaryAssetId(ItemId);
	Value.ExpectedInventoryId = Reader.Guid();
	const FString GroupName = Reader.String(256);
	Value.InventorySlot.Group = GroupName.IsEmpty() ? FGameplayTag() : FGameplayTag::RequestGameplayTag(FName(*GroupName), false);
	Value.InventorySlot.Index = Reader.I32();
	Value.ExpectedInventoryRevision = Reader.U64();
	Value.ExpectedInventoryGeneration = Reader.U32();
	Value.ExpectedRevision = Reader.U64();
	Value.PlacementQuarterTurns = Reader.U8();
	const double DirectionLength = Value.Direction.SizeSquared();
	if (!Reader.End() ||
		Value.PlacementQuarterTurns > 3 ||
		DirectionLength < 0.99 ||
		DirectionLength > 1.01 ||
		(!ItemId.IsEmpty() && !Value.ExpectedItemID.IsValid()) ||
		Action == static_cast<uint8>(EVoxelEditAction::None) ||
		Value.InventorySlot.Index < INDEX_NONE ||
		Value.InventorySlot.Index >= 4096 ||
		(!GroupName.IsEmpty() && !Value.InventorySlot.Group.IsValid()) ||
		Value.ExpectedInventoryGeneration == 0)
	{
		return false;
	}
	Value.Direction.Normalize();
	OutIntent = MoveTemp(Value);
	return true;
}

bool FVoxelNetworkCodec::EncodeReply(
	const FVoxelEditReply& InReply,
	TArray<uint8>& OutBytes)
{
	FVoxelByteWriter Writer(1024);
	Writer.U64(InReply.RequestId);
	Writer.U8(static_cast<uint8>(InReply.Code));
	Writer.String(InReply.Reason, 768);
	return Writer.Finish(OutBytes);
}

bool FVoxelNetworkCodec::DecodeReply(
	TConstArrayView<uint8> InBytes,
	FVoxelEditReply& OutReply)
{
	FVoxelByteReader Reader(InBytes);
	FVoxelEditReply Value;
	Value.RequestId = Reader.U64();
	const uint8 Code = Reader.U8();
	if (Code > static_cast<uint8>(EVoxelEditCode::ConfirmationPending))
	{
		return false;
	}
	Value.Code = static_cast<EVoxelEditCode>(Code);
	Value.Reason = Reader.String(768);
	if (!Reader.End())
	{
		return false;
	}
	OutReply = MoveTemp(Value);
	return true;
}

void FVoxelNetworkCodec::WriteCellEdit(
	FVoxelByteWriter& InWriter,
	const FVoxelSectionCellEdit& InEdit)
{
	InWriter.U16(static_cast<uint16>(InEdit.CellIndex));
	InWriter.U8(InEdit.bNatural ? 1 : 0);
	InWriter.U32(InEdit.State.Pack());
}

bool FVoxelNetworkCodec::ReadCellEdit(
	FVoxelByteReader& InReader,
	FVoxelSectionCellEdit& OutEdit)
{
	FVoxelSectionCellEdit Value;
	Value.CellIndex = InReader.U16();
	const uint8 Natural = InReader.U8();
	Value.State = FVoxelBlockState::Unpack(InReader.U32());
	if (!InReader.IsValid() || Value.CellIndex >= VoxelBlock::Volume || Natural > 1)
	{
		return false;
	}
	Value.bNatural = Natural != 0;
	OutEdit = Value;
	return true;
}

void FVoxelNetworkCodec::WriteEntity(
	FVoxelByteWriter& InWriter,
	const FVoxelEntityWrite& InEdit)
{
	InWriter.U16(static_cast<uint16>(InEdit.CellIndex));
	InWriter.U8(InEdit.bRemove ? 1 : 0);
	InWriter.U16(InEdit.Value.Kind);
	const bool bInventory = !InEdit.bRemove && FInventoryVoxelEntityCodec::IsInventory(InEdit.Value);
	InWriter.U16(bInventory ? FInventoryVoxelEntityCodec::Schema : InEdit.Value.Schema);
	InWriter.Blob(bInventory ? FInventoryVoxelEntityCodec::GetPublicPayload() : MakeArrayView(InEdit.Value.Payload), MaxEntityPayloadBytes);
}

bool FVoxelNetworkCodec::ReadEntity(
	FVoxelByteReader& InReader,
	FVoxelEntityWrite& OutEdit)
{
	FVoxelEntityWrite Value;
	Value.CellIndex = InReader.U16();
	const uint8 Remove = InReader.U8();
	Value.Value.Kind = InReader.U16();
	Value.Value.Schema = InReader.U16();
	Value.Value.Payload = InReader.Blob(MaxEntityPayloadBytes);
	if (!InReader.IsValid() || Value.CellIndex >= VoxelBlock::Volume || Remove > 1)
	{
		return false;
	}
	Value.bRemove = Remove != 0;
	if(!Value.bRemove && FInventoryVoxelEntityCodec::IsInventory(Value.Value) && !FInventoryVoxelEntityCodec::IsPublic(Value.Value)) return false;
	OutEdit = MoveTemp(Value);
	return true;
}

bool FVoxelNetworkCodec::ValidateWireKey(const FVoxelRepresentationWireKey& InKey)
{
	return InKey.Level <= 31;
}

bool FVoxelNetworkSettings::Validate(FString& OutError) const
{
	if (BytesPerSecond < 1024 ||
		BurstBytes < 4096 ||
		MaxQueuedBytes < BurstBytes ||
		MaxRepresentationRequests < 1)
	{
		OutError = TEXT("Voxel network settings contain invalid limits");
		return false;
	}
	OutError.Reset();
	return true;
}
