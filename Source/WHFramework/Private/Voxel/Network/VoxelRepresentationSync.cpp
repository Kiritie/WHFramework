#include "Voxel/Network/VoxelRepresentationSync.h"
#include "Voxel/Network/VoxelGeometryWire.h"

#include "Voxel/Network/VoxelNetworkCodec.h"
#include "Voxel/Serialization/VoxelBinaryCodec.h"
#include "Voxel/Rendering/VoxelWaterView.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"
#include "Voxel/Rendering/VoxelViewLod.h"
#include "Voxel/VoxelModule.h"

namespace
{
	constexpr uint32 RepresentationMagic = 0x34505256;
	constexpr uint16 RepresentationVersion = 6;
	constexpr int32 MaxRepresentationRawBytes = 16 * 1024 * 1024;
	constexpr int32 MaxCells = 1 << 20;
	constexpr uint32 MaxDistantCells = 16384;

	bool ValidateRepresentationRequest(const FVoxelRepresentationRequest& InRequest, FString& OutError)
	{
		if (!InRequest.Geometry.Validate(OutError))
		{
			return false;
		}
		EVoxelTerrainRepresentation Representation = EVoxelTerrainRepresentation::None;
		switch (InRequest.Type)
		{
		case EVoxelRepresentationWireType::VoxelProxy:
			Representation = EVoxelTerrainRepresentation::VoxelProxy;
			break;
		case EVoxelRepresentationWireType::SurfaceProxy:
			Representation = EVoxelTerrainRepresentation::Surface;
			break;
		case EVoxelRepresentationWireType::MacroTerrain:
			Representation = EVoxelTerrainRepresentation::Macro;
			break;
		default:
			break;
		}
		if (Representation == EVoxelTerrainRepresentation::None || InRequest.Serial == 0 ||
			InRequest.Geometry.Key != FVoxelTerrainSpatialKey{Representation, InRequest.Key.Coordinate, InRequest.Key.Level} ||
			(Representation == EVoxelTerrainRepresentation::VoxelProxy && InRequest.Key.Level == 0))
		{
			OutError = TEXT("Representation request disagrees with its actual geometry identity");
			return false;
		}
		return true;
	}

	bool ResolveBuildBounds(const FVoxelTerrainGeometry& InGeometry,
		const FVoxelGenerationSettings& InSettings, FVoxelGenerationBounds& OutBounds, FString& OutError)
	{
		FIntVector Min, Max;
		const int32 Step = InGeometry.SampleStepCells;
		const int32 Margin = FMath::Max(Step, InSettings.Ecology.Tree.bEnabled ? InSettings.Ecology.Tree.CrownRadius : 0);
		const FIntVector Padding(Margin, Margin, InGeometry.Key.IsColumn() ? 0 : Step);
		if (!VoxelStreamingGeometry::TrySamplingBounds(InGeometry, Padding, Padding, Min, Max, OutError))
		{
			return false;
		}
		OutBounds = {Min, Max};
		return true;
	}

	bool WriteGeometry(FVoxelByteWriter& Writer, const FVoxelTerrainGeometry& Geometry,
		uint64 RequestId, uint64 Revision, FString& Error)
	{
		TArray<uint8> Bytes;
		if (!VoxelGeometryWire::Encode({Geometry, RequestId, Revision}, Bytes, Error)) return false;
		Writer.Raw(Bytes);
		return Writer.IsValid();
	}

	void ReadGeometry(FVoxelByteReader& Reader, FVoxelTerrainGeometry& Geometry, uint64& RequestId, uint64 Revision)
	{
		FVoxelGeometryWireHeader Header;
		FString Error;
		if (!VoxelGeometryWire::Decode(Reader.View(VoxelGeometryWire::HeaderBytes), Header, Error) || Header.ContentRevision != Revision)
		{ Reader.Reject(); return; }
		Geometry = Header.Geometry;
		RequestId = Header.RequestId;
	}

	void WriteViewKey(FVoxelByteWriter& InWriter, const FVoxelViewKey& InKey)
	{
		InWriter.I32(InKey.Coordinate.X);
		InWriter.I32(InKey.Coordinate.Y);
		InWriter.I32(InKey.Coordinate.Z);
		InWriter.U8(InKey.Level);
	}

	FVoxelViewKey ReadViewKey(FVoxelByteReader& InReader)
	{
		FVoxelViewKey Key;
		Key.Coordinate.X = InReader.I32();
		Key.Coordinate.Y = InReader.I32();
		Key.Coordinate.Z = InReader.I32();
		Key.Level = InReader.U8();
		return Key;
	}

	void WriteTileKey(FVoxelByteWriter& InWriter, const FIntPoint& InCoordinate, const uint8 InLevel)
	{
		InWriter.I32(InCoordinate.X);
		InWriter.I32(InCoordinate.Y);
		InWriter.U8(InLevel);
	}

