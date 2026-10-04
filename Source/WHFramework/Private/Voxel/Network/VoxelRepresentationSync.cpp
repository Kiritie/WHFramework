#include "Voxel/Network/VoxelRepresentationSync.h"
#include "Voxel/Network/VoxelGeometryWire.h"

#include "Voxel/Network/VoxelNetworkCodec.h"
#include "Voxel/Serialization/VoxelBinaryCodec.h"
#include "Voxel/Rendering/VoxelWaterView.h"
#include "Voxel/Rendering/VoxelViewLod.h"
#include "Voxel/VoxelModule.h"

namespace
{
	constexpr uint32 RepresentationMagic = 0x34505256;
	constexpr uint16 RepresentationVersion = 6;
	constexpr int32 MaxRepresentationRawBytes = 16 * 1024 * 1024;
	constexpr int32 MaxCells = 1 << 20;
	constexpr uint32 MaxDistantCells = 16384;

	FVoxelGenerationBounds ResolveBuildBounds(const FVoxelTerrainGeometry& InGeometry,
		const FVoxelGenerationSettings& InSettings)
	{
		FIntVector Min, Max;
		check(InGeometry.Key.TryBounds(InGeometry.WorldMinZ, InGeometry.WorldMaxZ, Min, Max));
		const int32 Step = InGeometry.SampleStepCells;
		const int32 Margin = FMath::Max(Step, InSettings.Ecology.Tree.bEnabled ? InSettings.Ecology.Tree.CrownRadius : 0);
		Min -= FIntVector(Margin, Margin, InGeometry.Key.IsColumn() ? 0 : Step);
		Max += FIntVector(Margin, Margin, InGeometry.Key.IsColumn() ? 0 : Step);
		return {Min, Max};
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
		TArray<uint8> Bytes;
		FVoxelGeometryWireHeader Header;
		FString Error;
		if (!Reader.Raw(VoxelGeometryWire::HeaderBytes, Bytes) || !VoxelGeometryWire::Decode(Bytes, Header, Error) || Header.ContentRevision != Revision)
		{ Reader.Reject(); return; }
		Geometry = Header.Geometry;
		RequestId = Header.RequestId;
	}

	uint64 ResolveBuildRevision(
		const FVoxelWorldRuntime& InRuntime,
		const EVoxelRepresentationWireType InType,
		const FVoxelRepresentationWireKey& InKey)
	{
		switch (InType)
		{
		case EVoxelRepresentationWireType::VoxelProxy:
			return InRuntime.GetChangeHierarchy().GetVoxelProxyRevision({ InKey.Coordinate, InKey.Level });
		case EVoxelRepresentationWireType::SurfaceProxy:
			return InRuntime.GetChangeHierarchy().GetSurfaceRevision({ FIntPoint(InKey.Coordinate.X, InKey.Coordinate.Y), InKey.Level });
		case EVoxelRepresentationWireType::MacroTerrain:
			return InRuntime.GetChangeHierarchy().GetMacroRevision({ FIntPoint(InKey.Coordinate.X, InKey.Coordinate.Y), InKey.Level });
		default:
			return 0;
		}
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
		if (Count > MaxDistantCells) InReader.Reject();
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

	bool FinishCompressed(FVoxelByteWriter& InWriter, TArray<uint8>& OutBytes, FString& OutError)
	{
		TArray<uint8> Raw;
		if (!InWriter.Finish(Raw) || !VoxelBinary::Compress(Raw, OutBytes, FVoxelNetworkCodec::MaxRepresentationBytes))
		{
			OutError = TEXT("Voxel representation encoding failed");
			return false;
		}
		OutError.Reset();
		return true;
	}

	bool OpenCompressed(TConstArrayView<uint8> InBytes, TArray<uint8>& OutRaw, FString& OutError)
	{
		if (!VoxelBinary::Decompress(InBytes, OutRaw, MaxRepresentationRawBytes))
		{
			OutError = TEXT("Voxel representation compression envelope is invalid");
			return false;
		}
		return true;
	}
}

bool FVoxelRepresentationSync::PrepareServerBuild(
	UVoxelModule& InModule,
	const FVoxelRepresentationRequest& InRequest,
	FVoxelRepresentationBuildInput& OutInput,
	FString& OutError)
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

	if (!InRequest.Geometry.Validate(OutError)) return false;
	const FVoxelGenerationBounds Bounds = ResolveBuildBounds(InRequest.Geometry, Input.Settings);
	if (!InModule.EnsureEnvironmentBounds(Bounds, EVoxelWorkClass::Exploration, 0, 0.0, 0.0).bReady ||
		!Input.Generator->EnsureCoarseOverlayBoundsReady(Bounds, EVoxelWorkClass::Exploration, 0).bReady)
	{ OutError = TEXT("DependencyNotReady: representation environment"); return false; }
	if (!InModule.CaptureOverlays(Bounds, Input.Overlays, OutError))
	{
		return false;
	}
	Input.Revision = ResolveBuildRevision(*Runtime, InRequest.Type, InRequest.Key);
	OutInput = MoveTemp(Input);
	OutError.Reset();
	return true;
}

