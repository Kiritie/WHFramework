#include "Voxel/Network/VoxelRepresentationSync.h"

#include "Voxel/Network/VoxelNetworkCodec.h"
#include "Voxel/Serialization/VoxelBinaryCodec.h"
#include "Voxel/VoxelModule.h"

namespace
{
	constexpr uint32 RepresentationMagic = 0x34505256;
	constexpr uint16 RepresentationVersion = 3;
	constexpr int32 MaxCells = 4096;
	constexpr uint32 MaxDistantCells = 16384;

	FVoxelGenerationBounds ResolveBuildBounds(
		const EVoxelRepresentationWireType InType,
		const FVoxelRepresentationWireKey& InKey,
		const FVoxelGenerationSettings& InSettings)
	{
		if (InType ==
			EVoxelRepresentationWireType::
				VoxelProxy)
		{
			const FVoxelViewKey Key{ InKey.Coordinate, InKey.Level };
			FVoxelGenerationBounds Bounds = Key.GetBounds();
			Bounds.Min -= FIntVector(Key.GetStep());
			Bounds.Max += FIntVector(Key.GetStep());
			return Bounds;
		}

		const int32 Side =
			InType ==
				EVoxelRepresentationWireType::
					SurfaceProxy
				? FVoxelSurfaceTileData::
					CellSide *
					(1 << InKey.Level)
				: FVoxelMacroTileData::
					CellSide *
					(
						FVoxelMacroTileData::
							BaseStep <<
						InKey.Level
					);

		const FIntVector Min(
			InKey.Coordinate.X *
				Side,
			InKey.Coordinate.Y *
				Side,
			InSettings.MinZ);

		const int32 Step = InType == EVoxelRepresentationWireType::SurfaceProxy
			? 1 << InKey.Level : FVoxelMacroTileData::BaseStep << InKey.Level;
		const int32 Margin = FMath::Max(Step, InSettings.Ecology.Tree.bEnabled
			? InSettings.Ecology.Tree.CrownRadius : 0);
		return {
			Min - FIntVector(Margin, Margin, 0),
			FIntVector(
				Min.X + Side + Margin,
				Min.Y + Side + Margin,
				InSettings.MaxZ)
		};
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
		if (!VoxelBinary::Decompress(InBytes, OutRaw, FVoxelNetworkCodec::MaxRepresentationBytes))
		{
			OutError = TEXT("Voxel representation compression envelope is invalid");
			return false;
		}
		return true;
	}
}

bool FVoxelRepresentationSync::PrepareServerBuild(
	const UVoxelModule& InModule,
	const FVoxelRepresentationRequest& InRequest,
	FVoxelRepresentationBuildInput& OutInput,
	FString& OutError)
{
	const FVoxelWorldRuntime* Runtime = InModule.GetRuntime();
	FVoxelRepresentationBuildInput Input;
	Input.Request = InRequest;
	Input.Settings = InModule.GetManifest().Settings;
	Input.Config = InModule.GetGenerationConfig();
	Input.Cache = InModule.GetGenerationCache();
	Input.Generator = InModule.GetGenerator();
	Input.Registry = InModule.GetRegistry();
	if (!Input.Config || !Input.Cache || !Input.Generator || !Input.Registry || !Runtime)
	{
		OutError = TEXT("Voxel representation dependencies are unavailable");
		return false;
	}

	const FVoxelGenerationBounds Bounds = ResolveBuildBounds(
		InRequest.Type,
		InRequest.Key,
		Input.Settings);
	if (!InModule.CaptureOverlays(Bounds, Input.Overlays, OutError))
	{
		return false;
	}
	Input.Revision = ResolveBuildRevision(*Runtime, InRequest.Type, InRequest.Key);
	OutInput = MoveTemp(Input);
	OutError.Reset();
	return true;
}

