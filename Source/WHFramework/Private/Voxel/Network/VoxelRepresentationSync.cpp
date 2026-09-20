#include "Voxel/Network/VoxelRepresentationSync.h"

#include "Voxel/Network/VoxelNetworkCodec.h"
#include "Voxel/Save/VoxelDeltaCodec.h"
#include "Voxel/Serialization/VoxelBinaryCodec.h"
#include "Voxel/VoxelModule.h"

namespace
{
	constexpr uint32 RepresentationMagic = 0x34505256;
	constexpr uint16 RepresentationVersion = 2;
	constexpr int32 MaxCells = 4096;
	constexpr int32 ExactSectionSide = 16;

	FVoxelGenerationBounds ResolveBuildBounds(
		const EVoxelRepresentationWireType InType,
		const FVoxelRepresentationWireKey& InKey,
		const FVoxelGenerationSettings& InSettings)
	{
		if (InType ==
			EVoxelRepresentationWireType::
				VoxelProxy)
		{
			return FVoxelViewKey {
				InKey.Coordinate,
				InKey.Level
			}.
			GetBounds();
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

		return {
			Min,
			FIntVector(
				Min.X + Side,
				Min.Y + Side,
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
			return InRuntime.GetChangeHierarchy().GetVoxelProxyRevision(InKey.Coordinate);
		case EVoxelRepresentationWireType::SurfaceProxy:
			return InRuntime.GetChangeHierarchy().GetSurfaceRevision(
				FIntPoint(InKey.Coordinate.X, InKey.Coordinate.Y));
		case EVoxelRepresentationWireType::MacroTerrain:
			return InRuntime.GetChangeHierarchy().GetMacroRevision(
				FIntPoint(InKey.Coordinate.X, InKey.Coordinate.Y));
		default:
			return 0;
		}
	}

	class FStoredOverlaySource final : public IVoxelOverlaySource
	{
	public:
		explicit FStoredOverlaySource(const UVoxelModule& InModule)
			: Module(InModule)
		{
		}

		virtual bool EnumerateModifiedSections(
			const FVoxelGenerationBounds& InBounds,
			TArray<FIntVector>& OutSections,
			const TAtomic<bool>* InCancel = nullptr) const override
		{
			OutSections.Reset();
			if (const FVoxelWorldRuntime* Runtime = Module.GetRuntime())
			{
				return Runtime->GetChangeIndex().Enumerate(InBounds, OutSections, InCancel);
			}
			return true;
		}

		virtual bool ReadOverlay(
			const FIntVector& InSection,
			FVoxelOverlaySnapshot& OutOverlay) const override
		{
			const FVoxelWorldRuntime* Runtime = Module.GetRuntime();
			if (Runtime)
			{
				if (const FVoxelSection* Section = Runtime->FindSection(InSection);
					Section && Section->Status == EVoxelSectionStatus::DataReady)
				{
					OutOverlay.Section = InSection;
					OutOverlay.Revision = Section->CommittedRevision;
					OutOverlay.Blocks = Section->Overlay;
					return true;
				}
			}
			const TSharedPtr<const FVoxelRegistrySnapshot, ESPMode::ThreadSafe> Registry = Module.GetRegistry();
			if (!Registry)
			{
				return false;
			}
			TArray<uint8> Bytes;
			FString Error;
			if (Module.GetRegionStore().ReadSection(InSection, Bytes, Error) != EVoxelRegionRead::Loaded)
			{
				return false;
			}
			FVoxelPersistentSection Persistent;
			if (!FVoxelDeltaCodec::Decode(Bytes, Module.GetManifest(), *Registry, Persistent))
			{
				return false;
			}
			OutOverlay.Section = InSection;
			OutOverlay.Revision = Persistent.Revision;
			OutOverlay.Blocks = MoveTemp(Persistent.Blocks);
			return true;
		}

	private:
		const UVoxelModule& Module;
	};

	class FSnapshotOverlaySource final : public IVoxelOverlaySource
	{
	public:
		explicit FSnapshotOverlaySource(const TMap<FIntVector, FVoxelOverlaySnapshot>& InOverlays)
			: Overlays(InOverlays)
		{
		}

		virtual bool EnumerateModifiedSections(
			const FVoxelGenerationBounds& InBounds,
			TArray<FIntVector>& OutSections,
			const TAtomic<bool>* InCancel = nullptr) const override
		{
			OutSections.Reset();
			for (const TPair<FIntVector, FVoxelOverlaySnapshot>& Pair : Overlays)
			{
				if (InCancel && InCancel->Load())
				{
					OutSections.Reset();
					return false;
				}
				const FVoxelGenerationBounds SectionBounds {
					Pair.Key * ExactSectionSide,
					(Pair.Key + FIntVector(1)) * ExactSectionSide };
				if (SectionBounds.Intersects(InBounds))
				{
					OutSections.Add(Pair.Key);
				}
			}
			return true;
		}

		virtual bool ReadOverlay(
			const FIntVector& InSection,
			FVoxelOverlaySnapshot& OutOverlay) const override
		{
			if (const FVoxelOverlaySnapshot* Overlay = Overlays.Find(InSection))
			{
				OutOverlay = *Overlay;
				return true;
			}
			return false;
		}

	private:
		const TMap<FIntVector, FVoxelOverlaySnapshot>& Overlays;
	};

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
	if (!Input.Config || !Input.Cache || !Input.Generator || !Runtime)
	{
		OutError = TEXT("Voxel representation dependencies are unavailable");
		return false;
	}

	FStoredOverlaySource OverlaySource(InModule);
	const FVoxelGenerationBounds Bounds = ResolveBuildBounds(
		InRequest.Type,
		InRequest.Key,
		Input.Settings);
	TArray<FIntVector> ModifiedSections;
	OverlaySource.EnumerateModifiedSections(Bounds, ModifiedSections);
	for (const FIntVector& Section : ModifiedSections)
	{
		FVoxelOverlaySnapshot Overlay;
		if (!OverlaySource.ReadOverlay(Section, Overlay))
		{
			OutError = TEXT("Modified representation overlay could not be read");
			return false;
		}
		Input.Overlays.Add(Section, MoveTemp(Overlay));
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
	const FSnapshotOverlaySource OverlaySource(InInput.Overlays);
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

	if (!Builder.BuildNatural(
			Key,
			Data,
			OutError,
			InCancel))
	{
		return false;
	}

	const FVoxelGenerationBounds Bounds =
		Key.GetBounds();

	const int32 Step =
		Key.GetStep();

	auto ApplyOverlayAtWorld =
		[
			&InInput
		](
			const FIntVector& InWorld,
			FVoxelBlockState& InOutState)
		{
			const FIntVector Section(
				VoxelGeneration::FloorDivide(
					InWorld.X,
					ExactSectionSide),
				VoxelGeneration::FloorDivide(
					InWorld.Y,
					ExactSectionSide),
				VoxelGeneration::FloorDivide(
					InWorld.Z,
					ExactSectionSide));

			const FVoxelOverlaySnapshot* Overlay =
				InInput.
					Overlays.
					Find(
						Section);

			if (!Overlay)
			{
				return;
			}

			const FIntVector Local =
				InWorld -
				Section *
					ExactSectionSide;

			if (Local.X < 0 ||
				Local.X >=
					ExactSectionSide ||
				Local.Y < 0 ||
				Local.Y >=
					ExactSectionSide ||
				Local.Z < 0 ||
				Local.Z >=
					ExactSectionSide)
			{
				return;
			}

			const int32 CellIndex =
				Local.X +
				Local.Y *
					ExactSectionSide +
				Local.Z *
					ExactSectionSide *
					ExactSectionSide;

			if (const FVoxelBlockState* Modified =
				Overlay->
					Blocks.
					Find(
						CellIndex))
			{
				InOutState =
					*Modified;
			}
		};

	for (int32 Z = 0;
		Z < Data.GridSide;
		++Z)
	{
		if (InCancel &&
			InCancel->Load())
		{
			OutError =
				TEXT("Canceled");

			return false;
		}

		for (int32 Y = 0;
			Y < Data.GridSide;
			++Y)
		{
			for (int32 X = 0;
				X < Data.GridSide;
				++X)
			{
				const FIntVector World =
					Bounds.Min +
					FIntVector(
						X * Step +
							Step / 2,
						Y * Step +
							Step / 2,
						Z * Step +
							Step / 2);

				FVoxelBlockState& State =
					Data.Cells[
						X +
						Y *
							Data.GridSide +
						Z *
							Data.GridSide *
							Data.GridSide];

				ApplyOverlayAtWorld(
					World,
					State);
			}
		}
	}

	for (int32 Face = 0;
		Face < 6;
		++Face)
	{
		if (!Data.Known[
				Face] ||
			Data.Halo[
				Face].Num() !=
				256)
		{
			continue;
		}

		const int32 Axis =
			Face /
			2;

		const bool bNegative =
			(Face &
				1) !=
			0;

		const int32 U =
			(Axis + 1) %
			3;

		const int32 V =
			(Axis + 2) %
			3;

		for (int32 LocalV = 0;
			LocalV < 16;
			++LocalV)
		{
			for (int32 LocalU = 0;
				LocalU < 16;
				++LocalU)
			{
				FIntVector Local(
					0,
					0,
					0);

				Local[Axis] =
					bNegative
						? -1
						: 16;

				Local[U] =
					LocalU;

				Local[V] =
					LocalV;

				const FIntVector World =
					Bounds.Min +
					FIntVector(
						Local.X *
							Step +
							Step / 2,
						Local.Y *
							Step +
							Step / 2,
						Local.Z *
							Step +
							Step / 2);

				ApplyOverlayAtWorld(
					World,
					Data.Halo[
						Face][
							LocalU +
							LocalV *
								16]);
			}
		}
	}

	bool bHasAir =
		false;

	bool bHasSolid =
		false;

	for (const FVoxelBlockState State :
		Data.Cells)
	{
		bHasAir |=
			State.IsAir();

		bHasSolid |=
			!State.IsAir() &&
			State !=
				InInput.Config->
					Water &&
			State !=
				InInput.Config->
					Lava;
	}

	Data.bHasVisibleSurfaceEvidence =
		bHasAir &&
		bHasSolid;

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
			OverlaySource);
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
		const FVoxelMacroTerrainBuilder Builder(InInput.Generator.ToSharedRef());
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
		InData.SurfaceMaterial.Num() != Count || InData.Biome.Num() != Count || InData.Flags.Num() != Count)
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
		InData.LargeStructures.Num() > 4096)
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
	if (!Reader.End()) { OutError = TEXT("Voxel macro representation payload is invalid"); return false; }
	OutData = MoveTemp(Data); OutError.Reset(); return true;
}
