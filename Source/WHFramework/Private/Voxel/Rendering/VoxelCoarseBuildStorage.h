#pragma once

#include "Voxel/Task/VoxelBoundedAllocation.h"
#include "Voxel/Task/VoxelTaskScheduler.h"
#include "Voxel/Geometry/VoxelSectionMesher.h"
#include "Voxel/Rendering/VoxelMacroTerrain.h"
#include "Voxel/Rendering/VoxelProxyBuilder.h"
#include "Voxel/Rendering/VoxelWaterView.h"

// 输入控制结构、微步 scratch 和最终 CPU 输出共用 Scheduler 四账本。
// 第一次执行先保留完整输出容量，输入不能把完成路径挤出 Staged。
struct FVoxelCoarseBuildStorage
{
	TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	FVoxelGenerationConsumerKey Consumer;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> MetadataLease;
	// 完成结果只保留小额输入对象；构建数组由 MetadataLease 保持到最后一个 Worker 退出。
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResultInputLease;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> GeometryLease;
	uint64 GeometryCapacityBytes = 0;
	uint64 WorkingCapacityBytes = 8ull * 1024ull * 1024ull;
	TAtomic<bool> bWaitingCapacity{false};
	EVoxelTerrainResourcePool WaitingPool = EVoxelTerrainResourcePool::Count;
	uint64 WaitingBytes = 0;
	uint64 WaitingRequiredInputBytes = 0;
	int32 WaitingRequiredLeaseCount = 0;
	uint64 WaitingCapacityVersion = 0;

	FVoxelCoarseBuildStorage(TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
		const FVoxelGenerationConsumerKey& InConsumer,
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InMetadata,
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InResultInput)
		: Resources(InResources), Consumer(InConsumer), MetadataLease(MoveTemp(InMetadata)),
		ResultInputLease(MoveTemp(InResultInput))
	{
	}

	EVoxelTaskBlockReason BlockReason() const
	{
		if (!bWaitingCapacity.Load())
		{
			return EVoxelTaskBlockReason::None;
		}
		if (WaitingPool == EVoxelTerrainResourcePool::Count)
		{
			uint64 ReleaseVersion = 0;
			return !Resources->TryGetReleaseVersionExcludingOwner(Consumer, ReleaseVersion) ||
				ReleaseVersion <= WaitingCapacityVersion ? EVoxelTaskBlockReason::Memory : EVoxelTaskBlockReason::None;
		}
		EVoxelTerrainResourceAdmission Admission;
		if (!Resources->TryQueryAcquire(Consumer, WaitingPool, WaitingBytes, Admission, 0,
			WaitingRequiredInputBytes, WaitingRequiredLeaseCount) ||
			Admission == EVoxelTerrainResourceAdmission::CapacityBlocked)
		{
			return EVoxelTaskBlockReason::Memory;
		}
		// 不可满足或身份失效仍回到原执行入口产生确定性失败，不能永远停在等待态。
		return EVoxelTaskBlockReason::None;
	}

	void DeferCapacity(FVoxelTaskResult& OutResult, const EVoxelTerrainResourcePool InPool,
		const uint64 InBytes, const uint64 InRequiredInputBytes = 0, const int32 InRequiredLeaseCount = 0)
	{
		WaitingPool = InPool;
		WaitingBytes = InBytes;
		WaitingRequiredInputBytes = InRequiredInputBytes;
		WaitingRequiredLeaseCount = InRequiredLeaseCount;
		bWaitingCapacity.Store(true);
		OutResult.Error.Reset();
		OutResult.bSuccess = OutResult.bContinue = true;
	}

	void DeferUnspecifiedCapacity(FVoxelTaskResult& OutResult, const uint64 InBeforeAdmission)
	{
		WaitingPool = EVoxelTerrainResourcePool::Count;
		WaitingCapacityVersion = InBeforeAdmission;
		bWaitingCapacity.Store(true);
		OutResult.Error.Reset();
		OutResult.bSuccess = OutResult.bContinue = true;
	}