	void WriteDistantCells(FVoxelByteWriter& InWriter, TConstArrayView<FVoxelDistantCell> InCells)
	{
		InWriter.U32(InCells.Num());
		for (const FVoxelDistantCell& Cell : InCells)
		{
			InWriter.I32(Cell.Min.X); InWriter.I32(Cell.Min.Y); InWriter.I32(Cell.Min.Z);
			InWriter.I32(Cell.Max.X); InWriter.I32(Cell.Max.Y); InWriter.I32(Cell.Max.Z);
			InWriter.U32(Cell.State.Pack());
			InWriter.U64(Cell.SourceTreeId.High);
			InWriter.U64(Cell.SourceTreeId.Low);
		}
	}

	void ReadDistantCells(FVoxelByteReader& InReader, TArray<FVoxelDistantCell>& OutCells)
	{
		const uint32 Count = InReader.U32();
		if (Count > MaxDistantCells || !InReader.IsValid() || uint64(Count) * 44 > uint64(InReader.Remaining())) { InReader.Reject(); return; }
		OutCells.Reserve(Count);
		for (uint32 Index = 0; Index < Count && InReader.IsValid(); ++Index)
		{
			FVoxelDistantCell Cell;
			Cell.Min.X = InReader.I32(); Cell.Min.Y = InReader.I32(); Cell.Min.Z = InReader.I32();
			Cell.Max.X = InReader.I32(); Cell.Max.Y = InReader.I32(); Cell.Max.Z = InReader.I32();
			Cell.State = FVoxelBlockState::Unpack(InReader.U32());
			Cell.SourceTreeId.High = InReader.U64();
			Cell.SourceTreeId.Low = InReader.U64();
			if (Cell.State.IsAir() || Cell.Min.X >= Cell.Max.X ||
				Cell.Min.Y >= Cell.Max.Y || Cell.Min.Z >= Cell.Max.Z) InReader.Reject();
			if (InReader.IsValid()) OutCells.Add(Cell);
		}
	}

	bool ReserveRawWriter(const uint64 InBytes, FVoxelByteWriter& InWriter,
		TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
		const FVoxelGenerationConsumerKey& InConsumer,
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& OutLease, FString& OutError)
	{
		if (!InConsumer.IsValid() || InBytes == 0 || InBytes > MaxRepresentationRawBytes)
		{
			OutError = TEXT("Oversized: invalid representation codec input");
			return false;
		}
		const uint64 Bytes = VoxelBoundedAllocation::ArrayBytes<uint8>(int32(InBytes)) +
			VoxelBoundedAllocation::ArrayBytes<uint8>(VoxelGeometryWire::HeaderBytes);
		if (!InResources->Acquire(InConsumer, EVoxelTerrainResourcePool::Working, Bytes, OutLease))
		{
			OutError = TEXT("CapacityBlocked: representation raw writer");
			return false;
		}
		return InWriter.Reserve(int32(InBytes));
	}

	bool FinishCompressed(FVoxelByteWriter& InWriter, FVoxelBinaryBuffer& OutBytes, FString& OutError,
		TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
		const FVoxelGenerationConsumerKey& InConsumer)
	{
		TArray<uint8> Raw;
		bool bCapacityBlocked = false;
		if (!InWriter.Finish(Raw) || !VoxelBinary::CompressGranted(Raw, OutBytes, InResources,
			InConsumer, MaxRepresentationRawBytes, FVoxelNetworkCodec::MaxRepresentationBytes, bCapacityBlocked))
		{
			OutError = bCapacityBlocked ? TEXT("CapacityBlocked: representation native compression") :
				TEXT("Voxel representation encoding failed");
			return false;
		}
		OutError.Reset();
		return true;
	}

	bool OpenCompressed(TConstArrayView<uint8> InBytes, FVoxelBinaryBuffer& OutRaw, FString& OutError,
		TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
		const FVoxelGenerationConsumerKey& InConsumer)
	{
		if (!InConsumer.IsValid() || InBytes.Num() > FVoxelNetworkCodec::MaxRepresentationBytes)
		{
			OutError = TEXT("Invalid representation compression input");
			return false;
		}
		FVoxelByteReader Header(InBytes);
		const uint32 Magic = Header.U32();
		const uint32 RawBytes = Header.U32();
		Header.U32();
		const auto Compressed = Header.BlobView(MaxRepresentationRawBytes + 65536);
		if (!Header.End() || Magic != 0x315a5856 || !RawBytes ||
			RawBytes > MaxRepresentationRawBytes || Compressed.IsEmpty())
		{
			OutError = TEXT("Voxel representation compression envelope is invalid");
			return false;
		}
		FVoxelBinaryBuffer Candidate;
		if (!InResources->Acquire(InConsumer, EVoxelTerrainResourcePool::Working,
			VoxelBoundedAllocation::ArrayBytes<uint8>(int32(RawBytes)), Candidate.ResourceLease))
		{
			OutError = TEXT("CapacityBlocked: representation raw decode input");
			return false;
		}
		bool bCapacityBlocked = false;
		if (!VoxelBinary::DecompressGranted(InBytes, Candidate.Bytes, InResources, InConsumer,
			MaxRepresentationRawBytes, bCapacityBlocked))
		{
			OutError = bCapacityBlocked ? TEXT("CapacityBlocked: representation native decompression") :
				TEXT("Voxel representation compression envelope is invalid");
			return false;
		}
		check(Candidate.Bytes.GetAllocatedSize() <= Candidate.ResourceLease->Bytes());
		OutRaw = MoveTemp(Candidate);
		return true;
	}
}