bool FVoxelRepresentationSync::BuildServerData(const FVoxelRepresentationBuildInput& InInput,
	TArray<uint8>& OutBytes, FString& OutError, const TAtomic<bool>* InCancel)
{
	FVoxelRepresentationPageBuild Build;
	FVoxelRepresentationBuildInput Input = InInput;
	if (!Build.Initialize(MoveTemp(Input), OutError)) return false;
	bool bComplete = false;
	while (!bComplete)
		if (!Build.Advance(OutBytes, bComplete, OutError, InCancel)) return false;
	return true;
}

bool FVoxelRepresentationSync::EncodeVoxelProxy(
	const FVoxelVoxelProxyData& InData,
	TArray<uint8>& OutBytes,
	FString& OutError)
{
	if (InData.Key.Level > 20 || InData.Geometry.SampleStepCells <= 0 || InData.GridSide !=
			InData.Key.GetSide() / InData.Geometry.SampleStepCells ||
		InData.Cells.Num() !=
			(InData.GridSide * InData.GridSide * InData.GridSide))
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

	FVoxelByteWriter Writer(MaxRepresentationRawBytes);

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

	return FinishCompressed(
		Writer,
		OutBytes,
		OutError);
}

bool FVoxelRepresentationSync::DecodeVoxelProxy(
	TConstArrayView<uint8> InBytes,
	FVoxelVoxelProxyData& OutData,
	FString& OutError)
{
	TArray<uint8> Raw;

	if (!OpenCompressed(
			InBytes,
			Raw,
			OutError))
	{
		return false;
	}

	FVoxelByteReader Reader(
		Raw);

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

	if (Data.Key.Level > 20 || Data.Geometry.SampleStepCells <= 0 || Data.GridSide != Data.Key.GetSide() / Data.Geometry.SampleStepCells || Data.GridSide > 128 ||
		Count != (Data.GridSide * Data.GridSide * Data.GridSide))
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
	TArray<uint8>& OutBytes,
	FString& OutError)
{
	const int32 Count = InData.Side * InData.Side;
	if (Count <= 0 || Count > MaxCells || InData.GroundZ.Num() != Count || InData.WaterZ.Num() != Count ||
		InData.SurfaceMaterial.Num() != Count || InData.Biome.Num() != Count || InData.Flags.Num() != Count ||
		InData.DistantCells.Num() > MaxDistantCells)
	{
		OutError = TEXT("Voxel surface representation is invalid");
		return false;
	}
	FVoxelByteWriter Writer(FVoxelNetworkCodec::MaxRepresentationBytes);
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
	return FinishCompressed(Writer, OutBytes, OutError);
}

bool FVoxelRepresentationSync::DecodeSurface(
	TConstArrayView<uint8> InBytes,
	FVoxelSurfaceTileData& OutData,
	FString& OutError)
{
	TArray<uint8> Raw;
	if (!OpenCompressed(InBytes, Raw, OutError)) return false;
	FVoxelByteReader Reader(Raw);
	if (Reader.U32() != RepresentationMagic || Reader.U16() != RepresentationVersion || Reader.U8() != 2)
	{
		OutError = TEXT("Voxel surface representation header is invalid"); return false;
	}
	FVoxelSurfaceTileData Data;
	Data.Key.Coordinate.X = Reader.I32(); Data.Key.Coordinate.Y = Reader.I32(); Data.Key.Level = Reader.U8();
	Data.Revision = Reader.U64();
	ReadGeometry(Reader, Data.Geometry, Data.RequestId, Data.Revision);
	if (Data.Geometry.Key != FVoxelTerrainSpatialKey{EVoxelTerrainRepresentation::Surface, FIntVector(Data.Key.Coordinate.X, Data.Key.Coordinate.Y, 0), Data.Key.Level})
	{ OutError = TEXT("Representation payload key disagrees with geometry header"); return false; }
	Data.Side = Reader.U16(); Data.Step = Reader.I32();
	const uint32 Count = Reader.U32();
	if (Data.Step != Data.Geometry.SampleStepCells || Data.Side <= 1 || int64(Data.Side - 1) * Data.Step != Data.Geometry.Key.SideCells() || Count != static_cast<uint32>(Data.Side * Data.Side) || Count > MaxCells) Reader.Reject();
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
	TArray<uint8>& OutBytes,
	FString& OutError)
{
	const int32 Count = InData.Side * InData.Side;
	if (Count <= 0 || Count > MaxCells || InData.Height.Num() != Count || InData.WaterHeight.Num() != Count ||
		InData.WaterKind.Num() != Count ||
		InData.SurfaceClass.Num() != Count || InData.ForestCoverage.Num() != Count || InData.SnowCoverage.Num() != Count ||
		InData.LargeStructures.Num() > 4096 || InData.DistantCells.Num() > MaxDistantCells)
	{
		OutError = TEXT("Voxel macro representation is invalid"); return false;
	}
	FVoxelByteWriter Writer(FVoxelNetworkCodec::MaxRepresentationBytes);
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
	return FinishCompressed(Writer, OutBytes, OutError);
}

