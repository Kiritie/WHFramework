#pragma once

#include "Voxel/Network/VoxelGeometryWire.h"
#include "Voxel/Network/VoxelRepresentationSync.h"
#include "Voxel/Rendering/VoxelCoarseBuildStorage.h"
#include "Voxel/Rendering/VoxelHeightfieldPageMesh.h"
#include "Voxel/Rendering/VoxelProxyPageMesh.h"
#include "Voxel/Rendering/VoxelWaterView.h"

// 压缩输入与解码对象先授予容量；计数仅保存计划，不构造整页临时网格。
struct FVoxelRemotePageBuild
{
	TSharedPtr<FVoxelCoarseBuildStorage, ESPMode::ThreadSafe> Storage;
	TArray<uint8> Bytes;
	TSharedPtr<const FVoxelRepresentationReplyData, ESPMode::ThreadSafe> SharedBytes;
	FVoxelGeometryWireHeader Expected;
	FVoxelTaskResult Data;
	FVoxelProxyPageMesh ProxyCounter;
	FVoxelHeightfieldPageMesh HeightfieldCounter;
	uint64 MeshPieceLimit = 0;
	uint64 GeometryLimit = 0;
	bool bDecoded = false;
	bool bCompacted = false;

	static uint64 RawCapacity(const FVoxelTerrainGeometry& Geometry)
	{
		FString Error;
		if (!Geometry.Validate(Error) ||
			(Geometry.Key.Representation != EVoxelTerrainRepresentation::VoxelProxy && !Geometry.Key.IsColumn()) ||
			(Geometry.Key.Representation == EVoxelTerrainRepresentation::VoxelProxy && Geometry.Key.Level == 0)) return 0;
		const int64 Samples = Geometry.Key.SideCells() / Geometry.SampleStepCells;
		if (!Geometry.Key.IsColumn())
		{
			if (Samples <= 0 || Samples > 128) return 0;
			return VoxelBoundedAllocation::WorkerSharedBytes<FVoxelVoxelProxyData>() +
				VoxelBoundedAllocation::ArrayBytes<FVoxelBlockState>(int32(Samples * Samples * Samples)) +
				VoxelBoundedAllocation::ArrayBytes<FVoxelCoverageRect>(int32(Samples * ((Samples + 1) / 2))) +
				6 * VoxelBoundedAllocation::ArrayBytes<FVoxelBlockState>(int32(Samples * Samples));
		}
		if (Samples <= 0 || Samples > 256) return 0;
		const int32 Count = int32((Samples + 1) * (Samples + 1));
		const uint64 Distant = VoxelBoundedAllocation::ArrayBytes<FVoxelDistantCell>(16384);
		if (Geometry.Key.Representation == EVoxelTerrainRepresentation::Surface)
			return VoxelBoundedAllocation::WorkerSharedBytes<FVoxelSurfaceTileData>() +
				VoxelBoundedAllocation::WorkerSharedBytes<FVoxelWaterSurfaceTileData>() +
				4 * VoxelBoundedAllocation::ArrayBytes<int32>(Count) +
				2 * VoxelBoundedAllocation::ArrayBytes<uint16>(Count) +
				2 * VoxelBoundedAllocation::ArrayBytes<uint8>(Count) + Distant;
		return VoxelBoundedAllocation::WorkerSharedBytes<FVoxelMacroTileData>() +
			2 * VoxelBoundedAllocation::ArrayBytes<int32>(Count) +
			VoxelBoundedAllocation::ArrayBytes<uint16>(Count) +
			3 * VoxelBoundedAllocation::ArrayBytes<uint8>(Count) + Distant +
			VoxelBoundedAllocation::ArrayBytes<FVoxelMacroStructureProxy>(4096);
	}