bool FVoxelRepresentationSync::PrepareServerBuild(
	UVoxelModule& InModule,
	const FVoxelRepresentationRequest& InRequest,
	FVoxelRepresentationBuildInput& OutInput,
	FString& OutError, const FVoxelGenerationConsumerKey* InConsumer)
{
	const FVoxelWorldRuntime* Runtime = InModule.GetRuntime();
	FVoxelRepresentationBuildInput Input;
	Input.Request = InRequest;
	Input.Settings = InModule.GetManifest().Settings;
	Input.Lod = InModule.GetViewSettings().Lod;
	Input.Config = InModule.GetGenerationConfig();
	Input.Cache = InModule.GetGenerationCache();
	Input.Generator = InModule.GetGenerator();
	Input.Registry = InModule.GetRegistry();
	if (!Input.Config || !Input.Cache || !Input.Generator || !Input.Registry || !Runtime)
	{
		OutError = TEXT("Voxel representation dependencies are unavailable");
		return false;
	}

	if (!ValidateRepresentationRequest(InRequest, OutError)) return false;
	FVoxelGenerationBounds Bounds;
	if (!ResolveBuildBounds(InRequest.Geometry, Input.Settings, Bounds, OutError)) return false;
	if (!InModule.EnsureEnvironmentBounds(Bounds, EVoxelWorkClass::Exploration, 0, 0.0, 0.0).bReady ||
		!Input.Generator->EnsureCoarseOverlayBoundsReady(Bounds, EVoxelWorkClass::Exploration, 0).bReady)
	{ OutError = TEXT("DependencyNotReady: representation environment"); return false; }
	if (!InModule.CaptureFineOverlays(Bounds, Input.OverlayCapture, OutError, InConsumer))
	{
		return false;
	}
	Input.Revision = Runtime->GetChangeHierarchy().CaptureVersionSnapshot().GetGeometryRevision(InRequest.Geometry);
	OutInput = MoveTemp(Input);
	OutError.Reset();
	return true;
}

bool FVoxelRepresentationSync::EncodeVoxelProxy(
	const FVoxelVoxelProxyData& InData,
	FVoxelBinaryBuffer& OutBytes,
	FString& OutError,
	TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
	const FVoxelGenerationConsumerKey& InConsumer)
{
	if (!InData.Geometry.Validate(OutError) ||
		InData.Geometry.Key != FVoxelTerrainSpatialKey{EVoxelTerrainRepresentation::VoxelProxy, InData.Key.Coordinate, InData.Key.Level} ||
		InData.Key.Level == 0 || InData.GridSide <= 0 || InData.GridSide > 128 || InData.LocalMinCells != FIntVector::ZeroValue || InData.GridSide !=
			InData.Key.GetSide() / InData.Geometry.SampleStepCells ||
		int64(InData.Cells.Num()) != int64(InData.GridSide) * InData.GridSide * InData.GridSide)
	{
		OutError =
			TEXT(
				"Voxel proxy representation is invalid");

		return false;
	}

	for (int32 Face = 0;
		Face < 6;
		++Face)
	{
		if (InData.Known[
				Face] &&
			InData.Halo[
				Face].Num() !=
				(InData.GridSide * InData.GridSide))
		{
			OutError =
				TEXT(
					"Voxel proxy representation halo is invalid");

			return false;
		}
	}

	uint64 RawBytes = 105 + uint64(InData.Cells.Num()) * 4;
	for (int32 Face = 0; Face < 6; ++Face)
	{
		if (InData.Known[Face]) RawBytes += uint64(InData.Halo[Face].Num()) * 4;
	}
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> RawLease;
	FVoxelByteWriter Writer{int32(RawBytes)};
	if (!ReserveRawWriter(RawBytes, Writer, InResources, InConsumer, RawLease, OutError)) return false;

	Writer.U32(
		RepresentationMagic);

	Writer.U16(
		RepresentationVersion);

	Writer.U8(
		1);

	WriteViewKey(
		Writer,
		InData.Key);

	Writer.U64(InData.Revision);
	if (!WriteGeometry(Writer, InData.Geometry, InData.RequestId, InData.Revision, OutError)) return false;

	Writer.U16(
		InData.GridSide);

	Writer.U8(
		InData.
			bHasVisibleSurfaceEvidence
				? 1
				: 0);

	Writer.U32(
		InData.Cells.Num());

	for (const FVoxelBlockState State :
		InData.Cells)
	{
		Writer.U32(
			State.Pack());
	}

	for (int32 Face = 0;
		Face < 6;
		++Face)
	{
		Writer.U8(
			InData.Known[
				Face]
				? 1
				: 0);

		if (!InData.Known[
				Face])
		{
			continue;
		}

		for (const FVoxelBlockState State :
			InData.Halo[
				Face])
		{
			Writer.U32(
				State.Pack());
		}
	}

	return FinishCompressed(Writer, OutBytes, OutError, InResources, InConsumer);
}