	// 已知的完成正文在排队前授额；无可行输出容量时不长期吸收新的构建输入。
	EVoxelTerrainResourceAdmission ReserveCompletion(uint64* OutRequiredInput = nullptr, int32* OutRequiredLeases = nullptr)
	{
		if (GeometryCapacityBytes == 0 || GeometryCapacityBytes > Resources->Snapshot().Limits.StagedBytes)
			return EVoxelTerrainResourceAdmission::CapacityUnachievable;
		if (GeometryLease) return EVoxelTerrainResourceAdmission::Granted;
		uint64 RequiredInput = 0;
		int32 RequiredLeases = 0;
		for (const auto& Lease : {MetadataLease, ResultInputLease})
		{
			if (!Lease) continue;
			if (!Lease->IsOwnedBy(Resources.Get())) return EVoxelTerrainResourceAdmission::InvalidRequest;
			if (Lease->GetPool() == EVoxelTerrainResourcePool::Input)
			{
				if (Lease->Bytes() > MAX_uint64 - RequiredInput) return EVoxelTerrainResourceAdmission::CapacityUnachievable;
				RequiredInput += Lease->Bytes();
			}
			++RequiredLeases;
		}
		if (OutRequiredInput)
		{
			*OutRequiredInput = RequiredInput;
		}
		if (OutRequiredLeases)
		{
			*OutRequiredLeases = RequiredLeases;
		}
		return Resources->TryAcquire(Consumer, EVoxelTerrainResourcePool::Staged,
			GeometryCapacityBytes, GeometryLease, 0, RequiredInput, RequiredLeases);
	}

	bool Begin(FVoxelTaskResult& OutResult,
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& OutWorking)
	{
		if (GeometryCapacityBytes == 0 || GeometryCapacityBytes > Resources->Snapshot().Limits.StagedBytes)
		{
			OutResult.Error = TEXT("Oversized: coarse completion cannot fit Staged capacity");
			return false;
		}
		uint64 RequiredInput = 0;
		int32 RequiredLeases = 0;
		const auto CompletionAdmission = ReserveCompletion(&RequiredInput, &RequiredLeases);
		if (CompletionAdmission == EVoxelTerrainResourceAdmission::CapacityUnachievable ||
			CompletionAdmission == EVoxelTerrainResourceAdmission::InvalidRequest)
		{
			OutResult.Error = TEXT("Oversized: coarse completion reservation");
			return false;
		}
		if (CompletionAdmission != EVoxelTerrainResourceAdmission::Granted)
		{
			DeferCapacity(OutResult, EVoxelTerrainResourcePool::Staged, GeometryCapacityBytes,
				RequiredInput, RequiredLeases);
			return false;
		}
		return BeginCount(OutResult, OutWorking);
	}

	bool BeginCount(FVoxelTaskResult& OutResult,
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& OutWorking)
	{
		if (WorkingCapacityBytes == 0)
		{
			bWaitingCapacity.Store(false);
			return true;
		}
		const auto Admission = Resources->TryAcquire(Consumer, EVoxelTerrainResourcePool::Working,
			WorkingCapacityBytes, OutWorking);
		if (Admission == EVoxelTerrainResourceAdmission::CapacityBlocked)
		{
			DeferCapacity(OutResult, EVoxelTerrainResourcePool::Working, WorkingCapacityBytes);
			return false;
		}
		if (Admission != EVoxelTerrainResourceAdmission::Granted)
		{
			OutResult.Error = Admission == EVoxelTerrainResourceAdmission::CapacityUnachievable ?
				TEXT("Oversized: coarse working capacity is unreachable") : TEXT("InvalidRequest: coarse working capacity");
			return false;
		}
		bWaitingCapacity.Store(false);
		return true;
	}