bool FVoxelRepresentationSync::DecodeMacro(
	TConstArrayView<uint8> InBytes,
	FVoxelMacroTileData& OutData,
	FString& OutError)
{
	TArray<uint8> Raw;
	if (!OpenCompressed(InBytes, Raw, OutError)) return false;
	FVoxelByteReader Reader(Raw);
	if (Reader.U32() != RepresentationMagic || Reader.U16() != RepresentationVersion || Reader.U8() != 3)
	{
		OutError = TEXT("Voxel macro representation header is invalid"); return false;
	}
	FVoxelMacroTileData Data;
	Data.Key.Coordinate.X = Reader.I32(); Data.Key.Coordinate.Y = Reader.I32(); Data.Key.Level = Reader.U8();
	Data.Revision = Reader.U64();
	ReadGeometry(Reader, Data.Geometry, Data.RequestId, Data.Revision);
	if (Data.Geometry.Key != FVoxelTerrainSpatialKey{EVoxelTerrainRepresentation::Macro, FIntVector(Data.Key.Coordinate.X, Data.Key.Coordinate.Y, 0), Data.Key.Level})
	{ OutError = TEXT("Representation payload key disagrees with geometry header"); return false; }
	Data.Side = Reader.U16(); Data.Step = Reader.I32();
	const uint32 Count = Reader.U32();
	if (Data.Step != Data.Geometry.SampleStepCells || Data.Side <= 1 || int64(Data.Side - 1) * Data.Step != Data.Geometry.Key.SideCells() || Count != static_cast<uint32>(Data.Side * Data.Side) || Count > MaxCells) Reader.Reject();
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
	if (StructureCount > 4096) Reader.Reject();
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

bool FVoxelRepresentationPageBuild::Initialize(FVoxelRepresentationBuildInput&& InInput, FString& OutError)
{
	if (!InInput.Config || !InInput.Cache || !InInput.Generator || !InInput.Registry ||
		!InInput.Request.Geometry.Validate(OutError) || InInput.Request.Serial == 0)
	{ OutError = TEXT("Invalid immutable representation build input"); return false; }
	Input = MoveTemp(InInput);
	return Input.Request.Type == EVoxelRepresentationWireType::VoxelProxy
		? Proxy.Initialize(Input.Request.Geometry, OutError) : Heightfield.Initialize(Input.Request.Geometry, OutError);
}

uint64 FVoxelRepresentationPageBuild::InputBytes() const
{
	return Input.Overlays.GetAllocatedBytes() + (Input.Request.Type == EVoxelRepresentationWireType::VoxelProxy
		? Proxy.MaximumRetainedBytes() : Heightfield.MaximumRetainedBytes());
}

bool FVoxelRepresentationPageBuild::Advance(TArray<uint8>& OutBytes, bool& bOutComplete,
	FString& OutError, const TAtomic<bool>* InCancel)
{
	bOutComplete = false;
	if (InCancel && InCancel->Load()) { OutError = TEXT("Canceled"); return false; }
	if (Input.Request.Type == EVoxelRepresentationWireType::VoxelProxy)
	{
		const FVoxelVoxelProxyBuilder Builder(Input.Config.ToSharedRef(), Input.Cache.ToSharedRef(), Input.Generator);
		FVoxelVoxelProxyData Data;
		if (!Proxy.Advance(Builder, Input.Overlays, Data, bOutComplete, OutError, InCancel)) return false;
		if (!bOutComplete) return true;
		Data.Revision = Input.Revision; Data.RequestId = Input.Request.Serial;
		return FVoxelRepresentationSync::EncodeVoxelProxy(Data, OutBytes, OutError);
	}
	if (Input.Request.Type == EVoxelRepresentationWireType::SurfaceProxy)
	{
		const FVoxelSurfaceProxyBuilder Builder(Input.Generator.ToSharedRef(), Input.Config.ToSharedRef(),
			Input.Settings, Input.Overlays, Input.Registry.ToSharedRef(), Input.Lod);
		FVoxelSurfaceTileData Data;
		if (!Heightfield.Advance(Builder, Data, bOutComplete, OutError, InCancel)) return false;
		if (!bOutComplete) return true;
		Data.Revision = Input.Revision; Data.RequestId = Input.Request.Serial;
		return FVoxelRepresentationSync::EncodeSurface(Data, OutBytes, OutError);
	}
	if (Input.Request.Type == EVoxelRepresentationWireType::MacroTerrain)
	{
		const FVoxelMacroTerrainBuilder Builder(Input.Generator.ToSharedRef(), Input.Config.ToSharedRef(),
			Input.Settings, Input.Overlays, Input.Registry.ToSharedRef(), Input.Lod);
		FVoxelMacroTileData Data;
		if (!Heightfield.Advance(Builder, Data, bOutComplete, OutError, InCancel)) return false;
		if (!bOutComplete) return true;
		Data.Revision = Input.Revision; Data.RequestId = Input.Request.Serial;
		return FVoxelRepresentationSync::EncodeMacro(Data, OutBytes, OutError);
	}
	OutError = TEXT("Invalid representation type"); return false;
}