bool FVoxelRepresentationSync::DecodeVoxelProxy(
	TConstArrayView<uint8> InBytes,
	FVoxelVoxelProxyData& OutData,
	FString& OutError,
	TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
	const FVoxelGenerationConsumerKey& InConsumer,
	const FVoxelGeometryWireHeader* InExpected)
{
	FVoxelBinaryBuffer Raw;

	if (!OpenCompressed(InBytes, Raw, OutError, InResources, InConsumer))
	{
		return false;
	}

	FVoxelByteReader Reader(Raw.Bytes);

	if (Reader.U32() !=
			RepresentationMagic ||
		Reader.U16() !=
			RepresentationVersion ||
		Reader.U8() !=
			1)
	{
		OutError =
			TEXT(
				"Voxel proxy representation header is invalid");

		return false;
	}

	FVoxelVoxelProxyData Data;

	Data.Key =
		ReadViewKey(
			Reader);

	Data.Revision = Reader.U64();
	ReadGeometry(Reader, Data.Geometry, Data.RequestId, Data.Revision);
	if (!Reader.IsValid() || (InExpected && !VoxelGeometryWire::ValidateExpected(
		{Data.Geometry, Data.RequestId, Data.Revision}, *InExpected, OutError))) return false;
	if (Data.Geometry.Key != FVoxelTerrainSpatialKey{EVoxelTerrainRepresentation::VoxelProxy, Data.Key.Coordinate, Data.Key.Level})
	{ OutError = TEXT("Representation payload key disagrees with geometry header"); return false; }

	Data.GridSide =
		Reader.U16();

	const uint8 VisibleSurface =
		Reader.U8();

	if (VisibleSurface > 1)
	{
		Reader.Reject();
	}

	Data.bHasVisibleSurfaceEvidence =
		VisibleSurface !=
			0;

	const uint32 Count =
		Reader.U32();

	if (!Reader.IsValid() || Data.Key.Level == 0 || Data.Key.Level > 20 ||
		Data.GridSide <= 0 || Data.GridSide > 128 || Data.Geometry.SampleStepCells <= 0 ||
		Data.GridSide != Data.Key.GetSide() / Data.Geometry.SampleStepCells ||
		Count != (Data.GridSide * Data.GridSide * Data.GridSide) || uint64(Count) * 4 + 6 > uint64(Reader.Remaining()))
	{
		Reader.Reject();
		OutError = TEXT("Invalid proxy sampling resolution");
		return false;
	}

	Data.Cells.Reserve(
		Count);

	for (uint32 Index = 0;
		Index < Count &&
			Reader.IsValid();
		++Index)
	{
		Data.Cells.Add(
			FVoxelBlockState::
				Unpack(
					Reader.U32()));
	}

	for (int32 Face = 0;
		Face < 6 &&
			Reader.IsValid();
		++Face)
	{
		const uint8 Known =
			Reader.U8();

		if (Known > 1)
		{
			Reader.Reject();
			break;
		}

		Data.Known[
			Face] =
				Known !=
				0;

		if (!Data.Known[
				Face])
		{
			continue;
		}

		if (uint64(Data.GridSide) * Data.GridSide * 4 > uint64(Reader.Remaining()))
		{
			Reader.Reject();
			break;
		}
		Data.Halo[
			Face].
			Reserve(
				(Data.GridSide * Data.GridSide));

		for (int32 Index = 0;
			Index < (Data.GridSide * Data.GridSide) &&
				Reader.IsValid();
			++Index)
		{
			Data.Halo[
				Face].
				Add(
					FVoxelBlockState::
						Unpack(
							Reader.U32()));
		}
	}

	if (!Reader.End())
	{
		OutError =
			TEXT(
				"Voxel proxy representation payload is invalid");

		return false;
	}

	OutData =
		MoveTemp(
			Data);

	OutError.Reset();
	return true;
}