	template<typename ElementType>
	bool CompactGeometryArray(TArray<ElementType>& InArray, const uint64 InReservedArrayBytes,
		const TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& InWorking,
		FVoxelTaskResult& OutResult)
	{
		constexpr uint32 Alignment = TAllocatorTraits<FDefaultAllocator>::SupportsElementAlignment ? alignof(ElementType) : DEFAULT_ALIGNMENT;
		const uint64 OldArrayBytes = VoxelBoundedAllocation::QuantizeBytes(InArray.GetAllocatedSize(), Alignment);
		if (!GeometryLease || InReservedArrayBytes > GeometryCapacityBytes || OldArrayBytes > InReservedArrayBytes)
		{
			OutResult.Error = TEXT("Invalid coarse array completion reservation");
			return false;
		}
		const uint64 NewArrayBytes = VoxelBoundedAllocation::ArrayBytes<ElementType>(InArray.Num());
		if (InArray.Max() > InArray.Num() && NewArrayBytes)
		{
			if (!InWorking)
			{
				OutResult.Error = TEXT("Missing coarse array compaction scratch");
				return false;
			}
			if (InWorking->Bytes() < NewArrayBytes)
			{
				const auto Admission = InWorking->TryResize(NewArrayBytes);
				if (Admission == EVoxelTerrainResourceAdmission::CapacityBlocked)
				{
					// 本切片退出会归还 Working；下一次必须能重新取得整个临时缓冲。
					DeferCapacity(OutResult, EVoxelTerrainResourcePool::Working,
						FMath::Max(WorkingCapacityBytes, NewArrayBytes));
					return false;
				}
				if (Admission != EVoxelTerrainResourceAdmission::Granted)
				{
					OutResult.Error = TEXT("Oversized: coarse array compaction peak");
					return false;
				}
			}
		}
		InArray.Shrink();
		const uint64 RetainedArrayBytes = VoxelBoundedAllocation::QuantizeBytes(InArray.GetAllocatedSize(), Alignment);
		check(RetainedArrayBytes <= InReservedArrayBytes);
		const uint64 RetainedBytes = GeometryCapacityBytes - InReservedArrayBytes + RetainedArrayBytes;
		const bool bResized = GeometryLease->Resize(RetainedBytes);
		check(bResized);
		GeometryCapacityBytes = RetainedBytes;
		return true;
	}

	bool SettleCompleted(FVoxelTaskResult& InOutResult,
		const FVoxelSectionMeshResult* InAdditionalMesh = nullptr, const uint64 InAdditionalObjectBytes = 0)
	{
		if (!InOutResult.bSuccess || InOutResult.bContinue || InOutResult.bCanceled)
		{
			return false;
		}
		FCompletedCapacity Capacity(GeometryLease.Get());
		Capacity.Add(InAdditionalObjectBytes);
		Capacity.AddObject(InOutResult.VoxelProxy.Get());
		Capacity.AddObject(InOutResult.Surface.Get());
		Capacity.AddObject(InOutResult.Water.Get());
		Capacity.AddObject(InOutResult.Macro.Get());
		Capacity.AddObject(InOutResult.FineMesh.Get());
		Capacity.AddObject(InOutResult.VoxelProxyMesh.Get());
		Capacity.AddObject(InOutResult.VolumeBoundaryMesh.Get());
		Capacity.AddObject(InOutResult.SurfaceMesh.Get());
		Capacity.AddObject(InOutResult.WaterMesh.Get());
		Capacity.AddObject(InOutResult.MacroMesh.Get());
		Capacity.AddObject(InAdditionalMesh);
		if (!GeometryLease || !GeometryLease->IsOwnedBy(Resources.Get()) ||
			GeometryLease->GetPool() != EVoxelTerrainResourcePool::Staged ||
			!Capacity.bValid || Capacity.Bytes == 0 || Capacity.Bytes > GeometryCapacityBytes ||
			GeometryLease->Bytes() != GeometryCapacityBytes)
		{
			InOutResult.Error = TEXT("Invalid coarse completed allocation capacity");
			InOutResult.bSuccess = false;
			return false;
		}
		if (!GeometryLease->Resize(Capacity.Bytes))
		{
			InOutResult.Error = TEXT("Failed to settle coarse completed capacity");
			InOutResult.bSuccess = false;
			return false;
		}
		GeometryCapacityBytes = Capacity.Bytes;
		return true;
	}