bool FVoxelRepresentationSync::BuildServerData(
	const FVoxelRepresentationBuildInput& InInput,
	TArray<uint8>& OutBytes,
	FString& OutError,
	const TAtomic<bool>* InCancel)
{
	const FVoxelRepresentationRequest& InRequest = InInput.Request;
	const FVoxelOverlaySnapshotSet& OverlaySource = InInput.Overlays;
	if (InRequest.Type != EVoxelRepresentationWireType::VoxelProxy && !InInput.Registry)
	{
		OutError = TEXT("Voxel surface registry snapshot is unavailable");
		return false;
	}
	if (InRequest.Type ==
	EVoxelRepresentationWireType::
		VoxelProxy)
{
	const FVoxelViewKey Key {
		InRequest.Key.Coordinate,
		InRequest.Key.Level
	};

	FVoxelVoxelProxyData Data;

	const FVoxelVoxelProxyBuilder Builder(
		InInput.Config.ToSharedRef(),
		InInput.Cache.ToSharedRef());

	if (!Builder.Build(Key, InInput.Overlays, Data, OutError, InCancel))
	{
		return false;
	}

	Data.Revision =
		InInput.Revision;

	return EncodeVoxelProxy(
		Data,
		OutBytes,
		OutError);
}
	if (InRequest.Type == EVoxelRepresentationWireType::SurfaceProxy)
	{
		const FVoxelSurfaceTileKey Key {
			FIntPoint(InRequest.Key.Coordinate.X, InRequest.Key.Coordinate.Y),
			InRequest.Key.Level };
		FVoxelSurfaceTileData Data;
		const FVoxelSurfaceProxyBuilder Builder(
			InInput.Generator.ToSharedRef(),
			InInput.Config.ToSharedRef(),
			InInput.Settings,
			OverlaySource, InInput.Registry.ToSharedRef());
		if (!Builder.Build(Key, Data, OutError, InCancel)) return false;
		Data.Revision = InInput.Revision;
		return EncodeSurface(Data, OutBytes, OutError);
	}
	if (InRequest.Type == EVoxelRepresentationWireType::MacroTerrain)
	{
		const FVoxelMacroTileKey Key {
			FIntPoint(InRequest.Key.Coordinate.X, InRequest.Key.Coordinate.Y),
			InRequest.Key.Level };
		FVoxelMacroTileData Data;
		const FVoxelMacroTerrainBuilder Builder(InInput.Generator.ToSharedRef(), InInput.Config.ToSharedRef(), InInput.Settings, OverlaySource, InInput.Registry.ToSharedRef());
		if (!Builder.Build(Key, Data, OutError, InCancel)) return false;
		Data.Revision = InInput.Revision;
		return EncodeMacro(Data, OutBytes, OutError);
	}
	OutError = TEXT("Voxel representation type is invalid");
	return false;
}

bool FVoxelRepresentationSync::EncodeVoxelProxy(
	const FVoxelVoxelProxyData& InData,
	TArray<uint8>& OutBytes,
	FString& OutError)
{
	if (InData.GridSide !=
			16 ||
		InData.Cells.Num() !=
			4096)
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
				256)
		{
			OutError =
				TEXT(
					"Voxel proxy representation halo is invalid");

			return false;
		}
	}

	FVoxelByteWriter Writer(
		FVoxelNetworkCodec::
			MaxRepresentationBytes);

	Writer.U32(
		RepresentationMagic);

	Writer.U16(
		RepresentationVersion);

	Writer.U8(
		1);

	WriteViewKey(
		Writer,
		InData.Key);

	Writer.U64(
		InData.Revision);

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

	Data.Revision =
		Reader.U64();

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

	if (Data.GridSide != 16 ||
		Count != 4096)
	{
		Reader.Reject();
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
				256);

		for (int32 Index = 0;
			Index < 256 &&
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
	Writer.U64(InData.Revision); Writer.U16(InData.Side); Writer.I32(InData.Step); Writer.U32(Count);
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
	Data.Revision = Reader.U64(); Data.Side = Reader.U16(); Data.Step = Reader.I32();
	const uint32 Count = Reader.U32();
	if (Data.Side <= 0 || Count != static_cast<uint32>(Data.Side * Data.Side) || Count > MaxCells) Reader.Reject();
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
		InData.SurfaceClass.Num() != Count || InData.ForestCoverage.Num() != Count || InData.SnowCoverage.Num() != Count ||
		InData.LargeStructures.Num() > 4096 || InData.DistantCells.Num() > MaxDistantCells)
	{
		OutError = TEXT("Voxel macro representation is invalid"); return false;
	}
	FVoxelByteWriter Writer(FVoxelNetworkCodec::MaxRepresentationBytes);
	Writer.U32(RepresentationMagic); Writer.U16(RepresentationVersion); Writer.U8(3);
	WriteTileKey(Writer, InData.Key.Coordinate, InData.Key.Level);
	Writer.U64(InData.Revision); Writer.U16(InData.Side); Writer.I32(InData.Step); Writer.U32(Count);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		Writer.I32(InData.Height[Index]); Writer.I32(InData.WaterHeight[Index]); Writer.U16(InData.SurfaceClass[Index]);
		Writer.U8(InData.ForestCoverage[Index]); Writer.U8(InData.SnowCoverage[Index]);
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
	Data.Revision = Reader.U64(); Data.Side = Reader.U16(); Data.Step = Reader.I32();
	const uint32 Count = Reader.U32();
	if (Data.Side <= 0 || Count != static_cast<uint32>(Data.Side * Data.Side) || Count > MaxCells) Reader.Reject();
	Data.Height.Reserve(Count); Data.WaterHeight.Reserve(Count); Data.SurfaceClass.Reserve(Count);
	Data.ForestCoverage.Reserve(Count); Data.SnowCoverage.Reserve(Count);
	for (uint32 Index = 0; Index < Count && Reader.IsValid(); ++Index)
	{
		Data.Height.Add(Reader.I32()); Data.WaterHeight.Add(Reader.I32()); Data.SurfaceClass.Add(Reader.U16());
		Data.ForestCoverage.Add(Reader.U8()); Data.SnowCoverage.Add(Reader.U8());
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