bool FVoxelRepresentationSync::EncodeSurface(
	const FVoxelSurfaceTileData& InData,
	FVoxelBinaryBuffer& OutBytes,
	FString& OutError,
	TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
	const FVoxelGenerationConsumerKey& InConsumer)
{
	if (!InData.Geometry.Validate(OutError) ||
		InData.Geometry.Key != FVoxelTerrainSpatialKey{EVoxelTerrainRepresentation::Surface, FIntVector(InData.Key.Coordinate.X, InData.Key.Coordinate.Y, 0), InData.Key.Level} ||
		InData.Side <= 1 || InData.Side > 257 || InData.Step != InData.Geometry.SampleStepCells || InData.LocalMinCells != FIntPoint::ZeroValue ||
		int64(InData.Side - 1) * InData.Step != InData.Geometry.Key.SideCells())
	{ OutError = TEXT("Surface representation disagrees with its actual geometry"); return false; }
	const int32 Count = InData.Side * InData.Side;
	if (Count <= 0 || Count > MaxCells || InData.GroundZ.Num() != Count || InData.WaterZ.Num() != Count ||
		InData.SurfaceMaterial.Num() != Count || InData.Biome.Num() != Count || InData.Flags.Num() != Count ||
		InData.DistantCells.Num() > MaxDistantCells)
	{
		OutError = TEXT("Voxel surface representation is invalid");
		return false;
	}
	const uint64 RawBytes = 102 + uint64(Count) * 13 + uint64(InData.DistantCells.Num()) * 44;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> RawLease;
	FVoxelByteWriter Writer{int32(RawBytes)};
	if (!ReserveRawWriter(RawBytes, Writer, InResources, InConsumer, RawLease, OutError)) return false;
	Writer.U32(RepresentationMagic); Writer.U16(RepresentationVersion); Writer.U8(2);
	WriteTileKey(Writer, InData.Key.Coordinate, InData.Key.Level);
	Writer.U64(InData.Revision);
	if (!WriteGeometry(Writer, InData.Geometry, InData.RequestId, InData.Revision, OutError)) return false;
	Writer.U16(InData.Side); Writer.I32(InData.Step); Writer.U32(Count);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		Writer.I32(InData.GroundZ[Index]); Writer.I32(InData.WaterZ[Index]);
		Writer.U16(InData.SurfaceMaterial[Index]); Writer.U16(InData.Biome[Index]); Writer.U8(InData.Flags[Index]);
	}
	WriteDistantCells(Writer, InData.DistantCells);
	return FinishCompressed(Writer, OutBytes, OutError, InResources, InConsumer);
}

bool FVoxelRepresentationSync::DecodeSurface(
	TConstArrayView<uint8> InBytes,
	FVoxelSurfaceTileData& OutData,
	FString& OutError,
	TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
	const FVoxelGenerationConsumerKey& InConsumer,
	const FVoxelGeometryWireHeader* InExpected)
{
	FVoxelBinaryBuffer Raw;
	if (!OpenCompressed(InBytes, Raw, OutError, InResources, InConsumer)) return false;
	FVoxelByteReader Reader(Raw.Bytes);
	if (Reader.U32() != RepresentationMagic || Reader.U16() != RepresentationVersion || Reader.U8() != 2)
	{
		OutError = TEXT("Voxel surface representation header is invalid"); return false;
	}
	FVoxelSurfaceTileData Data;
	Data.Key.Coordinate.X = Reader.I32(); Data.Key.Coordinate.Y = Reader.I32(); Data.Key.Level = Reader.U8();
	Data.Revision = Reader.U64();
	ReadGeometry(Reader, Data.Geometry, Data.RequestId, Data.Revision);
	if (!Reader.IsValid() || (InExpected && !VoxelGeometryWire::ValidateExpected(
		{Data.Geometry, Data.RequestId, Data.Revision}, *InExpected, OutError))) return false;
	if (Data.Geometry.Key != FVoxelTerrainSpatialKey{EVoxelTerrainRepresentation::Surface, FIntVector(Data.Key.Coordinate.X, Data.Key.Coordinate.Y, 0), Data.Key.Level})
	{ OutError = TEXT("Representation payload key disagrees with geometry header"); return false; }
	Data.Side = Reader.U16(); Data.Step = Reader.I32();
	const uint32 Count = Reader.U32();
	if (Data.Step != Data.Geometry.SampleStepCells || Data.Side <= 1 || Data.Side > 257 || int64(Data.Side - 1) * Data.Step != Data.Geometry.Key.SideCells() || Count != static_cast<uint32>(Data.Side * Data.Side) || Count > MaxCells || !Reader.IsValid() || uint64(Count) * 13 > uint64(Reader.Remaining()))
	{ OutError = TEXT("Invalid surface sampling resolution"); return false; }
	Data.GroundZ.Reserve(Count); Data.WaterZ.Reserve(Count); Data.SurfaceMaterial.Reserve(Count);
	Data.Biome.Reserve(Count); Data.Flags.Reserve(Count);
	for (uint32 Index = 0; Index < Count && Reader.IsValid(); ++Index)
	{
		Data.GroundZ.Add(Reader.I32()); Data.WaterZ.Add(Reader.I32());
		Data.SurfaceMaterial.Add(Reader.U16()); Data.Biome.Add(Reader.U16()); Data.Flags.Add(Reader.U8());
	}
	ReadDistantCells(Reader, Data.DistantCells);
	if (!Reader.End()) { OutError = TEXT("Voxel surface representation payload is invalid"); return false; }
	OutData = MoveTemp(Data); OutError.Reset(); return true;
}