	template<typename ObjectType>
	TSharedPtr<ObjectType, ESPMode::ThreadSafe> MakeResult() const
	{
		auto Result = VoxelBoundedAllocation::MakeWorkerShared<ObjectType>(GeometryLease);
		Result->ResourceLease = GeometryLease;
		return Result;
	}

private:
	// 只结算原正文租约拥有的对象和实际分配；冻结输入继续由各自原租约收费。
	struct FCompletedCapacity
	{
		const FVoxelTerrainResourceLease* Lease = nullptr;
		const void* Seen[11] = {};
		int32 SeenCount = 0;
		uint64 Bytes = 0;
		bool bValid = true;

		explicit FCompletedCapacity(const FVoxelTerrainResourceLease* InLease)
			: Lease(InLease)
		{
		}

		void Add(const uint64 InBytes)
		{
			if (InBytes > MAX_uint64 - Bytes)
			{
				bValid = false;
				return;
			}
			Bytes += InBytes;
		}

		template<typename ElementType>
		void AddArray(const TArray<ElementType>& InArray)
		{
			constexpr uint32 Alignment = TAllocatorTraits<FDefaultAllocator>::SupportsElementAlignment ?
				alignof(ElementType) : DEFAULT_ALIGNMENT;
			Add(VoxelBoundedAllocation::QuantizeBytes(InArray.GetAllocatedSize(), Alignment));
		}

		template<typename ObjectType>
		void AddObject(const ObjectType* InObject)
		{
			if (!InObject || !Lease || InObject->ResourceLease.Get() != Lease)
			{
				return;
			}
			for (int32 Index = 0; Index < SeenCount; ++Index)
			{
				if (Seen[Index] == InObject)
				{
					return;
				}
			}
			check(SeenCount < UE_ARRAY_COUNT(Seen));
			Seen[SeenCount++] = InObject;
			Add(VoxelBoundedAllocation::WorkerSharedBytes<ObjectType>());
			AddAllocations(*InObject);
		}

		void AddAllocations(const FVoxelVoxelProxyData& InData)
		{
			AddArray(InData.Cells);
			AddArray(InData.TopSurfaceCoverage);
			for (const auto& Halo : InData.Halo)
			{
				AddArray(Halo);
			}
		}

		void AddAllocations(const FVoxelSurfaceTileData& InData)
		{
			AddArray(InData.GroundZ);
			AddArray(InData.WaterZ);
			AddArray(InData.SurfaceMaterial);
			AddArray(InData.Biome);
			AddArray(InData.Flags);
			AddArray(InData.DistantCells);
		}

		void AddAllocations(const FVoxelWaterSurfaceTileData& InData)
		{
			AddArray(InData.GroundZ);
			AddArray(InData.WaterZ);
			AddArray(InData.WaterKind);
		}

		void AddAllocations(const FVoxelMacroTileData& InData)
		{
			AddArray(InData.Height);
			AddArray(InData.WaterHeight);
			AddArray(InData.WaterKind);
			AddArray(InData.SurfaceClass);
			AddArray(InData.ForestCoverage);
			AddArray(InData.SnowCoverage);
			AddArray(InData.LargeStructures);
			AddArray(InData.DistantCells);
		}

		void AddAllocations(const FVoxelSectionMeshResult& InMesh)
		{
			AddArray(InMesh.Batches);
			for (const auto& Batch : InMesh.Batches)
			{
				AddArray(Batch.Mesh.Vertices);
				AddArray(Batch.Mesh.Normals);
				AddArray(Batch.Mesh.Triangles);
				AddArray(Batch.Mesh.UV0);
				AddArray(Batch.Mesh.UV1);
				AddArray(Batch.Mesh.UV2);
				AddArray(Batch.Mesh.Colors);
				AddArray(Batch.Mesh.Tangents);
			}
		}
	};
};