	FVoxelTaskResult Advance(const FVoxelRegistrySnapshot& Registry, const FVoxelShapeRegistry& Shapes,
		const FVoxelGenerationRuntimeConfig& Config, const double TextureStretch, const TAtomic<bool>& Cancel)
	{
		FVoxelTaskResult Result;
		if (Cancel.Load()) { Result.bCanceled = true; return Result; }
		const uint64 BeforeAdmission = Storage->Resources->ReleaseVersionExcludingOwner(Storage->Consumer);
		// 解压正文和 native inflate 由 codec 按头部真实大小授予。
		Storage->WorkingCapacityBytes = bDecoded ? MeshPieceLimit : 0;
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Working;
		if (!Storage->Begin(Result, Working)) return Result;
		if (!bDecoded)
		{
			bool bValid = false;
			const TConstArrayView<uint8> Encoded = SharedBytes ? TConstArrayView<uint8>(SharedBytes->Bytes) : TConstArrayView<uint8>(Bytes);
			const auto& Geometry = Expected.Geometry;
			if (Geometry.Key.Representation == EVoxelTerrainRepresentation::VoxelProxy)
			{
				if (!Data.VoxelProxy) Data.VoxelProxy = Storage->MakeResult<FVoxelVoxelProxyData>();
				bValid = FVoxelRepresentationSync::DecodeVoxelProxy(Encoded, *Data.VoxelProxy, Result.Error,
					Storage->Resources, Storage->Consumer, &Expected) &&
					Data.VoxelProxy->Revision == Expected.ContentRevision &&
					ProxyCounter.Initialize(Geometry, Result.Error, 0, true, MeshPieceLimit, GeometryLimit);
				Data.VoxelProxy->ResourceLease = Storage->GeometryLease;
			}
			else if (Geometry.Key.Representation == EVoxelTerrainRepresentation::Surface)
			{
				if (!Data.Surface) Data.Surface = Storage->MakeResult<FVoxelSurfaceTileData>();
				if (!Data.Water) Data.Water = Storage->MakeResult<FVoxelWaterSurfaceTileData>();
				bValid = FVoxelRepresentationSync::DecodeSurface(Encoded, *Data.Surface, Result.Error,
					Storage->Resources, Storage->Consumer, &Expected) &&
					Data.Surface->Revision == Expected.ContentRevision &&
					FVoxelWaterViewBuilder().Build(*Data.Surface, *Data.Water, Result.Error) &&
					HeightfieldCounter.Initialize(Geometry, Result.Error, 0, true, MeshPieceLimit, GeometryLimit);
				Data.Surface->ResourceLease = Data.Water->ResourceLease = Storage->GeometryLease;
			}
			else
			{
				if (!Data.Macro) Data.Macro = Storage->MakeResult<FVoxelMacroTileData>();
				bValid = FVoxelRepresentationSync::DecodeMacro(Encoded, *Data.Macro, Result.Error,
					Storage->Resources, Storage->Consumer, &Expected) &&
					Data.Macro->Revision == Expected.ContentRevision &&
					HeightfieldCounter.Initialize(Geometry, Result.Error, 0, true, MeshPieceLimit, GeometryLimit);
				Data.Macro->ResourceLease = Storage->GeometryLease;
			}
			if (!bValid)
			{
				if (Result.Error.StartsWith(TEXT("CapacityBlocked")))
				{
					Working.Reset();
					Storage->DeferUnspecifiedCapacity(Result, BeforeAdmission);
					return Result;
				}
				if (Result.Error.IsEmpty()) Result.Error = TEXT("Remote content revision differs from reply envelope");
				return Result;
			}
			const uint64 EncodedCapacity = Bytes.GetAllocatedSize();
			Bytes.Empty();
			SharedBytes.Reset();
			check(Storage->MetadataLease->Bytes() > EncodedCapacity);
			Storage->MetadataLease->Resize(Storage->MetadataLease->Bytes() - EncodedCapacity);
			bDecoded = true;
			Result.bSuccess = Result.bContinue = true;
			return Result;
		}
		bool bComplete = false;
		if (Data.VoxelProxy && Data.VoxelProxy->CapturedSurfaceRows != Data.VoxelProxy->GridSide)
		{
			Result.bSuccess = Data.VoxelProxy->AdvanceSurfaceCoverage(1, bComplete, Result.Error, &Cancel);
			Result.bContinue = Result.bSuccess; return Result;
		}
		if (!bCompacted)
		{
			if (Data.Surface && !Storage->CompactGeometryArray(Data.Surface->DistantCells,
				VoxelBoundedAllocation::ArrayBytes<FVoxelDistantCell>(16384), Working, Result)) return Result;
			if (Data.Macro && !Storage->CompactGeometryArray(Data.Macro->DistantCells,
				VoxelBoundedAllocation::ArrayBytes<FVoxelDistantCell>(16384), Working, Result)) return Result;
			bCompacted = true;
		}
		FVoxelSectionMeshResult Counted;
		if (Data.VoxelProxy)
			Result.bSuccess = ProxyCounter.Advance(*Data.VoxelProxy, Registry, Shapes, nullptr,
				Expected.Geometry.SampleStepCells / FMath::Clamp(double(Expected.Geometry.SampleStepCells), 1.0, TextureStretch),
				Counted, bComplete, Result.Error, &Cancel);
		else if (Data.Surface)
			Result.bSuccess = HeightfieldCounter.Advance(*Data.Surface, *Data.Water, Config, Registry,
				TextureStretch, Counted, bComplete, Result.Error, &Cancel);
		else
			Result.bSuccess = HeightfieldCounter.Advance(*Data.Macro, Config, Registry,
				TextureStretch, Counted, bComplete, Result.Error, &Cancel);
		if (!Result.bSuccess || !bComplete) { Result.bContinue = Result.bSuccess; return Result; }
		if (Data.VoxelProxy)
		{
			Data.VoxelProxy->EstimatedMeshBytes = ProxyCounter.GetEstimatedMeshBytes();
			Result.bKnownEmptyMesh = Data.VoxelProxy->EstimatedMeshBytes == 0;
		}
		if (Data.Surface) Data.Surface->EstimatedMeshBytes = HeightfieldCounter.GetEstimatedMeshBytes();
		if (Data.Macro) Data.Macro->EstimatedMeshBytes = HeightfieldCounter.GetEstimatedMeshBytes();
		Result.VoxelProxy = MoveTemp(Data.VoxelProxy); Result.Surface = MoveTemp(Data.Surface);
		Result.Water = MoveTemp(Data.Water); Result.Macro = MoveTemp(Data.Macro);
		Result.InputResourceLease = Storage->ResultInputLease;
		Storage->SettleCompleted(Result);
		return Result;
	}
};