bool FVoxelRepresentationSync::EncodeMacro(
	const FVoxelMacroTileData& InData,
	FVoxelBinaryBuffer& OutBytes,
	FString& OutError,
	TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
	const FVoxelGenerationConsumerKey& InConsumer)
{
	if (!InData.Geometry.Validate(OutError) ||
		InData.Geometry.Key != FVoxelTerrainSpatialKey{EVoxelTerrainRepresentation::Macro, FIntVector(InData.Key.Coordinate.X, InData.Key.Coordinate.Y, 0), InData.Key.Level} ||
		InData.Side <= 1 || InData.Side > 257 || InData.Step != InData.Geometry.SampleStepCells || InData.LocalMinCells != FIntPoint::ZeroValue ||
		int64(InData.Side - 1) * InData.Step != InData.Geometry.Key.SideCells())
	{ OutError = TEXT("Macro representation disagrees with its actual geometry"); return false; }
	const int32 Count = InData.Side * InData.Side;
	if (Count <= 0 || Count > MaxCells || InData.Height.Num() != Count || InData.WaterHeight.Num() != Count ||
		InData.WaterKind.Num() != Count ||
		InData.SurfaceClass.Num() != Count || InData.ForestCoverage.Num() != Count || InData.SnowCoverage.Num() != Count ||
		InData.LargeStructures.Num() > 4096 || InData.DistantCells.Num() > MaxDistantCells)
	{
		OutError = TEXT("Voxel macro representation is invalid"); return false;
	}
	const uint64 RawBytes = 104 + uint64(Count) * 13 + uint64(InData.LargeStructures.Num()) * 16 +
		uint64(InData.DistantCells.Num()) * 44;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> RawLease;
	FVoxelByteWriter Writer{int32(RawBytes)};
	if (!ReserveRawWriter(RawBytes, Writer, InResources, InConsumer, RawLease, OutError)) return false;
	Writer.U32(RepresentationMagic); Writer.U16(RepresentationVersion); Writer.U8(3);
	WriteTileKey(Writer, InData.Key.Coordinate, InData.Key.Level);
	Writer.U64(InData.Revision);
	if (!WriteGeometry(Writer, InData.Geometry, InData.RequestId, InData.Revision, OutError)) return false;
	Writer.U16(InData.Side); Writer.I32(InData.Step); Writer.U32(Count);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		if (InData.WaterKind[Index] > static_cast<uint8>(EVoxelWaterKind::Edited))
		{
			OutError = TEXT("Voxel macro water kind is invalid"); return false;
		}
		Writer.I32(InData.Height[Index]); Writer.I32(InData.WaterHeight[Index]); Writer.U16(InData.SurfaceClass[Index]);
		Writer.U8(InData.WaterKind[Index]); Writer.U8(InData.ForestCoverage[Index]); Writer.U8(InData.SnowCoverage[Index]);
	}
	Writer.U16(InData.LargeStructures.Num());
	for (const FVoxelMacroStructureProxy& Structure : InData.LargeStructures)
	{
		Writer.I32(Structure.Coordinate.X); Writer.I32(Structure.Coordinate.Y); Writer.I32(Structure.GroundZ);
		Writer.U16(Structure.StructureIndex); Writer.U16(Structure.Radius);
	}
	WriteDistantCells(Writer, InData.DistantCells);
	return FinishCompressed(Writer, OutBytes, OutError, InResources, InConsumer);
}

bool FVoxelRepresentationSync::DecodeMacro(
	TConstArrayView<uint8> InBytes,
	FVoxelMacroTileData& OutData,
	FString& OutError,
	TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
	const FVoxelGenerationConsumerKey& InConsumer,
	const FVoxelGeometryWireHeader* InExpected)
{
	FVoxelBinaryBuffer Raw;
	if (!OpenCompressed(InBytes, Raw, OutError, InResources, InConsumer)) return false;
	FVoxelByteReader Reader(Raw.Bytes);
	if (Reader.U32() != RepresentationMagic || Reader.U16() != RepresentationVersion || Reader.U8() != 3)
	{
		OutError = TEXT("Voxel macro representation header is invalid"); return false;
	}
	FVoxelMacroTileData Data;
	Data.Key.Coordinate.X = Reader.I32(); Data.Key.Coordinate.Y = Reader.I32(); Data.Key.Level = Reader.U8();
	Data.Revision = Reader.U64();
	ReadGeometry(Reader, Data.Geometry, Data.RequestId, Data.Revision);
	if (!Reader.IsValid() || (InExpected && !VoxelGeometryWire::ValidateExpected(
		{Data.Geometry, Data.RequestId, Data.Revision}, *InExpected, OutError))) return false;
	if (Data.Geometry.Key != FVoxelTerrainSpatialKey{EVoxelTerrainRepresentation::Macro, FIntVector(Data.Key.Coordinate.X, Data.Key.Coordinate.Y, 0), Data.Key.Level})
	{ OutError = TEXT("Representation payload key disagrees with geometry header"); return false; }
	Data.Side = Reader.U16(); Data.Step = Reader.I32();
	const uint32 Count = Reader.U32();
	if (Data.Step != Data.Geometry.SampleStepCells || Data.Side <= 1 || Data.Side > 257 || int64(Data.Side - 1) * Data.Step != Data.Geometry.Key.SideCells() || Count != static_cast<uint32>(Data.Side * Data.Side) || Count > MaxCells || !Reader.IsValid() || uint64(Count) * 13 > uint64(Reader.Remaining()))
	{ OutError = TEXT("Invalid macro sampling resolution"); return false; }
	Data.Height.Reserve(Count); Data.WaterHeight.Reserve(Count); Data.WaterKind.Reserve(Count); Data.SurfaceClass.Reserve(Count);
	Data.ForestCoverage.Reserve(Count); Data.SnowCoverage.Reserve(Count);
	for (uint32 Index = 0; Index < Count && Reader.IsValid(); ++Index)
	{
		Data.Height.Add(Reader.I32()); Data.WaterHeight.Add(Reader.I32()); Data.SurfaceClass.Add(Reader.U16());
		const uint8 Kind = Reader.U8();
		if (Kind > static_cast<uint8>(EVoxelWaterKind::Edited)) Reader.Reject();
		Data.WaterKind.Add(Kind); Data.ForestCoverage.Add(Reader.U8()); Data.SnowCoverage.Add(Reader.U8());
	}
	const uint16 StructureCount = Reader.U16();
	if (StructureCount > 4096 || !Reader.IsValid() || uint64(StructureCount) * 16 > uint64(Reader.Remaining())) { OutError = TEXT("Invalid macro structure count"); return false; }
	Data.LargeStructures.Reserve(StructureCount);
	for (uint16 Index = 0; Index < StructureCount && Reader.IsValid(); ++Index)
	{
		FVoxelMacroStructureProxy Structure;
		Structure.Coordinate.X = Reader.I32(); Structure.Coordinate.Y = Reader.I32(); Structure.GroundZ = Reader.I32();
		Structure.StructureIndex = Reader.U16(); Structure.Radius = Reader.U16(); Data.LargeStructures.Add(Structure);
	}
	ReadDistantCells(Reader, Data.DistantCells);
	if (!Reader.End()) { OutError = TEXT("Voxel macro representation payload is invalid"); return false; }
	OutData = MoveTemp(Data); OutError.Reset(); return true;
}

FVoxelRepresentationPageBuild::FVoxelRepresentationPageBuild(
	TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
	const FVoxelGenerationConsumerKey& InConsumer)
	: Resources(MoveTemp(InResources)), Consumer(InConsumer)
{
}

bool FVoxelRepresentationPageBuild::Initialize(FVoxelRepresentationBuildInput&& InInput, FString& OutError)
{
	if (!Consumer.IsValid() || !InInput.Config || !InInput.Cache || !InInput.Generator || !InInput.Registry)
	{ OutError = TEXT("Invalid immutable representation build input"); return false; }
	if (!ValidateRepresentationRequest(InInput.Request, OutError)) return false;
	const int64 Samples = InInput.Request.Geometry.Key.SideCells() / InInput.Request.Geometry.SampleStepCells;
	if (Samples > (InInput.Request.Geometry.Key.IsColumn() ? 256 : 128))
	{ OutError = TEXT("Oversized: server representation sampling exceeds wire limits"); return false; }
	FVoxelRepresentationPageBuild Candidate(Resources, Consumer);
	Candidate.Input = MoveTemp(InInput);
	const bool bGeometryReady = Candidate.Input.Request.Type == EVoxelRepresentationWireType::VoxelProxy
		? Candidate.Proxy.Initialize(Candidate.Input.Request.Geometry, OutError) : Candidate.Heightfield.Initialize(Candidate.Input.Request.Geometry, OutError);
	if (!bGeometryReady || !Candidate.OverlayReader.Initialize(MoveTemp(Candidate.Input.OverlayCapture), OutError)) return false;
	*this = MoveTemp(Candidate);
	return true;
}

uint64 FVoxelRepresentationPageBuild::InputBytes() const
{
	return OverlayReader.AllocatedBytes() + OverlayReadResult.ContentRevisions.GetAllocatedSize() +
		(OverlayReadResult.Snapshot ? OverlayReadResult.Snapshot->GetAllocatedBytes() : 0) +
		(OverlayReadResult.ReadLeaseSet ? OverlayReadResult.ReadLeaseSet->GetMetadataBytes() : 0) +
		(Input.Request.Type == EVoxelRepresentationWireType::VoxelProxy
		? Proxy.MaximumRetainedBytes() : Heightfield.MaximumRetainedBytes());
}

uint64 FVoxelRepresentationPageBuild::RawCapacityBytes() const
{
	return Input.Request.Type == EVoxelRepresentationWireType::VoxelProxy ? Proxy.MaximumRetainedBytes() : Heightfield.MaximumRetainedBytes();
}

uint64 FVoxelRepresentationPageBuild::MaximumWireBytes() const
{
	FString Error;
	if (!Input.Request.Geometry.Validate(Error)) return 0;
	const int64 Samples = Input.Request.Geometry.Key.SideCells() / Input.Request.Geometry.SampleStepCells;
	uint64 RawBytes = 0;
	if (Input.Request.Type == EVoxelRepresentationWireType::VoxelProxy)
	{
		RawBytes = 105 + uint64(Samples * Samples) * (4 * Samples + 24);
	}
	else
	{
		RawBytes = 102 + uint64((Samples + 1) * (Samples + 1)) * 13 + uint64(MaxDistantCells) * 44;
		if (Input.Request.Type == EVoxelRepresentationWireType::MacroTerrain) RawBytes += 2 + 4096 * 16;
	}
	if (RawBytes == 0 || RawBytes > MaxRepresentationRawBytes) return 0;
	const int32 Bound = VoxelBinary::CompressionBound(int32(RawBytes));
	return Bound > 0 ? FMath::Min(uint64(Bound) + 16, uint64(FVoxelNetworkCodec::MaxRepresentationBytes)) : 0;
}

const FVoxelFineOverlayReadResult& FVoxelRepresentationPageBuild::GetOverlayReadResult() const
{
	return OverlayReadResult;
}

bool FVoxelRepresentationPageBuild::Advance(FVoxelBinaryBuffer& OutBytes, bool& bOutComplete,
	FString& OutError, const TAtomic<bool>* InCancel)
{
	bOutComplete = false;
	if (InCancel && InCancel->Load()) { OutError = TEXT("Canceled"); return false; }
	if (bEncodedTaken) { OutError = TEXT("Representation encoding result was already taken"); return false; }
	if (!OverlayReadResult.Snapshot)
	{
		const TAtomic<bool> NeverCanceled(false);
		bool bReadComplete = false;
		if (!OverlayReader.Advance(1, InCancel ? *InCancel : NeverCanceled, bReadComplete, OutError)) return false;
		if (!bReadComplete) return true;
		if (!OverlayReader.TakeResult(OverlayReadResult))
		{
			OutError = TEXT("Overlay固定读取结果不能提交");
			return false;
		}
	}
	if (!bGenerated)
	{
		const FVoxelOverlaySnapshotSet& Overlays = *OverlayReadResult.Snapshot;
		if (Input.Request.Type == EVoxelRepresentationWireType::VoxelProxy)
		{
			const FVoxelVoxelProxyBuilder Builder(Input.Config.ToSharedRef(), Input.Cache.ToSharedRef(), Input.Generator);
			if (!Proxy.Advance(Builder, Overlays, ProxyData, bGenerated, OutError, InCancel)) return false;
			if (bGenerated) { ProxyData.Revision = Input.Revision; ProxyData.RequestId = Input.Request.Serial; }
		}
		else if (Input.Request.Type == EVoxelRepresentationWireType::SurfaceProxy)
		{
			const FVoxelSurfaceProxyBuilder Builder(Input.Generator.ToSharedRef(), Input.Config.ToSharedRef(),
				Input.Settings, Overlays, Input.Registry.ToSharedRef(), Input.Lod);
			if (!Heightfield.Advance(Builder, SurfaceData, bGenerated, OutError, InCancel)) return false;
			if (bGenerated) { SurfaceData.Revision = Input.Revision; SurfaceData.RequestId = Input.Request.Serial; }
		}
		else if (Input.Request.Type == EVoxelRepresentationWireType::MacroTerrain)
		{
			const FVoxelMacroTerrainBuilder Builder(Input.Generator.ToSharedRef(), Input.Config.ToSharedRef(),
				Input.Settings, Overlays, Input.Registry.ToSharedRef(), Input.Lod);
			if (!Heightfield.Advance(Builder, MacroData, bGenerated, OutError, InCancel)) return false;
			if (bGenerated) { MacroData.Revision = Input.Revision; MacroData.RequestId = Input.Request.Serial; }
		}
		else { OutError = TEXT("Invalid representation type"); return false; }
		// 生成与编码是两个 Worker 片段，原生成 scratch 在压缩接纳前归还。
		return true;
	}
	// Codec 拒收保留冻结正文；再次执行只重试编码，不再次 Take/生成或更换身份。
	const bool bEncoded = Input.Request.Type == EVoxelRepresentationWireType::VoxelProxy
		? FVoxelRepresentationSync::EncodeVoxelProxy(ProxyData, OutBytes, OutError, Resources, Consumer)
		: Input.Request.Type == EVoxelRepresentationWireType::SurfaceProxy
		? FVoxelRepresentationSync::EncodeSurface(SurfaceData, OutBytes, OutError, Resources, Consumer)
		: FVoxelRepresentationSync::EncodeMacro(MacroData, OutBytes, OutError, Resources, Consumer);
	if (!bEncoded) return false;
	ProxyData = FVoxelVoxelProxyData{};
	SurfaceData = FVoxelSurfaceTileData{};
	MacroData = FVoxelMacroTileData{};
	bEncodedTaken = bOutComplete = true;
	return true;
}
