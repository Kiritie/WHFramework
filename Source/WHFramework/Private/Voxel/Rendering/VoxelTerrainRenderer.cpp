#include "Voxel/Rendering/VoxelTerrainRenderer.h"

#include "Async/Async.h"
#include "Components/SceneComponent.h"
#include "Containers/Ticker.h"
#include "DynamicMeshBuilder.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "LocalVertexFactory.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialRenderProxy.h"
#include "MeshElementCollector.h"
#include "Misc/ScopeExit.h"
#include "PrimitiveSceneProxy.h"
#include "PrimitiveUniformShaderParametersBuilder.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "RenderCommandFence.h"
#include "RenderGraphBuilder.h"
#include "RenderUtils.h"
#include "RenderingThread.h"
#include "SceneView.h"
#include "SceneViewExtension.h"
#include "StaticMeshResources.h"
#include "StaticMeshVertexData.h"
#include "UObject/StrongObjectPtr.h"
#include "Voxel/Components/VoxelTerrainPrimitiveComponent.h"
#include "Voxel/Rendering/VoxelFineShardBuild.h"
#include "Voxel/Rendering/VoxelProxyBuilder.h"
#include "Voxel/Rendering/VoxelMacroTerrain.h"
#include "Voxel/Rendering/VoxelWaterView.h"
#include "Voxel/Rendering/VoxelHeightfieldWaterTriangles.h"
#include "Voxel/Generation/VoxelGenerationBinding.h"
#include "Voxel/Geometry/DWVoxelBoundaryTransition.h"
#include "Voxel/Chunks/VoxelSectionKey.h"
#include "Voxel/Rendering/VoxelMaterialSet.h"
#include "Voxel/Rendering/VoxelCoalescedSubmissionMailbox.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"
#include "Voxel/VoxelModule.h"
#include "VoxelPersistentRootMerge.h"

namespace
{
	constexpr int32 VoxelNativeRootPreparationConcurrency = 8;
	constexpr int32 VoxelNativePublicationCapacity = 512;
	constexpr int32 VoxelNativeNearPublicationReserve = 64;

	template<typename ObjectType>
	struct TVoxelRenderRootDeleteTask
	{
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> TaskLease;
		ObjectType* Object = nullptr;
		void operator()() const { delete Object; }
	};

	template<typename ObjectType>
	struct TVoxelRenderRootDeleter
	{
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
		void operator()(ObjectType* Object) const
		{
			if (IsInGameThread() || IsInRenderingThread())
			{
				UE::Tasks::Launch(UE_SOURCE_LOCATION, TVoxelRenderRootDeleteTask<ObjectType>{Object->InputLease, Object});
			}
			else delete Object;
		}
	};

	template<typename ObjectType>
	uint64 RenderRootAllocationBytes()
	{
		using FController = SharedPointerInternals::TReferenceControllerWithDeleter<ObjectType,
			TVoxelRenderRootDeleter<ObjectType>, ESPMode::ThreadSafe>;
		return FMemory::QuantizeSize(sizeof(ObjectType), alignof(ObjectType)) +
			FMemory::QuantizeSize(sizeof(FController), alignof(FController)) +
			VoxelBoundedAllocation::TaskBytes<TVoxelRenderRootDeleteTask<ObjectType>>();
	}

	template<typename ObjectType, typename... ArgumentTypes>
	TSharedPtr<ObjectType, ESPMode::ThreadSafe> CreateRenderRoot(
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InLease, ArgumentTypes&&... InArguments)
	{
		auto* Object = new ObjectType(Forward<ArgumentTypes>(InArguments)...);
		Object->InputLease = InLease;
		return TSharedPtr<ObjectType, ESPMode::ThreadSafe>(Object, TVoxelRenderRootDeleter<ObjectType>{MoveTemp(InLease)});
	}
}

class FVoxelTerrainRenderChunk
{
public:
	explicit FVoxelTerrainRenderChunk(const ERHIFeatureLevel::Type InFeatureLevel)
		: VertexFactory(InFeatureLevel, "VoxelTerrainAtom"), FeatureLevel(InFeatureLevel)
	{
	}

	void ReleaseRenderResources()
	{
		check(IsInRenderingThread());
		VertexBuffers.PositionVertexBuffer.ReleaseResource();
		VertexBuffers.StaticMeshVertexBuffer.ReleaseResource();
		VertexBuffers.ColorVertexBuffer.ReleaseResource();
		IndexBuffer.ReleaseResource();
		VertexFactory.ReleaseResource();
	}

	FStaticMeshVertexBuffers VertexBuffers;
	FDynamicMeshIndexBuffer32 IndexBuffer;
	FLocalVertexFactory VertexFactory;
	const ERHIFeatureLevel::Type FeatureLevel;
	FMaterialRenderProxy* Material = nullptr;
	FMaterialRelevance Relevance;
	FBox WorldBounds = FBox(ForceInit);
	FBox LocalBounds = FBox(ForceInit);
	FMatrix LocalToWorld = FMatrix::Identity;
	uint32 PrimitiveUniformHash = 0;
	TSharedPtr<const FVoxelSectionMeshResult, ESPMode::ThreadSafe> Source;
	TSharedPtr<const FVoxelTerrainRenderChunk, ESPMode::ThreadSafe> NextSourceChunk;
	int32 FirstSourceTriangle = 0;
	int32 SourceBatch = INDEX_NONE;
	int32 SourceTriangleCount = 0;
	FVector3f FirstVertex = FVector3f::ZeroVector;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResidentLease;
	bool bGPUSceneStreams = false;
	TAtomic<bool> bReady{false};
	mutable TAtomic<bool> bResidentResourcesReady{false};
};

namespace
{
	// 同次收集共用 Proxy/Scene 参数，逐块覆盖的变换和边界必须完全一致。
	struct FVoxelTerrainUniformKeyFuncs : TDefaultMapKeyFuncs<const FVoxelTerrainRenderChunk*, FDynamicPrimitiveUniformBuffer*, false>
	{
		static bool Matches(const FVoxelTerrainRenderChunk* InFirst, const FVoxelTerrainRenderChunk* InSecond)
		{
			return InFirst == InSecond || (InFirst->PrimitiveUniformHash == InSecond->PrimitiveUniformHash &&
				InFirst->WorldBounds == InSecond->WorldBounds && InFirst->LocalBounds == InSecond->LocalBounds &&
				InFirst->LocalToWorld == InSecond->LocalToWorld);
		}

		static uint32 GetKeyHash(const FVoxelTerrainRenderChunk* InChunk)
		{
			return InChunk->PrimitiveUniformHash;
		}
	};

	struct FVoxelReleaseRenderChunkCommand
	{
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
		FVoxelTerrainRenderChunk* Chunk = nullptr;
		void operator()(FRHICommandListImmediate&) const
		{
			Chunk->ReleaseRenderResources();
			delete Chunk;
		}
	};

	struct FVoxelRenderChunkDeleter
	{
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
		void operator()(FVoxelTerrainRenderChunk* Chunk) const
		{
			ENQUEUE_RENDER_COMMAND(VoxelReleaseAtomChunk)(FVoxelReleaseRenderChunkCommand{Lease, Chunk});
		}
	};

	struct FVoxelPrepareRenderChunkCommand
	{
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> WorkingLease;
		TSharedPtr<FVoxelTerrainRenderChunk, ESPMode::ThreadSafe> Chunk;
		TSharedPtr<const FVoxelSectionMeshResult, ESPMode::ThreadSafe> Mesh;
		double WorldScale = 1.0;
		int32 Batch = 0;
		int32 FirstTriangle = 0;
		int32 TriangleCount = 0;
		int32 VertexCount = 0;

		void operator()(FRHICommandListImmediate& RHICmdList) const;
	};

	struct FVoxelRenderChunkCapacity
	{
		template<typename ElementType>
		static uint64 VertexStorageBytes(const int32 Count)
		{
			TMemoryImageAllocator<VERTEXBUFFER_ALIGNMENT>::ForElementType<ElementType> Allocator;
			const int32 Capacity = Allocator.CalculateSlackReserve(Count, sizeof(ElementType), alignof(ElementType));
			return FMemory::QuantizeSize(sizeof(TStaticMeshVertexData<ElementType>), alignof(TStaticMeshVertexData<ElementType>)) +
				uint64(Capacity) * sizeof(ElementType);
		}

		static uint64 FinalResidentBytes(const int32 VertexCount, const int32 IndexCount)
		{
			using FTangent = TStaticMeshVertexTangentDatum<FPackedRGBA16N>;
			using FTexCoord = TStaticMeshVertexUVsDatum<FVector2f>;
			using FController = SharedPointerInternals::TReferenceControllerWithDeleter<FVoxelTerrainRenderChunk,
				FVoxelRenderChunkDeleter, ESPMode::ThreadSafe>;
			return FMemory::QuantizeSize(sizeof(FVoxelTerrainRenderChunk), alignof(FVoxelTerrainRenderChunk)) +
				FMemory::QuantizeSize(sizeof(FController), alignof(FController)) +
				VoxelBoundedAllocation::FunctionBytes<FVoxelReleaseRenderChunkCommand>() +
				uint64(VertexCount) * (sizeof(FPositionVertex) + sizeof(FColor) + sizeof(FTangent) + 3 * sizeof(FTexCoord)) +
				uint64(IndexCount) * sizeof(uint32);
		}

		static FVoxelRenderChunkCapacity Calculate(const int32 VertexCount)
		{
			using FTangent = TStaticMeshVertexTangentDatum<FPackedRGBA16N>;
			using FTexCoord = TStaticMeshVertexUVsDatum<FVector2f>;
			FVoxelRenderChunkCapacity Capacity;
			// 授额覆盖上传时的 CPU/GPU 峰值；上传消费数据后按实际索引顶点结算。
			Capacity.Resident = FinalResidentBytes(VertexCount, VertexCount) +
				VertexStorageBytes<FPositionVertex>(VertexCount) + VertexStorageBytes<FColor>(VertexCount) +
				VertexStorageBytes<FTangent>(VertexCount) + VertexStorageBytes<FTexCoord>(VertexCount * 3) +
				VoxelBoundedAllocation::ArrayBytes<uint32>(VertexCount);
			Capacity.Working = VoxelBoundedAllocation::ArrayBytes<FDynamicMeshVertex>(VertexCount) +
				VoxelBoundedAllocation::ArrayBytes<FIntPoint>(FMath::RoundUpToPowerOfTwo(uint32(VertexCount) * 2)) +
				VoxelBoundedAllocation::FunctionBytes<FVoxelPrepareRenderChunkCommand>();
			return Capacity;
		}

		uint64 Resident = 0;
		uint64 Working = 0;
	};

	void FVoxelPrepareRenderChunkCommand::operator()(FRHICommandListImmediate& RHICmdList) const
	{
		const FVoxelMeshBuffers& Input = Mesh->Batches[Batch].Mesh;
		TArray<FDynamicMeshVertex> Vertices;
		Vertices.Reserve(VertexCount);
		TArray<FIntPoint> Remap;
		const int32 RemapSlots = int32(FMath::RoundUpToPowerOfTwo(uint32(VertexCount) * 2));
		Remap.Reserve(RemapSlots);
		Remap.Init(FIntPoint(INDEX_NONE, INDEX_NONE), RemapSlots);
		Chunk->IndexBuffer.Indices.Reserve(VertexCount);
		for (int32 Corner = FirstTriangle * 3; Corner < (FirstTriangle + TriangleCount) * 3; ++Corner)
		{
			const int32 Index = Input.Triangles[Corner];
			uint32 Slot = GetTypeHash(Index) & uint32(Remap.Num() - 1);
			while (Remap[Slot].X != INDEX_NONE && Remap[Slot].X != Index) Slot = (Slot + 1) & uint32(Remap.Num() - 1);
			if (Remap[Slot].X == INDEX_NONE)
			{
				FDynamicMeshVertex Vertex;
				Vertex.Position = FVector3f(Input.Vertices[Index] * WorldScale);
				Vertex.Color = Input.Colors[Index].ToFColor(false);
				Vertex.TextureCoordinate[0] = FVector2f(Input.UV0[Index]);
				Vertex.TextureCoordinate[1] = FVector2f(Input.UV1[Index]);
				Vertex.TextureCoordinate[2] = FVector2f(Input.UV2[Index]);
				const FVector3f TangentX(Input.Tangents[Index].TangentX);
				const FVector3f Normal(Input.Normals[Index]);
				const FVector3f TangentY = FVector3f::CrossProduct(Normal, TangentX) *
					(Input.Tangents[Index].bFlipTangentY ? -1.0f : 1.0f);
				Vertex.SetTangents(TangentX, TangentY, Normal);
				Remap[Slot] = FIntPoint(Index, Vertices.Add(Vertex));
			}
			Chunk->IndexBuffer.Indices.Add(uint32(Remap[Slot].Y));
		}
		Chunk->FirstVertex = Vertices[0].Position;
		Chunk->VertexBuffers.StaticMeshVertexBuffer.SetUseFullPrecisionUVs(true);
		Chunk->VertexBuffers.StaticMeshVertexBuffer.SetUseHighPrecisionTangentBasis(true);
		Chunk->VertexBuffers.InitFromDynamicVertex(RHICmdList, &Chunk->VertexFactory, Vertices, 3);
		Chunk->IndexBuffer.InitResource(RHICmdList);
		const auto FeatureLevel = Chunk->FeatureLevel;
		Chunk->bGPUSceneStreams =
			Chunk->VertexFactory.GetPrimitiveIdStreamIndex(FeatureLevel, EVertexInputStreamType::Default) >= 0 &&
			Chunk->VertexFactory.GetPrimitiveIdStreamIndex(FeatureLevel, EVertexInputStreamType::PositionOnly) >= 0 &&
			Chunk->VertexFactory.GetPrimitiveIdStreamIndex(FeatureLevel, EVertexInputStreamType::PositionAndNormalOnly) >= 0;
		// CreateBuffer 已消费上传数组；重建和接口裁切始终使用冻结的 Source，不读取上传副本。
		Chunk->VertexBuffers.PositionVertexBuffer.CleanUp();
		Chunk->VertexBuffers.StaticMeshVertexBuffer.CleanUp();
		Chunk->VertexBuffers.ColorVertexBuffer.CleanUp();
		Chunk->IndexBuffer.Indices.Empty();
		verify(Chunk->ResidentLease->TryResize(FVoxelRenderChunkCapacity::FinalResidentBytes(Vertices.Num(), VertexCount)) ==
			EVoxelTerrainResourceAdmission::Granted);
		Chunk->bReady.Store(true);
	}

	struct FAtomTreeNode;
	using FAtomNodePtr = TSharedPtr<const FAtomTreeNode, ESPMode::ThreadSafe>;
	using FAtomPtr = TSharedPtr<const FVoxelTerrainRenderAtom, ESPMode::ThreadSafe>;

	bool RendererHasExternalRelease(const TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe>& Resources,
		const FVoxelGenerationConsumerKey& Consumer, const uint64 Version)
	{
		uint64 Current = 0;
		return Resources->TryGetReleaseVersionExcludingOwner(Consumer, Current) && Current > Version;
	}

	bool RendererIsTerminalAdmission(const EVoxelTaskBlockReason Reason)
	{
		return (static_cast<uint16>(Reason) & (static_cast<uint16>(EVoxelTaskBlockReason::Oversized) |
			static_cast<uint16>(EVoxelTaskBlockReason::TerminalFailure))) != 0;
	}

	// 只声明直接强持有且属于同一资源域的实际租约；相同 handle 不重复计入。
	struct FRendererBindingInputFloor
	{
		bool Add(const TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& Lease,
			const FVoxelTerrainResources& Resources)
		{
			if (!Lease || !Lease->IsOwnedBy(Resources)) return true;
			for (int32 Index = 0; Index < LeaseCount; ++Index) if (Leases[Index]->Serial() == Lease->Serial()) return true;
			if (LeaseCount == UE_ARRAY_COUNT(Leases)) return false;
			const uint64 InputBytes = Lease->GetPool() == EVoxelTerrainResourcePool::Input ? Lease->Bytes() : 0;
			if (InputBytes > MAX_uint64 - Bytes) return false;
			Bytes += InputBytes;
			Leases[LeaseCount++] = Lease.Get();
			return true;
		}

		uint64 Bytes = 0;
		int32 LeaseCount = 0;
		// 八个准备事务及其直接租约，另加 State、当前原生 Worker 和待授予来源。
		const FVoxelTerrainResourceLease* Leases[VoxelNativeRootPreparationConcurrency * 5 + 4] = {};
	};

	EVoxelTerrainResourceAdmission TryAcquirePublicationResident(
		const TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe>& Resources,
		const FVoxelGenerationConsumerKey& Consumer, const uint64 Bytes,
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& Lease, const int32 Splits,
		const bool bAllowOverlap, const uint64 RequiredInputBytes = 0, const int32 RequiredLeaseCount = 0)
	{
		const auto Primary = Resources->TryAcquire(Consumer, EVoxelTerrainResourcePool::Resident,
			Bytes, Lease, Splits, RequiredInputBytes, RequiredLeaseCount);
		if (Primary == EVoxelTerrainResourceAdmission::Granted || !bAllowOverlap) return Primary;
		const auto Overlap = Resources->TryAcquire(Consumer, EVoxelTerrainResourcePool::ResidentOverlap,
			Bytes, Lease, Splits, RequiredInputBytes, RequiredLeaseCount);
		if (Overlap == EVoxelTerrainResourceAdmission::Granted) return Overlap;
		return Primary == EVoxelTerrainResourceAdmission::CapacityBlocked || Overlap == EVoxelTerrainResourceAdmission::CapacityBlocked
			? EVoxelTerrainResourceAdmission::CapacityBlocked : Overlap;
	}

	bool TransferPublicationLease(const TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& Lease,
		const bool bAllowOverlap)
	{
		return Lease && (Lease->Transfer(EVoxelTerrainResourcePool::Resident) ||
			(bAllowOverlap && Lease->Transfer(EVoxelTerrainResourcePool::ResidentOverlap)));
	}

	EVoxelTerrainResourceAdmission TryTransferPublicationLease(const FVoxelTerrainResources& Resources,
		const TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& Lease, const bool bAllowOverlap)
	{
		if (!Lease || !Lease->IsOwnedBy(Resources)) return EVoxelTerrainResourceAdmission::InvalidRequest;
		if (TransferPublicationLease(Lease, bAllowOverlap)) return EVoxelTerrainResourceAdmission::Granted;
		const auto Limits = Resources.Snapshot().Limits;
		return Lease->Bytes() > Limits.ResidentBytes - Limits.OverlapBytes && (!bAllowOverlap || Lease->Bytes() > Limits.OverlapBytes)
			? EVoxelTerrainResourceAdmission::CapacityUnachievable : EVoxelTerrainResourceAdmission::CapacityBlocked;
	}

	// 符号位翻转后的三维 radix 树。修改只复制 Atom 所在路径，遍历按包围盒裁剪。
	struct FAtomTreeNode
	{
		FAtomNodePtr Children[8];
		FAtomPtr Atom;
		FBox Bounds = FBox(ForceInit);
		FMaterialRelevance MaterialRelevance;
		int32 FineAtomCount = 0;
		int32 OwnerAtomCounts[6] = {};
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> PathLease;
	};

	uint8 Branch(const FIntVector& InSection, const int32 InBit)
	{
		return (((uint32(InSection.X) ^ 0x80000000u) >> InBit) & 1u) |
			((((uint32(InSection.Y) ^ 0x80000000u) >> InBit) & 1u) << 1) |
			((((uint32(InSection.Z) ^ 0x80000000u) >> InBit) & 1u) << 2);
	}

	FAtomNodePtr SetAtom(const FAtomNodePtr& InBase, const FAtomPtr& InAtom, const int32 InBit,
		const TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& InPathLease, const double InCellSize)
	{
		if (InBit < 0 && InAtom->Evidence.Owner.Kind == EVoxelFineOwnerKind::None) return nullptr;
		const uint64 NodeBytes = VoxelBoundedAllocation::WorkerSharedBytes<FAtomTreeNode>();
		auto NodeLease = InPathLease->Split(NodeBytes);
		check(NodeLease);
		auto Node = VoxelBoundedAllocation::MakeWorkerShared<FAtomTreeNode>(NodeLease);
		if (InBase)
		{
			for (int32 Index = 0; Index < 8; ++Index) Node->Children[Index] = InBase->Children[Index];
			Node->Atom = InBase->Atom;
		}
		Node->PathLease = MoveTemp(NodeLease);
		if (InBit < 0)
		{
			Node->Atom = InAtom;
			const FVector Min = FVector(InAtom->Section) * (16.0 * InCellSize);
			Node->Bounds = FBox(Min, Min + FVector(16.0 * InCellSize));
			Node->FineAtomCount = InAtom->Evidence.Owner.Kind == EVoxelFineOwnerKind::F0 ||
				InAtom->Evidence.Owner.Kind == EVoxelFineOwnerKind::F1Plus ? 1 : 0;
			Node->OwnerAtomCounts[uint8(InAtom->Evidence.Owner.Kind)] = 1;
			for (const auto& Chunk : InAtom->Chunks) if (Chunk) Node->MaterialRelevance |= Chunk->Relevance;
			for (const auto& Range : InAtom->DrawRanges) if (Range.Chunk) Node->MaterialRelevance |= Range.Chunk->Relevance;
		}
		else
		{
			const uint8 Index = Branch(InAtom->Section, InBit);
			Node->Children[Index] = SetAtom(Node->Children[Index], InAtom, InBit - 1, InPathLease, InCellSize);
			for (const FAtomNodePtr& Child : Node->Children)
			{
				if (!Child) continue;
				if (Child->Bounds.IsValid) Node->Bounds += Child->Bounds;
				Node->FineAtomCount += Child->FineAtomCount;
				for (int32 Kind = 0; Kind < UE_ARRAY_COUNT(Node->OwnerAtomCounts); ++Kind)
					Node->OwnerAtomCounts[Kind] += Child->OwnerAtomCounts[Kind];
				Node->MaterialRelevance |= Child->MaterialRelevance;
			}
		}
		if (!Node->Atom && !Node->Bounds.IsValid) return nullptr;
		return Node;
	}

	struct FCoarseTreeNode;
	using FCoarseNodePtr = TSharedPtr<const FCoarseTreeNode, ESPMode::ThreadSafe>;
	using FCoarsePtr = TSharedPtr<const FVoxelTerrainRenderCoarse, ESPMode::ThreadSafe>;
	struct FCoarseTreeNode
	{
		FCoarseNodePtr Children[8];
		FCoarsePtr Item;
		FBox CellBounds = FBox(ForceInit);
		FMaterialRelevance MaterialRelevance;
		int32 OwnerCounts[6] = {};
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	};

	uint8 CoarseBranch(const FVoxelTerrainSpatialKey& Key, const FGuid& SourceId, const int32 Bit)
	{
		// 先按完整 3D 世界 Atom 坐标分支；随机 SourceId 只能放在空间叶子内部。
		// GUID 前缀会让每个高层节点横跨全世界，局部查询退化成遍历所有粗表示。
		if (Bit >= 46)
		{
			FIntVector WorldAtom;
			for (int32 Axis = 0; Axis < 3; ++Axis)
			{
				const int64 Coordinate = int64(Key.Coordinate[Axis]) * (1ll << Key.Level);
				check(Coordinate >= MIN_int32 && Coordinate <= MAX_int32);
				WorldAtom[Axis] = int32(Coordinate);
			}
			return Branch(WorldAtom, Bit - 46);
		}
		if (Bit == 45) return uint8(Key.Representation);
		if (Bit == 44) return Key.Level >> 3;
		if (Bit == 43) return Key.Level & 7;
		{
			const uint32 Words[4] = {SourceId.A, SourceId.B, SourceId.C, SourceId.D};
			uint8 Out = 0;
			for (int32 Index = 0; Index < 3; ++Index)
			{
				const int32 SourceBit = Bit * 3 + Index;
				if (SourceBit < 128) Out |= ((Words[SourceBit / 32] >> (SourceBit % 32)) & 1u) << Index;
			}
			return Out;
		}
	}

	FCoarseNodePtr SetCoarse(const FCoarseNodePtr& Base, const FVoxelTerrainSpatialKey& Key, const FGuid& SourceId,
		const FCoarsePtr& Item, const int32 Bit,
		const TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& PathLease)
	{
		if (Bit < 0 && !Item) return nullptr;
		auto Lease = PathLease->Split(VoxelBoundedAllocation::WorkerSharedBytes<FCoarseTreeNode>());
		check(Lease);
		auto Node = VoxelBoundedAllocation::MakeWorkerShared<FCoarseTreeNode>(Lease);
		Node->Lease = MoveTemp(Lease);
		if (Base)
		{
			for (int32 Child = 0; Child < 8; ++Child) Node->Children[Child] = Base->Children[Child];
			Node->Item = Base->Item;
		}
		if (Bit < 0)
		{
			Node->Item = Item;
			Node->CellBounds = Item->OwnershipBounds;
			if (!Item->bBoundaryOnly) Node->OwnerCounts[uint8(Item->Evidence.Owner.Kind)] = 1;
			for (const auto& Chunk : Item->SourceChunks) if (Chunk) Node->MaterialRelevance |= Chunk->Relevance;
		}
		else
		{
			const uint8 Child = CoarseBranch(Key, SourceId, Bit);
			Node->Children[Child] = SetCoarse(Node->Children[Child], Key, SourceId, Item, Bit - 1, PathLease);
			for (const auto& Other : Node->Children)
			{
				if (!Other) continue;
				Node->CellBounds += Other->CellBounds;
				Node->MaterialRelevance |= Other->MaterialRelevance;
				for (int32 Kind = 0; Kind < UE_ARRAY_COUNT(Node->OwnerCounts); ++Kind) Node->OwnerCounts[Kind] += Other->OwnerCounts[Kind];
			}
		}
		return Node->CellBounds.IsValid ? Node : nullptr;
	}

	bool FinerCoarse(const FVoxelTerrainGeometry& A, const FVoxelTerrainGeometry& B)
	{
		const uint8 ARank = uint8(A.Key.Representation);
		const uint8 BRank = uint8(B.Key.Representation);
		return ARank != BRank ? ARank < BRank : A.Key.Level < B.Key.Level;
	}

	const FVoxelFineOwnerEvidence* FindCoarseEvidence(const FCoarseNodePtr& Root, const FIntVector& Section)
	{
		const FVector Min = FVector(Section) * 16.0;
		const FBox Box(Min, Min + FVector(16.0));
		const FVoxelTerrainRenderCoarse* Best = nullptr;
		const auto Visit = [&](auto&& Self, const FCoarseNodePtr& Node) -> void
		{
			if (!Node || !Node->CellBounds.Intersect(Box)) return;
			if (Node->Item)
			{
				const auto& Item = *Node->Item;
				if (!Item.bBoundaryOnly && Item.OwnershipBounds.IsInsideOrOn(Box.Min) && Item.OwnershipBounds.IsInsideOrOn(Box.Max) &&
					(!Best || FinerCoarse(Item.Geometry->Stamp.GeometryIdentity, Best->Geometry->Stamp.GeometryIdentity))) Best = &Item;
			}
			else for (const auto& Child : Node->Children) Self(Self, Child);
		};
		Visit(Visit, Root);
		return Best ? &Best->Evidence : nullptr;
	}

	FCoarsePtr FindCoarseItem(const FCoarseNodePtr& Root, const FVoxelTerrainSpatialKey& Key, const FGuid& SourceId)
	{
		const FCoarseTreeNode* Node = Root.Get();
		for (int32 Bit = 77; Bit >= 0 && Node; --Bit) Node = Node->Children[CoarseBranch(Key, SourceId, Bit)].Get();
		return Node ? Node->Item : nullptr;
	}

	bool SameCoarseGeometry(const FCoarsePtr& Current, const FCoarsePtr& Expected)
	{
		return Current && Expected && Current->SourceId == Expected->SourceId &&
			Current->Geometry->GeometryIdentity == Expected->Geometry->GeometryIdentity &&
			Current->Geometry->Stamp == Expected->Geometry->Stamp;
	}

	struct FPresentedSnapshot final : IVoxelTerrainPresentedSnapshot
	{
		FAtomNodePtr Root;
		FCoarseNodePtr CoarseRoot;
		uint64 WorldEpoch = 0;
		uint64 Serial = 0;
		uint64 PresentationSerial = 0;
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
		virtual uint64 GetWorldEpoch() const override { return WorldEpoch; }
		virtual uint64 GetSerial() const override { return Serial; }
		virtual int32 GetFineAtomCount() const override { return Root ? Root->FineAtomCount : 0; }
		virtual int32 GetOwnerAtomCount(const EVoxelFineOwnerKind InKind) const override
		{
			return Root && uint8(InKind) < UE_ARRAY_COUNT(Root->OwnerAtomCounts) ? Root->OwnerAtomCounts[uint8(InKind)] : 0;
		}
		virtual int32 GetCoarseOwnerCount(const EVoxelFineOwnerKind InKind) const override
		{
			return CoarseRoot && uint8(InKind) < UE_ARRAY_COUNT(CoarseRoot->OwnerCounts) ? CoarseRoot->OwnerCounts[uint8(InKind)] : 0;
		}
		virtual const FVoxelFineOwnerEvidence* Find(const FIntVector& InSection) const override
		{
			const FAtomTreeNode* Node = Root.Get();
			for (int32 Bit = 31; Bit >= 0 && Node; --Bit) Node = Node->Children[Branch(InSection, Bit)].Get();
			return Node && Node->Atom ? &Node->Atom->Evidence : FindCoarseEvidence(CoarseRoot, InSection);
		}
	};

	struct FAtomPublication final : FVoxelTaskCustomPayload
	{
		mutable TSharedPtr<FVoxelTerrainWorkReceipt, ESPMode::ThreadSafe> PreparationReceipt;
		mutable TSharedPtr<FVoxelTerrainWorkReceipt, ESPMode::ThreadSafe> MergeReceipt;
		TArray<FAtomPtr> Atoms;
		TArray<FVoxelTerrainCoarseMutation> CoarseChanges;
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> CoarseCallbackLease;
		FGuid Transaction;
		FVoxelFinePublishStamp Stamp;
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
		mutable TAtomic<bool> bReceiptSettled{false};
		mutable uint64 ReceiptOwnerFrameSerial = 0;
		mutable int32 ReceiptAtomCursor = 0;
		mutable int32 ReceiptCoarseCursor = 0;
		mutable int32 PendingIssuedIndex = INDEX_NONE;
		mutable bool bReceiptOwnershipCommitted = false;
		FBox GetChangedBounds() const
		{
			FBox Bounds(ForceInit);
			for (const auto& Atom : Atoms)
			{
				const FVector Min = FVector(Atom->Section) * 16.0;
				Bounds += FBox(Min, Min + FVector(16.0));
			}
			for (const auto& Change : CoarseChanges) Bounds += Change.Item->OwnershipBounds;
			return Bounds;
		}
		bool AffectsFineSection(const FIntVector& Section) const
		{
			for (const auto& Atom : Atoms)
			{
				if (FMath::Abs(int64(Atom->Section.X) - Section.X) + FMath::Abs(int64(Atom->Section.Y) - Section.Y) +
					FMath::Abs(int64(Atom->Section.Z) - Section.Z) <= 1) return true;
			}
			const auto Overlaps = [](const FBox& A, const FBox& B)
			{
				return A.Min.X < B.Max.X && B.Min.X < A.Max.X && A.Min.Y < B.Max.Y && B.Min.Y < A.Max.Y &&
					A.Min.Z < B.Max.Z && B.Min.Z < A.Max.Z;
			};
			for (const auto& Change : CoarseChanges)
				for (int32 Neighbor = -1; Neighbor < 6; ++Neighbor)
				{
					const FVector Min = FVector(Section + (Neighbor < 0 ? FIntVector::ZeroValue : VoxelCoord::Direction(Neighbor))) * 16.0;
					if (Overlaps(Change.Item->OwnershipBounds, FBox(Min, Min + FVector(16.0)))) return true;
				}
			return false;
		}
		bool ConflictsWith(const FAtomPublication& Other) const
		{
			if (!CoarseChanges.IsEmpty() || !Other.CoarseChanges.IsEmpty())
				return GetChangedBounds().ExpandBy(32.0).Intersect(Other.GetChangedBounds());
			// 事务重建自身与六个面邻居；两步内的另一写入会改变读输入或重叠合并叶子。
			for (const auto& Atom : Atoms)
				for (const auto& OtherAtom : Other.Atoms)
				{
					if (FMath::Abs(int64(Atom->Section.X) - OtherAtom->Section.X) +
						FMath::Abs(int64(Atom->Section.Y) - OtherAtom->Section.Y) +
						FMath::Abs(int64(Atom->Section.Z) - OtherAtom->Section.Z) <= 2) return true;
				}
			return false;
		}
		bool SharesCoarseOwner(const FAtomPublication& Other, const FCoarseNodePtr& CoarseRoot) const
		{
			const FBox FirstBounds = GetChangedBounds().ExpandBy(32.0);
			const FBox OtherBounds = Other.GetChangedBounds().ExpandBy(32.0);
			const auto Visit = [&](auto&& Self, const FCoarseTreeNode* Node) -> bool
			{
				if (!Node || !Node->CellBounds.Intersect(FirstBounds) || !Node->CellBounds.Intersect(OtherBounds)) return false;
				// 相隔的 Fine 或 Coarse 写入仍可能重建同一粗表示的 Patch / Draw 叶子。
				// 共享粗来源必须串行；不同粗来源的局部根仍可并行准备。
				if (Node->Item) return true;
				for (const auto& Child : Node->Children) if (Self(Self, Child.Get())) return true;
				return false;
			};
			return Visit(Visit, CoarseRoot.Get());
		}
		virtual uint64 GetAllocatedBytes() const override
		{
			return VoxelBoundedAllocation::WorkerSharedBytes<FAtomPublication>() + Atoms.GetAllocatedSize() + CoarseChanges.GetAllocatedSize();
		}
	};

	struct FFrozenPublication
	{
		TSharedPtr<const FVoxelFineFrozenHandoff, ESPMode::ThreadSafe> Handoff;
		TSharedPtr<const FAtomPublication, ESPMode::ThreadSafe> Resources;
	};

	struct FRenderSnapshot;
	using FSnapshotPtr = TSharedPtr<const FRenderSnapshot, ESPMode::ThreadSafe>;

	struct FFrameSubmissionData
	{
		TStaticArray<FGPUFenceRHIRef, MAX_NUM_GPUS> GpuFences;
		uint32 GpuMask = 0;
		bool bHasReceipts = false;
	};

	struct FRetiringFrame
	{
		FSnapshotPtr Snapshot;
		TStaticArray<FGPUFenceRHIRef, MAX_NUM_GPUS> GpuFences;
		FRenderCommandFence RenderFence;
		TArray<FVoxelFinePresentedAtom> PreviousAtoms;
		TArray<FVoxelFineRenderReceiptAtom> ReceiptScratch;
		TArray<FGuid> SettledTransactions;
		bool bReceiptsSettled = false;
		bool bHasReceipts = false;
		int32 ReceiptCursor = 0;
		int32 ForgetCursor = 0;
		uint32 SubmittedGpuMask = 0;

		void IncludeGpuSubmission(const FFrameSubmissionData& InSubmission)
		{
			SubmittedGpuMask |= InSubmission.GpuMask;
			for (int32 Gpu = 0; Gpu < MAX_NUM_GPUS; ++Gpu)
				if ((InSubmission.GpuMask & (1u << Gpu)) != 0) GpuFences[Gpu] = InSubmission.GpuFences[Gpu];
		}

		bool AreGpuFencesComplete() const
		{
			if (!SubmittedGpuMask) return false;
			for (int32 Gpu = 0; Gpu < MAX_NUM_GPUS; ++Gpu)
				if ((SubmittedGpuMask & (1u << Gpu)) != 0 &&
					(!GpuFences[Gpu].IsValid() || !GpuFences[Gpu]->Poll(FRHIGPUMask::FromIndex(Gpu)))) return false;
			return true;
		}
	};

	struct FRenderSnapshot
	{
		uint64 WorldEpoch = 0;
		FGuid RendererOwner;
		uint64 Serial = 0;
		uint64 FrameCounter = 0;
		FAtomNodePtr Root;
		FCoarseNodePtr CoarseRoot;
		TArray<FFrozenPublication> Issued;
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
		mutable TAtomic<bool> bReceiptProduced{false};
		mutable TVoxelCoalescedSubmissionNode<FRenderSnapshot, FFrameSubmissionData> SubmissionSlot;
		mutable FRetiringFrame Retirement;
		mutable const FRenderSnapshot* RetirementPrevious = nullptr;
		mutable const FRenderSnapshot* RetirementNext = nullptr;
		mutable bool bRetirementRegistered = false;
	};

	struct FFrameBinding : ISceneViewFamilyExtentionData
	{
		static const TCHAR* GSubclassIdentifier;
		virtual const TCHAR* GetSubclassIdentifier() const override { return GSubclassIdentifier; }
		FSnapshotPtr Snapshot;
		mutable TAtomic<bool> bProxySubmitted{false};
	};
	const TCHAR* FFrameBinding::GSubclassIdentifier = TEXT("WHFramework.Voxel.FrameBinding");


	struct FRootPreparation;
	struct FRootMerge;
	struct FReadyTransaction
	{
		FGuid Id;
		FVoxelTerrainServicePriority Priority;
		double FirstQueuedSeconds = 0.0;
		uint64 Sequence = 0;
		uint64 PriorityPass = 0;
		TSharedPtr<FAtomPublication, ESPMode::ThreadSafe> Resources;
		TFunction<bool(const FIntVector&, const FVoxelFineOwnerEvidence&)> IsCurrent;
		TFunction<bool(const FIntVector&, const FVoxelFineOwnerEvidence&)> IsCollisionCurrent;
		TFunction<void()> OnSubmitted;
		TFunction<bool()> IsCoarseCurrent;
		TSharedPtr<struct FRootPreparation, ESPMode::ThreadSafe> Preparation;
		TSharedPtr<FRootMerge, ESPMode::ThreadSafe> SuspendedMerge;
		FVoxelGenerationConsumerKey PreparationConsumer;
		FVoxelGenerationConsumerKey MergeConsumer;
		FVoxelGenerationConsumerKey SealConsumer;
		bool bTerminalFailure = false;
		bool bPreparationAdmissionWaiting = false;
		uint64 PreparationAdmissionVersion = 0;
		bool bMergeCapacityBlocked = false;
		uint64 MergeCapacityVersion = 0;
		bool bSealCapacityBlocked = false;
		uint64 SealCapacityVersion = 0;
	};
	bool HigherVisibleTransaction(const FReadyTransaction& First, const FReadyTransaction& Second)
	{
		const bool bFirstSafety = FVoxelTerrainWorkSelection::IsNear(First.Priority.Band);
		const bool bSecondSafety = FVoxelTerrainWorkSelection::IsNear(Second.Priority.Band);
		if (bFirstSafety != bSecondSafety) return bFirstSafety;
		if (bFirstSafety && First.Priority.Band != Second.Priority.Band)
			return uint8(First.Priority.Band) < uint8(Second.Priority.Band);
		if (First.Priority.DistanceCells != Second.Priority.DistanceCells)
			return First.Priority.DistanceCells < Second.Priority.DistanceCells;
		if (First.Priority.SourcePriority != Second.Priority.SourcePriority)
			return First.Priority.SourcePriority < Second.Priority.SourcePriority;
		if (First.Priority.ForwardScore != Second.Priority.ForwardScore)
			return First.Priority.ForwardScore > Second.Priority.ForwardScore;
		return First.Sequence < Second.Sequence;
	}

	struct FCollectorFrameLease
	{
		FSnapshotPtr Snapshot;
	};
	struct FRetirementWalkFrame
	{
		FAtomNodePtr Node;
		int32 Child = 0;
	};
	struct FRetirementProof final : FVoxelTaskCustomPayload
	{
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	};

	struct FCoarseWalk
	{
		FCoarseNodePtr Node;
		int32 Child = 0;
	};
	struct FFineWalk
	{
		FAtomNodePtr Node;
		int32 Child = 0;
	};
	struct FDrawWalk
	{
		TSharedPtr<const FVoxelTerrainCoarsePatchNode, ESPMode::ThreadSafe> Patch;
		TSharedPtr<const FVoxelTerrainCoarseDrawNode, ESPMode::ThreadSafe> Base;
		TSharedPtr<FVoxelTerrainCoarseDrawNode, ESPMode::ThreadSafe> Result;
		TSharedPtr<const FVoxelTerrainCoarsePatchPiece, ESPMode::ThreadSafe> Piece;
		TSharedPtr<FVoxelTerrainCoarseDrawPiece, ESPMode::ThreadSafe> LastPiece;
		int32 Child = 0;
	};
	// 上传结束后有界扫描真实 Draw chunk；不为每个裁切叶子单独等待一帧。
	struct FDrawReadinessWalk
	{
		TSharedPtr<const FVoxelTerrainCoarseDrawNode, ESPMode::ThreadSafe> Node;
		TSharedPtr<const FVoxelTerrainCoarseDrawPiece, ESPMode::ThreadSafe> Piece;
		int32 Child = -1;
	};
	struct FPreparedCoarse
	{
		FCoarsePtr Base;
		TSharedPtr<FVoxelTerrainRenderCoarse, ESPMode::ThreadSafe> Item;
		TSharedPtr<FPreparedCoarse, ESPMode::ThreadSafe> Next;
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> PathLease;
	};

#include "VoxelNativeHeightfieldInterface.inl"
#include "VoxelNativeFineInterface.inl"

	// 每份局部事务独立准备；提交时与最新根做局部三方合并。
	struct FRootPreparation
	{
		FRootPreparation(TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
			const FVoxelGenerationConsumerKey& InConsumer, const int32 InChangedBounds)
			: Resources(MoveTemp(InResources)), Consumer(InConsumer)
		{
			check(InChangedBounds > 0 && InChangedBounds <= 8200);
			ChangedBounds.Reserve(InChangedBounds);
		}

		bool ObserveAdmission(const EVoxelTerrainResourceAdmission InAdmission, FString& OutError)
		{
			NativeAdmission = InAdmission;
			if (InAdmission == EVoxelTerrainResourceAdmission::Granted) return true;
			OutError = InAdmission == EVoxelTerrainResourceAdmission::CapacityBlocked ? TEXT("CapacityBlocked: Native render preparation") :
				InAdmission == EVoxelTerrainResourceAdmission::CapacityUnachievable ? TEXT("CapacityUnachievable: Native render preparation") :
				TEXT("InvalidRequest: Native render preparation");
			return false;
		}

		bool AcquireNative(const EVoxelTerrainResourcePool Pool, const uint64 Bytes,
			TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& OutLease, FString& OutError)
		{
			FRendererBindingInputFloor Floor;
			if (!Floor.Add(ControlInputLease, Resources.Get()) || !Floor.Add(InputLease, Resources.Get()) || !Floor.Add(Publication->Lease, Resources.Get()) ||
				!Floor.Add(Publication->CoarseCallbackLease, Resources.Get()) ||
				!Floor.Add(PrimaryPathLease, Resources.Get()))
				return ObserveAdmission(EVoxelTerrainResourceAdmission::InvalidRequest, OutError);
			const bool bGranted = ObserveAdmission(Pool == EVoxelTerrainResourcePool::Resident ?
				TryAcquirePublicationResident(Resources, Consumer, Bytes, OutLease, 0, bAllowResidentOverlap, Floor.Bytes, Floor.LeaseCount) :
				Resources->TryAcquire(Consumer, Pool, Bytes, OutLease, 0, Floor.Bytes, Floor.LeaseCount), OutError);
			if (!bGranted)
				OutError += FString::Printf(TEXT(" pool=%d bytes=%llu retainedInput=%llu retainedLeases=%llu"),
					int32(Pool), Bytes, Floor.Bytes, uint64(Floor.LeaseCount));
			return bGranted;
		}

		bool AddExclusion(const FBox& Box, FString& OutError)
		{
			if (Exclusions->Boxes.Num() == Exclusions->Boxes.Max())
			{
				if (Exclusions->Boxes.Num() == MAX_int32)
					return ObserveAdmission(EVoxelTerrainResourceAdmission::CapacityUnachievable, OutError);
				const int32 Capacity = Exclusions->Boxes.Num() + 1;
				const uint64 BodyBytes = VoxelBoundedAllocation::WorkerSharedBytes<FVoxelTerrainCoarseExclusions>();
				FRendererBindingInputFloor Floor;
				if (!Floor.Add(ControlInputLease, Resources.Get()) || !Floor.Add(InputLease, Resources.Get()) || !Floor.Add(Publication->Lease, Resources.Get()) ||
					!Floor.Add(Publication->CoarseCallbackLease, Resources.Get()) ||
					!Floor.Add(PrimaryPathLease, Resources.Get()))
					return ObserveAdmission(EVoxelTerrainResourceAdmission::InvalidRequest, OutError);
				if (!ObserveAdmission(Exclusions->Lease->TryResize(BodyBytes + Exclusions->Boxes.GetAllocatedSize() +
					VoxelBoundedAllocation::ArrayBytes<FBox>(Capacity), Floor.Bytes, Floor.LeaseCount), OutError)) return false;
				Exclusions->Boxes.Reserve(Capacity);
				verify(Exclusions->Lease->Resize(BodyBytes + Exclusions->Boxes.GetAllocatedSize()));
			}
			Exclusions->Boxes.Add(Box);
			return true;
		}

		bool BeginItem(const FCoarsePtr& Item, FString& OutError)
		{
			TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
			if (!AcquireNative(EVoxelTerrainResourcePool::Input,
				VoxelBoundedAllocation::WorkerSharedBytes<FVoxelTerrainCoarseExclusions>(), Lease, OutError)) return false;
			Exclusions = VoxelBoundedAllocation::MakeWorkerShared<FVoxelTerrainCoarseExclusions>(Lease);
			Exclusions->Lease = MoveTemp(Lease);
			Current = Item;
			FineWalk.Reset();
			ExclusionWalk.Reset();
			if (Root) FineWalk.Add({Root, 0});
			if (CoarseRoot) ExclusionWalk.Add({CoarseRoot, 0});
			return true;
		}

		FVoxelTaskResult Advance(const TAtomic<bool>& Cancel)
		{
			FVoxelTaskResult Result;
			bExecuting.Store(true);
			ON_SCOPE_EXIT
			{
				DiagnosticPhase.Store(Phase);
				DiagnosticFineReused.Store(FineReused);
				DiagnosticFineSourceRangesReused.Store(FineSourceRangesReused);
				DiagnosticFinePrepared.Store(FinePrepared);
				DiagnosticFineCompleted.Store(FineCompleted);
				DiagnosticHeightfieldTiles.Store(HeightfieldTiles + (FineBuild ? FineBuild->HeightfieldTilesWithGeometry : 0));
				DiagnosticHeightfieldPieces.Store(HeightfieldPieces + (FineBuild ? FineBuild->HeightfieldPieces : 0));
				const FIntVector Section = FineBuild && FineBuild->Base ? FineBuild->Base->Section : FIntVector::ZeroValue;
				DiagnosticFineSectionX.Store(Section.X);
				DiagnosticFineSectionY.Store(Section.Y);
				DiagnosticFineSectionZ.Store(Section.Z);
				DiagnosticFineProgress.Store(FineBuild ? uint32(FineBuild->Phase * 4096 + FineBuild->FaceCursor * 256 + FineBuild->TileCursor) : MAX_uint32);
				bExecutionComplete.Store(!Result.bContinue);
				bExecuting.Store(false);
			};
			bWaitingCapacity.Store(false);
			NativeAdmission = EVoxelTerrainResourceAdmission::Granted;
			const uint64 BeforeBuild = Resources->ReleaseVersionExcludingOwner(Consumer);
			const auto Blocked = [&]
			{
				if (NativeAdmission != EVoxelTerrainResourceAdmission::Granted &&
					NativeAdmission != EVoxelTerrainResourceAdmission::CapacityBlocked) return MoveTemp(Result);
				if (!Result.Error.IsEmpty() && !Result.Error.StartsWith(TEXT("CapacityBlocked"))) return MoveTemp(Result);
				const uint32 ErrorHash = GetTypeHash(Result.Error);
				if (!bHasLoggedCapacityBlock || LastCapacityBlockHash != ErrorHash)
				{
					const FIntVector FineSection = FineBuild && FineBuild->Base ? FineBuild->Base->Section : FIntVector::ZeroValue;
					UE_LOG(LogTemp, Log, TEXT("[TerrainNative] preparation blocked: request=%llu phase=%d atoms=%d/%d coarse=%d/%d fine=(%d,%d,%d) finePhase=%d face=%u tile=%u error=%s"),
						Consumer.RequestSerial, Phase, AtomCursor, Publication->Atoms.Num(), CoarseCursor, Publication->CoarseChanges.Num(),
						FineSection.X, FineSection.Y, FineSection.Z, FineBuild ? FineBuild->Phase : INDEX_NONE,
						FineBuild ? uint32(FineBuild->FaceCursor) : 0u, FineBuild ? uint32(FineBuild->TileCursor) : 0u, *Result.Error);
					LastCapacityBlockHash = ErrorHash;
					bHasLoggedCapacityBlock = true;
				}
				CapacityVersion.Store(BeforeBuild);
				bWaitingCapacity.Store(true);
				Result.bSuccess = Result.bContinue = true;
				return MoveTemp(Result);
			};
			// 后台遍历按时间切片，避免每 32 个轻量节点都回到 GT 排队一帧。
			const double WorkDeadline = FPlatformTime::Seconds() + 0.002;
			for (int32 Step = 0; Step < 4096 && (Step == 0 || FPlatformTime::Seconds() < WorkDeadline); ++Step)
			{
				if (Cancel.Load()) { Result.bCanceled = true; return Result; }
				if (Phase == 0)
				{
					if (AtomCursor < Publication->Atoms.Num())
					{
						const auto& Atom = Publication->Atoms[AtomCursor++];
						Root = SetAtom(Root, Atom, 31, PrimaryPathLease, CellSize);
						const FVector Min = FVector(Atom->Section) * 16.0;
						ChangedBounds.Add(FBox(Min, Min + FVector(16.0)));
						continue;
					}
					if (Publication->CoarseChanges.IsValidIndex(CoarseCursor))
					{
						const auto& Change = Publication->CoarseChanges[CoarseCursor++];
						const auto& Item = Change.Item;
						CoarseRoot = SetCoarse(CoarseRoot, Item->Geometry->Stamp.GeometryIdentity.Key, Item->SourceId,
							Change.bRemove ? nullptr : Item, 77, PrimaryPathLease);
						ChangedBounds.Add(Item->OwnershipBounds);
						continue;
					}
					if (Root) InterfaceWalk.Add({Root, 0});
					Phase = 1;
					continue;
				}
				if (Phase == 1)
				{
					if (InterfaceWalk.IsEmpty()) { FineBuild = FirstFine; Phase = 2; continue; }
					auto& Frame = InterfaceWalk.Last();
					const FBox Bounds(Frame.Node->Bounds.Min / CellSize, Frame.Node->Bounds.Max / CellSize);
					if (!ChangedBounds.ContainsByPredicate([&Bounds](const FBox& Box) { return Box.ExpandBy(16.0).Intersect(Bounds); }))
					{ InterfaceWalk.Pop(EAllowShrinking::No); continue; }
					if (!Frame.Node->Atom)
					{
						if (Frame.Child == 8) InterfaceWalk.Pop(EAllowShrinking::No);
						else if (const auto Child = Frame.Node->Children[Frame.Child++]) InterfaceWalk.Add({Child, 0});
						continue;
					}
					if (!Frame.Node->Atom->FineGeometry) { InterfaceWalk.Pop(EAllowShrinking::No); continue; }
					if (!Publication->AffectsFineSection(Frame.Node->Atom->Section))
					{ InterfaceWalk.Pop(EAllowShrinking::No); continue; }
					TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
					if (!AcquireNative(EVoxelTerrainResourcePool::Input,
						VoxelBoundedAllocation::WorkerSharedBytes<FPreparedFine>(), Lease, Result.Error)) return Blocked();
					auto Fine = VoxelBoundedAllocation::MakeWorkerShared<FPreparedFine>(Lease, Resources, Consumer);
					Fine->Lease = MoveTemp(Lease);
					Fine->ControlInputLease = ControlInputLease;
					Fine->PreparationInputLease = InputLease;
					Fine->PublicationLease = Publication->Lease;
					Fine->PublicationCallbackLease = Publication->CoarseCallbackLease;
					Fine->bAllowOverlap = bAllowResidentOverlap;
					Fine->MaximumTextureStretchCells = MaximumTextureStretchCells;
					Fine->MeshPieceLimit = MeshPieceLimit;
					Fine->GeometryLimit = GeometryLimit;
					if (!Fine->Initialize(Frame.Node->Atom, Root, CoarseRoot, CellSize, Result.Error)) return Result;
					Fine->bReuseNativeInterfaces = Fine->CanReuseNativeInterfaces(BaseSnapshot);
					FineReused += Fine->bReuseNativeInterfaces ? 1 : 0;
					FineSourceRangesReused += !Fine->bReuseNativeInterfaces && Fine->bReuseSourceRanges ? 1 : 0;
					FinePrepared += Fine->bReuseNativeInterfaces ? 0 : 1;
					if (LastFine) LastFine->Next = Fine; else FirstFine = Fine;
					LastFine = MoveTemp(Fine);
					InterfaceWalk.Pop(EAllowShrinking::No);
					continue;
				}
				if (Phase == 2)
				{
					if (!FineBuild) { if (CoarseRoot) Owners.Add({CoarseRoot, 0}); Phase = 3; continue; }
					const auto Progress = FineBuild->Advance(*Registry, *Shapes, *Config, Cancel, Result.Error);
					if (Progress == EVoxelFineBuildProgress::CapacityBlocked) return Blocked();
					if (Progress == EVoxelFineBuildProgress::Working) continue;
					if (Progress != EVoxelFineBuildProgress::Complete)
					{ Result.bCanceled = Progress == EVoxelFineBuildProgress::Canceled; return Result; }
					++FineCompleted;
					HeightfieldTiles += FineBuild->HeightfieldTilesWithGeometry;
					HeightfieldPieces += FineBuild->HeightfieldPieces;
					FineBuild->Upload = FineBuild->FirstPiece;
					FineBuild = FineBuild->Next;
					continue;
				}
				if (!Current)
				{
					if (Owners.IsEmpty())
					{
						Result.bSuccess = true;
						return Result;
					}
					auto& Frame = Owners.Last();
					if (!ChangedBounds.ContainsByPredicate([&Frame](const FBox& Box) { return Frame.Node->CellBounds.Intersect(Box); }))
					{
						Owners.Pop(EAllowShrinking::No);
						continue;
					}
					if (!Frame.Node->Item)
					{
						if (Frame.Child == 8) Owners.Pop(EAllowShrinking::No);
						else if (const auto Child = Frame.Node->Children[Frame.Child++]) Owners.Add({Child, 0});
						continue;
					}
					if (!BeginItem(Frame.Node->Item, Result.Error)) return Blocked();
					Owners.Pop(EAllowShrinking::No);
					continue;
				}
				if (!FineWalk.IsEmpty())
				{
					auto& Frame = FineWalk.Last();
					const FBox CellBounds(Frame.Node->Bounds.Min / CellSize, Frame.Node->Bounds.Max / CellSize);
					if (!Current->OwnershipBounds.Intersect(CellBounds)) { FineWalk.Pop(EAllowShrinking::No); continue; }
					if (!Frame.Node->Atom)
					{
						if (Frame.Child == 8) FineWalk.Pop(EAllowShrinking::No);
						else if (const auto Child = Frame.Node->Children[Frame.Child++]) FineWalk.Add({Child, 0});
						continue;
					}
					if (!AddExclusion(CellBounds, Result.Error)) return Blocked();
					FineWalk.Pop(EAllowShrinking::No);
					continue;
				}
				if (!ExclusionWalk.IsEmpty())
				{
					auto& Frame = ExclusionWalk.Last();
					if (!Current->OwnershipBounds.Intersect(Frame.Node->CellBounds)) { ExclusionWalk.Pop(EAllowShrinking::No); continue; }
					if (!Frame.Node->Item)
					{
						if (Frame.Child == 8) ExclusionWalk.Pop(EAllowShrinking::No);
						else if (const auto Child = Frame.Node->Children[Frame.Child++]) ExclusionWalk.Add({Child, 0});
						continue;
					}
					if (!Frame.Node->Item->bBoundaryOnly && FinerCoarse(Frame.Node->Item->Geometry->Stamp.GeometryIdentity, Current->Geometry->Stamp.GeometryIdentity) &&
						!AddExclusion(Frame.Node->Item->OwnershipBounds, Result.Error)) return Blocked();
					ExclusionWalk.Pop(EAllowShrinking::No);
					continue;
				}
				if (!Builder)
				{
					if (!BuilderLease && !AcquireNative(EVoxelTerrainResourcePool::Input,
						FMemory::QuantizeSize(sizeof(FVoxelTerrainCoarsePatchBuilder), alignof(FVoxelTerrainCoarsePatchBuilder)),
						BuilderLease, Result.Error)) return Blocked();
					Builder = MakeUnique<FVoxelTerrainCoarsePatchBuilder>(Resources, Consumer, true, bAllowResidentOverlap);
					if (!Builder->Initialize(Current->Geometry, Current->Patch, Exclusions, ChangedBounds, Result.Error))
					{
						Builder.Reset();
						BuilderLease.Reset();
						return Blocked();
					}
				}
				const auto Progress = Builder->Advance(16, Cancel, Result.Error);
				if (Progress == EVoxelTerrainCoarseMeshProgress::CapacityBlocked) return Blocked();
				if (Progress == EVoxelTerrainCoarseMeshProgress::Working) continue;
				if (Progress != EVoxelTerrainCoarseMeshProgress::Complete)
				{
					Result.bCanceled = Progress == EVoxelTerrainCoarseMeshProgress::Canceled;
					return Result;
				}
				TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
				TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ItemLease;
				TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> PathLease;
				const uint64 ItemBytes = VoxelBoundedAllocation::WorkerSharedBytes<FVoxelTerrainRenderCoarse>() +
					VoxelBoundedAllocation::ArrayBytes<TSharedPtr<const FVoxelTerrainRenderChunk, ESPMode::ThreadSafe>>(Current->SourceChunks.Num()) +
					VoxelBoundedAllocation::ArrayBytes<TSharedPtr<const FVoxelTerrainCoarseDrawNode, ESPMode::ThreadSafe>>(Current->Geometry->Roots.Num());
				if (!AcquireNative(EVoxelTerrainResourcePool::Input, VoxelBoundedAllocation::WorkerSharedBytes<FPreparedCoarse>(), Lease, Result.Error) ||
					!AcquireNative(EVoxelTerrainResourcePool::Resident, ItemBytes, ItemLease, Result.Error) ||
					!ObserveAdmission(TryAcquirePublicationResident(Resources, Consumer,
						79ull * VoxelBoundedAllocation::WorkerSharedBytes<FCoarseTreeNode>(), PathLease, 79, bAllowResidentOverlap), Result.Error)) return Blocked();
				auto Prepared = VoxelBoundedAllocation::MakeWorkerShared<FPreparedCoarse>(Lease);
				Prepared->Lease = MoveTemp(Lease);
				Prepared->PathLease = MoveTemp(PathLease);
				Prepared->Base = Current;
				Prepared->Item = VoxelBoundedAllocation::MakeWorkerShared<FVoxelTerrainRenderCoarse>(ItemLease);
				auto& Item = *Prepared->Item;
				Item.SourceId = Current->SourceId;
				Item.bBoundaryOnly = Current->bBoundaryOnly;
				Item.MetadataLease = MoveTemp(ItemLease);
				Item.Geometry = Current->Geometry;
				Item.Evidence = Current->Evidence;
				Item.OwnershipBounds = Current->OwnershipBounds;
				Item.SourceChunks.Reserve(Current->SourceChunks.Num());
				Item.SourceChunks.Append(Current->SourceChunks);
				Item.Patch = Builder->TakeResult();
				Item.DrawRoots.Reserve(Item.Geometry->Roots.Num());
				Item.DrawRoots.SetNum(Item.Geometry->Roots.Num());
				if (LastPrepared) LastPrepared->Next = Prepared;
				else FirstPrepared = Prepared;
				LastPrepared = MoveTemp(Prepared);
				Builder.Reset();
				BuilderLease.Reset();
				Exclusions.Reset();
				Current.Reset();
			}
			Result.bSuccess = Result.bContinue = true;
			Result.bContinueImmediately = true;
			return Result;
		}

		TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
		FVoxelGenerationConsumerKey Consumer;
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InputLease;
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ControlInputLease;
		TSharedPtr<FAtomPublication, ESPMode::ThreadSafe> Publication;
		EVoxelTerrainServiceBand SchedulingBand = EVoxelTerrainServiceBand::P6;
		int32 ActiveIndex = INDEX_NONE;
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> PrimaryPathLease;
		FAtomNodePtr Root;
		FCoarseNodePtr CoarseRoot;
		FSnapshotPtr BaseSnapshot;
		// 冻结 Publication 已确定全部变化范围；小事务不携带最大粗批次的内嵌数组。
		TArray<FBox> ChangedBounds;
		TArray<FCoarseWalk, TInlineAllocator<79>> Owners;
		TArray<FCoarseWalk, TInlineAllocator<79>> ExclusionWalk;
		TArray<FFineWalk, TInlineAllocator<33>> FineWalk;
		TArray<FFineWalk, TInlineAllocator<33>> InterfaceWalk;
		TArray<FDrawWalk, TInlineAllocator<64>> DrawWalk;
		TArray<FDrawReadinessWalk, TInlineAllocator<64>> ReadinessDrawWalk;
		FCoarsePtr Current;
		TSharedPtr<FVoxelTerrainCoarseExclusions, ESPMode::ThreadSafe> Exclusions;
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> BuilderLease;
		TUniquePtr<FVoxelTerrainCoarsePatchBuilder> Builder;
		TSharedPtr<FPreparedCoarse, ESPMode::ThreadSafe> FirstPrepared;
		TSharedPtr<FPreparedCoarse, ESPMode::ThreadSafe> LastPrepared;
		TSharedPtr<FPreparedCoarse, ESPMode::ThreadSafe> Upload;
		TSharedPtr<FPreparedFine, ESPMode::ThreadSafe> FirstFine, LastFine, FineBuild, FineUpload;
		TSharedPtr<FPreparedFine, ESPMode::ThreadSafe> ReadinessFine;
		TSharedPtr<FPreparedCoarse, ESPMode::ThreadSafe> ReadinessCoarse;
		int32 ReadinessRoot = 0;
		bool bUploadReadinessStarted = false;
		TSharedPtr<const FVoxelRegistrySnapshot, ESPMode::ThreadSafe> Registry;
		TSharedPtr<const FVoxelShapeRegistry, ESPMode::ThreadSafe> Shapes;
		TSharedPtr<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> Config;
		double CellSize = 1.0;
		int32 Phase = 0;
		int32 AtomCursor = 0;
		int32 CoarseCursor = 0;
		int32 UploadRoot = 0;
		bool bDecodeComplete = false;
		bool bUploadComplete = false;
		EVoxelTerrainResourceAdmission UploadAdmission = EVoxelTerrainResourceAdmission::Granted;
		uint64 UploadCapacityVersion = 0;
		bool bEnqueued = false;
		TAtomic<bool> bExecuting{false};
		TAtomic<bool> bExecutionComplete{false};
		TAtomic<int32> DiagnosticPhase{0};
		TAtomic<int32> DiagnosticFineReused{0};
		TAtomic<int32> DiagnosticFineSourceRangesReused{0};
		TAtomic<int32> DiagnosticFinePrepared{0};
		TAtomic<int32> DiagnosticFineCompleted{0};
		TAtomic<int32> DiagnosticHeightfieldTiles{0};
		TAtomic<int32> DiagnosticHeightfieldPieces{0};
		TAtomic<int32> DiagnosticFineSectionX{0};
		TAtomic<int32> DiagnosticFineSectionY{0};
		TAtomic<int32> DiagnosticFineSectionZ{0};
		int32 FineCompleted = 0;
		int32 HeightfieldTiles = 0;
		int32 HeightfieldPieces = 0;
		int32 FineReused = 0;
		int32 FineSourceRangesReused = 0;
		int32 FinePrepared = 0;
		TAtomic<uint32> DiagnosticFineProgress{MAX_uint32};
		EVoxelTerrainResourceAdmission NativeAdmission = EVoxelTerrainResourceAdmission::Granted;
		TAtomic<bool> bWaitingCapacity{false};
		TAtomic<uint64> CapacityVersion{0};
		uint32 LastCapacityBlockHash = 0;
		bool bHasLoggedCapacityBlock = false;
		bool bAllowResidentOverlap = false;
		double MaximumTextureStretchCells = 8.0;
		uint64 MeshPieceLimit = 0;
		uint64 GeometryLimit = 0;
	};

	struct FRootMerge
	{
		FRootMerge(TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
			const FVoxelGenerationConsumerKey& InConsumer)
			: Resources(MoveTemp(InResources)), Consumer(InConsumer)
		{
		}

		struct FEntry
		{
			TSharedPtr<FRootPreparation, ESPMode::ThreadSafe> Preparation;
			FAtomNodePtr Root;
			FCoarseNodePtr CoarseRoot;
		};

		void Begin(TConstArrayView<TSharedPtr<FRootPreparation, ESPMode::ThreadSafe>> InPreparations,
			const FSnapshotPtr& InCurrent)
		{
			check(!InPreparations.IsEmpty() && InPreparations.Num() <= VoxelNativeRootPreparationConcurrency);
			for (const auto& Prepared : InPreparations) Entries.Add({Prepared});
			Preparation = Entries[0].Preparation;
			Current = InCurrent;
			MergedRoot = Current ? Current->Root : nullptr;
			MergedCoarseRoot = Current ? Current->CoarseRoot : nullptr;
			BeginEntry();
		}

		bool Contains(const TSharedPtr<FRootPreparation, ESPMode::ThreadSafe>& InPreparation) const
		{
			return InPreparation && Entries.ContainsByPredicate([&InPreparation](const FEntry& Entry)
			{
				return Entry.Preparation == InPreparation;
			});
		}

		bool AddBindingInputs(FRendererBindingInputFloor& OutFloor) const
		{
			if (!OutFloor.Add(InputLease, Resources.Get())) return false;
			for (const auto& Entry : Entries)
			{
				const auto& Prepared = *Entry.Preparation;
				if (!OutFloor.Add(Prepared.ControlInputLease, Resources.Get()) ||
					!OutFloor.Add(Prepared.InputLease, Resources.Get()) ||
					!OutFloor.Add(Prepared.Publication->CoarseCallbackLease, Resources.Get()) ||
					!OutFloor.Add(Prepared.PrimaryPathLease, Resources.Get()) ||
					!OutFloor.Add(Prepared.Publication->Lease, Resources.Get())) return false;
			}
			return true;
		}

		void BeginEntry()
		{
			const auto& Prepared = *Entries[MergeIndex].Preparation;
			InputCheck = Prepared.FirstFine;
			Fine.Begin(Prepared.BaseSnapshot ? Prepared.BaseSnapshot->Root : nullptr, MergedRoot, Prepared.Root);
			Coarse.Begin(Prepared.BaseSnapshot ? Prepared.BaseSnapshot->CoarseRoot : nullptr, MergedCoarseRoot, Prepared.CoarseRoot);
			Phase = 0;
		}

		template<typename NodeType>
		bool AllocateNode(TSharedPtr<NodeType, ESPMode::ThreadSafe>& OutNode,
			TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> NodeType::* LeaseMember)
		{
			TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
			if (!AcquireNativeResident(VoxelBoundedAllocation::WorkerSharedBytes<NodeType>(), Lease)) return false;
			OutNode = VoxelBoundedAllocation::MakeWorkerShared<NodeType>(Lease);
			OutNode.Get()->*LeaseMember = MoveTemp(Lease);
			return true;
		}

		bool AcquireNativeResident(const uint64 Bytes,
			TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& OutLease,
			const TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& InPendingLease = nullptr)
		{
			FRendererBindingInputFloor Floor;
			if (!AddBindingInputs(Floor) || !Floor.Add(InPendingLease, Resources.Get()))
			{
				NativeAdmission = EVoxelTerrainResourceAdmission::InvalidRequest;
				return false;
			}
			NativeAdmission = TryAcquirePublicationResident(Resources, Consumer, Bytes, OutLease, 0,
				Entries[MergeIndex].Preparation->bAllowResidentOverlap, Floor.Bytes, Floor.LeaseCount);
			return NativeAdmission == EVoxelTerrainResourceAdmission::Granted;
		}

		EVoxelPersistentMergeProgress MergeCoarseLeaf(const FCoarseNodePtr& Base,
			const FCoarseNodePtr& Latest, const FCoarseNodePtr& Prepared, FCoarseNodePtr& OutNode)
		{
			if (!Base || !Latest || !Prepared || !Base->Item || !Latest->Item || !Prepared->Item)
				return EVoxelPersistentMergeProgress::Conflict;
			const auto SameGeometry = [](const FCoarsePtr& A, const FCoarsePtr& B)
			{
				return A->SourceId == B->SourceId && A->bBoundaryOnly == B->bBoundaryOnly &&
					A->Geometry->GeometryIdentity == B->Geometry->GeometryIdentity && A->Geometry->Stamp == B->Geometry->Stamp;
			};
			if (!SameGeometry(Base->Item, Latest->Item) || !SameGeometry(Base->Item, Prepared->Item))
				return EVoxelPersistentMergeProgress::Conflict;
			if (!MergedItem)
			{
				const int32 Roots = Prepared->Item->Geometry->Roots.Num();
				TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ItemLease;
				TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> PatchLease;
				if (!AcquireNativeResident(
					VoxelBoundedAllocation::WorkerSharedBytes<FVoxelTerrainRenderCoarse>() +
					VoxelBoundedAllocation::ArrayBytes<TSharedPtr<const FVoxelTerrainRenderChunk, ESPMode::ThreadSafe>>(Prepared->Item->SourceChunks.Num()) +
						VoxelBoundedAllocation::ArrayBytes<TSharedPtr<const FVoxelTerrainCoarseDrawNode, ESPMode::ThreadSafe>>(Roots), ItemLease) ||
					!AcquireNativeResident(
						VoxelBoundedAllocation::WorkerSharedBytes<FVoxelTerrainCoarsePatch>() +
						VoxelBoundedAllocation::ArrayBytes<TSharedPtr<const FVoxelTerrainCoarsePatchNode, ESPMode::ThreadSafe>>(Roots), PatchLease, ItemLease))
					return EVoxelPersistentMergeProgress::CapacityBlocked;
				MergedItem = VoxelBoundedAllocation::MakeWorkerShared<FVoxelTerrainRenderCoarse>(ItemLease);
				MergedItem->MetadataLease = MoveTemp(ItemLease);
				MergedItem->SourceId = Prepared->Item->SourceId;
				MergedItem->bBoundaryOnly = Prepared->Item->bBoundaryOnly;
				MergedItem->Geometry = Prepared->Item->Geometry;
				MergedItem->Evidence = Prepared->Item->Evidence;
				MergedItem->OwnershipBounds = Prepared->Item->OwnershipBounds;
				MergedItem->SourceChunks.Reserve(Prepared->Item->SourceChunks.Num());
				MergedItem->SourceChunks.Append(Prepared->Item->SourceChunks);
				MergedItem->DrawRoots.Reserve(Roots);
				MergedItem->DrawRoots.SetNum(Roots);
				MergedPatch = VoxelBoundedAllocation::MakeWorkerShared<FVoxelTerrainCoarsePatch>(PatchLease);
				MergedPatch->MetadataLease = MoveTemp(PatchLease);
				MergedPatch->Geometry = MergedItem->Geometry;
				MergedPatch->Roots.Reserve(Roots);
				MergedPatch->Roots.SetNum(Roots);
				MergedItem->Patch = MergedPatch;
				MergeRoot = 0;
			}
			if (MergeRoot < MergedItem->DrawRoots.Num())
			{
				const auto RootAt = [this](const FCoarsePtr& Item)
				{
					return Item->DrawRoots.IsValidIndex(MergeRoot) ? Item->DrawRoots[MergeRoot] : nullptr;
				};
				if (!bDrawActive)
				{
					Draw.Begin(RootAt(Base->Item), RootAt(Latest->Item), RootAt(Prepared->Item));
					bDrawActive = true;
				}
				const auto Progress = Draw.Step(
					[](const auto& A, const auto& B) { return A == B || (A && B && A->Patch == B->Patch); },
					[this](const auto& B, const auto& C, const auto& P)
					{
						const auto& Any = P ? P : C ? C : B;
						return Any && MergedItem->Geometry->Ranges[Any->Patch->Range].IsLeaf();
					},
					[](const auto&, const auto&, const auto&, auto&) { return EVoxelPersistentMergeProgress::Conflict; },
					[this](const auto& B, const auto& C, const auto& P, auto& Out)
					{
						TSharedPtr<FVoxelTerrainCoarsePatchNode, ESPMode::ThreadSafe> Patch;
						TSharedPtr<FVoxelTerrainCoarseDrawNode, ESPMode::ThreadSafe> Node;
						if (!AllocateNode(Node, &FVoxelTerrainCoarseDrawNode::Lease) ||
							!AllocateNode(Patch, &FVoxelTerrainCoarsePatchNode::MetadataLease)) return false;
						const auto& Any = P ? P : C ? C : B;
						Patch->Range = Any->Patch->Range;
						Node->Patch = MoveTemp(Patch);
						Out = MoveTemp(Node);
						return true;
					},
					[](FVoxelTerrainCoarseDrawNode& Node)
					{
						auto Patch = ConstCastSharedPtr<FVoxelTerrainCoarsePatchNode>(Node.Patch);
						for (int32 Child = 0; Child < 2; ++Child)
							Patch->Children[Child] = Node.Children[Child] ? Node.Children[Child]->Patch : nullptr;
					});
				if (Progress != EVoxelPersistentMergeProgress::Complete) return Progress;
				MergedItem->DrawRoots[MergeRoot] = Draw.GetResult();
				MergedPatch->Roots[MergeRoot] = Draw.GetResult() ? Draw.GetResult()->Patch : nullptr;
				Draw.Reset();
				bDrawActive = false;
				++MergeRoot;
				return EVoxelPersistentMergeProgress::Working;
			}
			TSharedPtr<FCoarseTreeNode, ESPMode::ThreadSafe> Node;
			if (!AllocateNode(Node, &FCoarseTreeNode::Lease)) return EVoxelPersistentMergeProgress::CapacityBlocked;
			Node->Item = MoveTemp(MergedItem);
			MergedPatch.Reset();
			FinishCoarse(*Node);
			OutNode = MoveTemp(Node);
			return EVoxelPersistentMergeProgress::Complete;
		}

		static void FinishCoarse(FCoarseTreeNode& Node)
		{
			if (Node.Item)
			{
				Node.CellBounds = Node.Item->OwnershipBounds;
				if (!Node.Item->bBoundaryOnly) Node.OwnerCounts[uint8(Node.Item->Evidence.Owner.Kind)] = 1;
				for (const auto& Chunk : Node.Item->SourceChunks) if (Chunk) Node.MaterialRelevance |= Chunk->Relevance;
			}
			else for (const auto& Child : Node.Children)
			{
				if (!Child) continue;
				Node.CellBounds += Child->CellBounds;
				Node.MaterialRelevance |= Child->MaterialRelevance;
				for (int32 Kind = 0; Kind < UE_ARRAY_COUNT(Node.OwnerCounts); ++Kind) Node.OwnerCounts[Kind] += Child->OwnerCounts[Kind];
			}
		}

		FVoxelTaskResult Advance(const TAtomic<bool>& Cancel)
		{
			FVoxelTaskResult Result;
			bWaitingCapacity.Store(false);
			NativeAdmission = EVoxelTerrainResourceAdmission::Granted;
			const uint64 BeforeBuild = Resources->ReleaseVersionExcludingOwner(Consumer);
			const double WorkDeadline = FPlatformTime::Seconds() + 0.002;
			for (int32 Step = 0; Step < 4096 && (Step == 0 || FPlatformTime::Seconds() < WorkDeadline); ++Step)
			{
				if (Cancel.Load()) { Result.bCanceled = true; return Result; }
				if (InputCheck)
				{
					if (!InputCheck->InputsMatch(MergedRoot, MergedCoarseRoot, *Entries[MergeIndex].Preparation->Publication))
					{ Result.Error = TEXT("DependencyNotReady:NativeInterfaceInputsChanged"); return Result; }
					InputCheck = InputCheck->Next;
					continue;
				}
				EVoxelPersistentMergeProgress Progress;
				if (Phase == 0)
				{
					Progress = Fine.Step([](const auto& A, const auto& B) { return A == B; },
						[](const auto& B, const auto& C, const auto& P) { return (B && B->Atom) || (C && C->Atom) || (P && P->Atom); },
						[](const auto&, const auto&, const auto&, auto&) { return EVoxelPersistentMergeProgress::Conflict; },
						[this](const auto&, const auto&, const auto&, auto& Out) { return AllocateNode(Out, &FAtomTreeNode::PathLease); },
						[](FAtomTreeNode& Node)
						{
							for (const auto& Child : Node.Children)
							{
								if (!Child) continue;
								Node.Bounds += Child->Bounds;
								Node.FineAtomCount += Child->FineAtomCount;
								for (int32 Kind = 0; Kind < UE_ARRAY_COUNT(Node.OwnerAtomCounts); ++Kind) Node.OwnerAtomCounts[Kind] += Child->OwnerAtomCounts[Kind];
								Node.MaterialRelevance |= Child->MaterialRelevance;
							}
						});
					if (Progress == EVoxelPersistentMergeProgress::Complete) { Phase = 1; continue; }
				}
				else
				{
					Progress = Coarse.Step([](const auto& A, const auto& B) { return A == B; },
						[](const auto& B, const auto& C, const auto& P) { return (B && B->Item) || (C && C->Item) || (P && P->Item); },
						[this](const auto& B, const auto& C, const auto& P, auto& Out) { return MergeCoarseLeaf(B, C, P, Out); },
						[this](const auto&, const auto&, const auto&, auto& Out) { return AllocateNode(Out, &FCoarseTreeNode::Lease); },
						[](FCoarseTreeNode& Node) { FinishCoarse(Node); });
					if (Progress == EVoxelPersistentMergeProgress::Complete)
					{
						auto& Entry = Entries[MergeIndex];
						Entry.Root = MergedRoot = Fine.GetResult();
						Entry.CoarseRoot = MergedCoarseRoot = Coarse.GetResult();
						Fine.Reset();
						Coarse.Reset();
						if (++MergeIndex == Entries.Num())
						{
							Result.bSuccess = true;
							return Result;
						}
						BeginEntry();
						continue;
					}
				}
				if (Progress == EVoxelPersistentMergeProgress::Conflict)
				{
					Result.Error = TEXT("DependencyNotReady:RenderRootConflict");
#if !UE_BUILD_SHIPPING
					UE_LOG(LogTemp, Display, TEXT("Native render root merge conflict: request=%llu phase=%d atoms=%d coarse=%d base=%llu current=%llu"),
						Consumer.RequestSerial, Phase, Entries[MergeIndex].Preparation->Publication->Atoms.Num(), Entries[MergeIndex].Preparation->Publication->CoarseChanges.Num(),
						Entries[MergeIndex].Preparation->BaseSnapshot ? Entries[MergeIndex].Preparation->BaseSnapshot->Serial : 0, Current ? Current->Serial : 0);
#endif
					return Result;
				}
				if (Progress == EVoxelPersistentMergeProgress::CapacityBlocked)
				{
					if (NativeAdmission != EVoxelTerrainResourceAdmission::Granted &&
						NativeAdmission != EVoxelTerrainResourceAdmission::CapacityBlocked)
					{
						Result.Error = NativeAdmission == EVoxelTerrainResourceAdmission::CapacityUnachievable ?
							TEXT("CapacityUnachievable: Native render merge") : TEXT("InvalidRequest: Native render merge");
						return Result;
					}
					CapacityVersion.Store(BeforeBuild);
					bWaitingCapacity.Store(true);
					break;
				}
			}
			Result.bSuccess = Result.bContinue = true;
			Result.bContinueImmediately = !bWaitingCapacity.Load();
			return Result;
		}

		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InputLease;
		TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
		FVoxelGenerationConsumerKey Consumer;
		TSharedPtr<FRootPreparation, ESPMode::ThreadSafe> Preparation;
		FSnapshotPtr Current;
		TArray<FEntry, TInlineAllocator<VoxelNativeRootPreparationConcurrency>> Entries;
		FAtomNodePtr MergedRoot;
		FCoarseNodePtr MergedCoarseRoot;
		int32 MergeIndex = 0;
		TSharedPtr<FPreparedFine, ESPMode::ThreadSafe> InputCheck;
		TVoxelPersistentRootMerge<FAtomTreeNode, 8, 33> Fine;
		TVoxelPersistentRootMerge<FCoarseTreeNode, 8, 79> Coarse;
		TVoxelPersistentRootMerge<FVoxelTerrainCoarseDrawNode, 2, 64> Draw;
		TSharedPtr<FVoxelTerrainRenderCoarse, ESPMode::ThreadSafe> MergedItem;
		TSharedPtr<FVoxelTerrainCoarsePatch, ESPMode::ThreadSafe> MergedPatch;
		int32 MergeRoot = 0;
		int32 Phase = 0;
		bool bDrawActive = false;
		bool bComplete = false;
		EVoxelTerrainResourceAdmission NativeAdmission = EVoxelTerrainResourceAdmission::Granted;
		TAtomic<bool> bSuspended{false};
		TAtomic<bool> bWaitingCapacity{false};
		TAtomic<uint64> CapacityVersion{0};
	};

#include "VoxelResidentPromotion.inl"
}

namespace
{
	struct FVoxelRendererSectionChanged
	{
		TWeakObjectPtr<UVoxelModule> Module;
		uint64 Epoch = 0;
		void operator()(const FIntVector& InSection) const;
	};

	struct FVoxelRendererDeleteState
	{
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
		FVoxelTerrainRendererState* State = nullptr;
		void operator()() const;
	};

	struct FVoxelRendererStateDeleter
	{
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
		void operator()(FVoxelTerrainRendererState* InState) const;
	};

	struct FVoxelRendererRetirement
	{
		TSharedPtr<FVoxelTerrainRendererState, ESPMode::ThreadSafe> State;
		bool operator()(float InDelta) const;
	};
}

struct FVoxelTerrainRendererState : TSharedFromThis<FVoxelTerrainRendererState, ESPMode::ThreadSafe>
{
	FVoxelTerrainRendererState(UVoxelModule& InModule, FVoxelTaskScheduler& InScheduler, const uint64 InEpoch,
		const FVoxelGenerationConsumerKey& InConsumer, TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InLease)
		: InputLease(MoveTemp(InLease)), ControlConsumer(InConsumer), Module(InModule), Scheduler(InScheduler),
			Resources(InScheduler.GetTerrainResources()), Handoff(InEpoch), Epoch(InEpoch), CellSize(InModule.BlockSize()), OwnerId(InConsumer.OwnerId)
	{
		const auto& SelectionConsumer = ControlConsumer;
		verify(Handoff.BindResources(Resources, ControlConsumer, InputLease));
		SealSelection.BindResources(Resources, SelectionConsumer);
		PreparationSelection.BindResources(Resources, SelectionConsumer);
		UploadSelection.BindResources(Resources, SelectionConsumer);
	}

	FSnapshotPtr SealFrameSnapshot(uint64 InFrameCounter, double InDeadline);
	FSnapshotPtr BindFrameSnapshot(uint64 InFrameCounter);
	bool PrepareMetadata();
	void InsertReady(FReadyTransaction InReady);
	void RemoveReady(int32 InIndex);
	bool BeginRootPreparation(FReadyTransaction& InTransaction);
	void AdvanceRootPreparations(double InDeadline);
	void AdvanceRootUpload(FVoxelTerrainRenderer& InRenderer, double InDeadline);
	bool AdvanceRootUpload(FVoxelTerrainRenderer& InRenderer, FRootPreparation& InWork, int32 InMaximumSteps, double InDeadline);
	bool BeginRootMerge(FReadyTransaction& InTransaction, double InDeadline);
	void ReleasePreparation(FReadyTransaction& InTransaction);
	void TerminateTransaction(FReadyTransaction& InTransaction);
	void ReleaseRootMerge(TSharedPtr<FRootMerge, ESPMode::ThreadSafe> InMerge = nullptr);
	FRetiringFrame& RegisterFrameSubmission(const FSnapshotPtr& InSnapshot, const FFrameSubmissionData& InData);
	const FRenderSnapshot* NextRetiringFrame();
	void RemoveRetiringFrame(const FRenderSnapshot& InSnapshot);
	void AdvanceFrameReceipt(FRetiringFrame& InFrame, bool bInClosing);
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InputLease;
	FVoxelGenerationConsumerKey ControlConsumer;
	EVoxelTerrainResourceAdmission ProxyAdmission = EVoxelTerrainResourceAdmission::None;
	uint64 ProxyCapacityVersion = 0;
	bool bProxyRecreationPending = false;
	UVoxelModule& Module;
	FVoxelTaskScheduler& Scheduler;
	TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	FVoxelFineHandoff Handoff;
	uint64 Epoch = 0;
	double CellSize = 1.0;
	uint64 NextPresentation = 0;
	uint64 NextMetadataSerial = 0;
	uint64 NextCoarseAttemptSerial = 0;
	uint64 NativeFrontendRevision = 0;
	void AdvanceCoarseAdmissionVersion()
	{
		if (NativeFrontendRevision < MAX_uint64) ++NativeFrontendRevision;
	}
	uint64 NextCoverageSerial = 0;
	uint64 LastFrame = MAX_uint64;
	FGuid OwnerId;
	TWeakObjectPtr<UVoxelTerrainPrimitiveComponent> Component;
	TWeakObjectPtr<AActor> Host;
	TStrongObjectPtr<UVoxelTerrainPrimitiveComponent> ComponentPin;
	TSharedPtr<ISceneViewExtension, ESPMode::ThreadSafe> Extension;
	TArray<FReadyTransaction> Ready;
	TArray<FRootPreparation*, TInlineAllocator<16>> ActivePreparations;
	TSharedPtr<FRootMerge, ESPMode::ThreadSafe> RootMerge;
	FVoxelTerrainWorkSelection SealSelection;
	FVoxelTerrainWorkSelection PreparationSelection;
	FVoxelTerrainWorkSelection UploadSelection;
	double PreparationCostMicroseconds = 50.0;
	double UploadCostMicroseconds = 50.0;
	int32 PreparationScanCursor = 0;
	int32 PreparationAdmissionCursor = 0;
	int32 PreparationRetryCursor = 0;
	int32 PreparationPriorityCursor = 0;
	uint64 PreparationPriorityPass = 1;
	int32 PreparationDiagnosticCursor = 0;
	double NextPreparationDiagnostic = 0.0;
	int32 NextPublicationPhase = 0;
	double SealCostMicroseconds = 50.0;
	int32 SealScanCursor = 0;
	TMap<FGuid, TFunction<void()>> SubmittedCallbacks;
	TSet<FGuid> PendingCoarseSources;
	FSnapshotPtr LastSealedSnapshot;
	FSnapshotPtr BoundSnapshot;
	uint64 LastBoundFrame = MAX_uint64;
	TArray<FSnapshotPtr> RetiringBoundSnapshots;
	TArray<FFrozenPublication> PendingIssued;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> MetadataLease;
	FVoxelGenerationConsumerKey MetadataConsumer;
	EVoxelTerrainResourceAdmission MetadataAdmission = EVoxelTerrainResourceAdmission::None;
	uint64 MetadataCapacityVersion = 0;
	TSharedPtr<const FPresentedSnapshot, ESPMode::ThreadSafe> PresentedSnapshot;
	FSnapshotPtr PresentedFrame;
	TSharedPtr<FResidentPromotion, ESPMode::ThreadSafe> ResidentPromotion;
	TVoxelCoalescedSubmissionMailbox<FRenderSnapshot, FFrameSubmissionData> Submissions;
	const FRenderSnapshot* RetiringFrameHead = nullptr;
	const FRenderSnapshot* RetiringFrameTail = nullptr;
	const FRenderSnapshot* RetiringFrameCursor = nullptr;
	int32 RetiringFrameCount = 0;
	double SubmissionEstimateMilliseconds = 0.02;
	double ReceiptEstimateMilliseconds = 0.02;
	double FrameRetirementEstimateMilliseconds = 0.02;
	double PromotionEstimateMilliseconds = 0.02;
	TSharedPtr<const FPresentedSnapshot, ESPMode::ThreadSafe> RetirementSnapshot;
	TArray<FRetirementWalkFrame, TInlineAllocator<33>> RetirementWalk;
	TSharedPtr<const FPresentedSnapshot, ESPMode::ThreadSafe> CoarseRetirementSnapshot;
	TArray<FCoarseWalk, TInlineAllocator<79>> CoarseRetirementWalk;
	TAtomic<bool> bActive{true};
};

void FVoxelTerrainRendererState::InsertReady(FReadyTransaction InReady)
{
	check(Ready.Num() + SubmittedCallbacks.Num() < VoxelNativePublicationCapacity);
	int32 First = 0;
	int32 Last = Ready.Num();
	while (First < Last)
	{
		const int32 Middle = First + (Last - First) / 2;
		if (HigherVisibleTransaction(Ready[Middle], InReady)) First = Middle + 1;
		else Last = Middle;
	}
	Ready.Insert(MoveTemp(InReady), First);
	if (First < SealScanCursor) ++SealScanCursor;
}

void FVoxelTerrainRendererState::RemoveReady(const int32 InIndex)
{
	check(Ready.IsValidIndex(InIndex));
	if (InIndex < SealScanCursor) --SealScanCursor;
	Ready.RemoveAt(InIndex, 1, EAllowShrinking::No);
}

namespace
{
	class FVoxelTerrainViewExtension final : public FSceneViewExtensionBase
	{
	public:
		FVoxelTerrainViewExtension(const FAutoRegister& InRegister,
			TSharedRef<FVoxelTerrainRendererState, ESPMode::ThreadSafe> InState, FSceneInterface* InScene)
			: FSceneViewExtensionBase(InRegister), State(InState), Scene(InScene)
		{
		}

		virtual void SetupViewFamily(FSceneViewFamily& InFamily) override {}
		virtual void SetupView(FSceneViewFamily& InFamily, FSceneView& InView) override {}
		virtual void BeginRenderViewFamily(FSceneViewFamily& InFamily) override
		{
			check(IsInGameThread());
			if (InFamily.Scene != Scene || !State->bActive.Load()) return;
			FFrameBinding* Binding = InFamily.GetOrCreateExtentionData<FFrameBinding>();
			// 首个参与的 Family 固定根，之后的主视图和 SceneCapture 只借用同一根。
			if (!Binding->Snapshot) Binding->Snapshot = State->BindFrameSnapshot(GFrameCounter);
		}

		virtual void PostRenderViewFamily_RenderThread(FRDGBuilder& InGraph, FSceneViewFamily& InFamily) override
		{
			if (InFamily.Scene != Scene) return;
			const FFrameBinding* Binding = InFamily.GetExtentionData<FFrameBinding>();
			if (!Binding || !Binding->Snapshot || !Binding->bProxySubmitted.Load() ||
				Binding->Snapshot->WorldEpoch != State->Epoch || Binding->Snapshot->RendererOwner != State->OwnerId) return;
			const FSnapshotPtr Snapshot = Binding->Snapshot;
			const auto PinnedState = State;
			InGraph.AddPass(RDG_EVENT_NAME("VoxelTerrainRenderReceipt"), ERDGPassFlags::NeverCull,
				[PinnedState, Snapshot](FRHICommandListImmediate& RHICmdList)
				{
					const FGPUFenceRHIRef Fence = RHICreateGPUFence(TEXT("VoxelTerrainFrameRetirement"));
					const uint32 GpuMask = RHICmdList.GetGPUMask().GetNative();
					RHICmdList.WriteGPUFence(Fence);
					const bool bHasReceipts = !Snapshot->bReceiptProduced.Exchange(true);
					PinnedState->Submissions.Publish(Snapshot, Snapshot->SubmissionSlot,
						[&Fence, GpuMask, bHasReceipts](FFrameSubmissionData& Data)
						{
							Data.GpuMask |= GpuMask;
							Data.bHasReceipts |= bHasReceipts;
							for (int32 Gpu = 0; Gpu < MAX_NUM_GPUS; ++Gpu)
								if ((GpuMask & (1u << Gpu)) != 0) Data.GpuFences[Gpu] = Fence;
							});
				});
		}

	private:
		TSharedRef<FVoxelTerrainRendererState, ESPMode::ThreadSafe> State;
		FSceneInterface* Scene = nullptr;
	};

	struct FVoxelSceneProxyStorage
	{
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	};

	class FVoxelTerrainSceneProxy final : public FPrimitiveSceneProxy
	{
	public:
		static SIZE_T StorageOffset()
		{
			return Align(sizeof(FVoxelSceneProxyStorage), alignof(FVoxelTerrainSceneProxy));
		}

		static uint64 MaterialListAllocationBytes(const int32 InCount)
		{
			FDefaultAllocator::ForElementType<UMaterialInterface*> Allocator;
			int32 Capacity = 0;
			for (int32 Index = 0; Index < InCount; ++Index)
			{
				if (Index == Capacity)
					Capacity = Allocator.CalculateSlackGrow(Index + 1, Capacity, sizeof(UMaterialInterface*));
			}
			return VoxelBoundedAllocation::QuantizeBytes(uint64(Capacity) * sizeof(UMaterialInterface*));
		}

		static uint64 AllocationBytes(const int32 InMaterialCount)
		{
			return FMemory::QuantizeSize(StorageOffset() + sizeof(FVoxelTerrainSceneProxy),
				FMath::Max(alignof(FVoxelSceneProxyStorage), alignof(FVoxelTerrainSceneProxy)))
#if WITH_EDITOR
				+ MaterialListAllocationBytes(InMaterialCount)
#endif
				;
		}

		static void* operator new(SIZE_T InSize, TSharedRef<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InLease)
		{
			void* Storage = FMemory::Malloc(StorageOffset() + InSize,
				FMath::Max(alignof(FVoxelSceneProxyStorage), alignof(FVoxelTerrainSceneProxy)));
			new (Storage) FVoxelSceneProxyStorage{InLease};
			return static_cast<uint8*>(Storage) + StorageOffset();
		}

		static void operator delete(void* InObject)
		{
			if (!InObject) return;
			auto* Storage = reinterpret_cast<FVoxelSceneProxyStorage*>(static_cast<uint8*>(InObject) - StorageOffset());
			auto Lease = MoveTemp(Storage->Lease);
			Storage->~FVoxelSceneProxyStorage();
			FMemory::Free(Storage);
			Lease.Reset();
		}

		static void operator delete(void* InObject, TSharedRef<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InLease)
		{
			operator delete(InObject);
		}

		explicit FVoxelTerrainSceneProxy(const UVoxelTerrainPrimitiveComponent* InComponent)
			: FPrimitiveSceneProxy(InComponent), State(InComponent->TerrainState)
		{
			EnableGPUSceneSupportFlags();
		}

		virtual SIZE_T GetTypeHash() const override
		{
			static size_t Type;
			return reinterpret_cast<size_t>(&Type);
		}
		virtual uint32 GetMemoryFootprint() const override { return sizeof(*this) + GetAllocatedSize(); }
		virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* InView) const override
		{
			FPrimitiveViewRelevance Relevance;
			Relevance.bDrawRelevance = IsShown(InView);
			Relevance.bShadowRelevance = IsShadowCast(InView);
			Relevance.bDynamicRelevance = true;
			Relevance.bRenderInMainPass = ShouldRenderInMainPass();
			const FFrameBinding* Binding = InView->Family->GetExtentionData<FFrameBinding>();
			FMaterialRelevance Materials;
			const bool bOwnSnapshot = Binding && Binding->Snapshot && Binding->Snapshot->WorldEpoch == State->Epoch &&
				Binding->Snapshot->RendererOwner == State->OwnerId;
			if (bOwnSnapshot && Binding->Snapshot->Root)
				Materials |= Binding->Snapshot->Root->MaterialRelevance;
			if (bOwnSnapshot && Binding->Snapshot->CoarseRoot)
				Materials |= Binding->Snapshot->CoarseRoot->MaterialRelevance;
			Materials.SetPrimitiveViewRelevance(Relevance);
			// UE 的速度预通道可能同时负责深度；必须与代理的速度资格同步声明。
			Relevance.bVelocityRelevance = DrawsVelocity() && Relevance.bOpaque && Relevance.bRenderInMainPass;
			return Relevance;
		}

		virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& InViews,
			const FSceneViewFamily& InFamily, uint32 InVisibilityMap, FMeshElementCollector& InCollector) const override
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_TerrainGatherDynamicMeshes);
			const FFrameBinding* Binding = InFamily.GetExtentionData<FFrameBinding>();
			if (!Binding || !Binding->Snapshot || Binding->Snapshot->WorldEpoch != State->Epoch ||
				Binding->Snapshot->RendererOwner != State->OwnerId) return;
			const FSnapshotPtr Snapshot = Binding->Snapshot;
			if (InVisibilityMap) Binding->bProxySubmitted.Store(true);
			InCollector.AllocateOneFrameResource<FCollectorFrameLease>().Snapshot = Snapshot;
			FPrimitiveUniformShaderParametersBuilder CommonUniformBuilder;
			bool bCommonUniformReady = false;
			TMap<const FVoxelTerrainRenderChunk*, FDynamicPrimitiveUniformBuffer*, FDefaultSetAllocator, FVoxelTerrainUniformKeyFuncs> Uniforms;
			Uniforms.Reserve(256);
			const auto FeatureLevel = InCollector.GetFeatureLevel();
			const auto ShaderPlatform = GShaderPlatformForFeatureLevel[FeatureLevel];
			const bool bGPUSceneContents = FeatureLevel != ERHIFeatureLevel::ES3_1 && UseGPUScene(ShaderPlatform) &&
				!PlatformGPUSceneUsesUniformBufferView(ShaderPlatform);
			for (int32 ViewIndex = 0; ViewIndex < InViews.Num(); ++ViewIndex)
			{
				if (!(InVisibilityMap & (1u << ViewIndex))) continue;
				int32 DrawnTriangles = 0;
				int32 DrawRanges = 0;
				int32 SubmittedDraws = 0;
				int32 SubmittedBatches = 0;
				int32 UniformBuffers = 0;
				int32 RHIUniforms = 0;
				int32 GPUSceneDraws = 0;
				int32 SourceChunkSteps = 0;
				int32 ChunkFrustumTests = 0;
				int32 ReusedChunkVisibility = 0;
				FBox DrawnBounds(ForceInit);
				const FVoxelTerrainRenderChunk* FirstDrawnChunk = nullptr;
				const FVoxelTerrainRenderChunk* PendingChunk = nullptr;
				int32 PendingFirstTriangle = 0;
				int32 PendingTriangleCount = 0;
				TMap<const FVoxelTerrainRenderChunk*, FMeshBatch*> PendingMeshes;
				TArray<FMeshBatch*> OrderedMeshes;
				const auto FlushMeshes = [&]()
				{
					for (FMeshBatch* Mesh : OrderedMeshes)
					{
						InCollector.AddMesh(ViewIndex, *Mesh);
						++SubmittedBatches;
					}
					OrderedMeshes.Reset();
					PendingMeshes.Reset();
				};
				const auto Submit = [&](const FVoxelTerrainRenderChunk* Chunk,
					const int32 FirstTriangle, const int32 TriangleCount)
				{
					TRACE_CPUPROFILER_EVENT_SCOPE_CONDITIONAL(Voxel_TerrainSubmitDrawSample, (SubmittedDraws & 1023) == 0);
					const bool bCanGroup = (Chunk->Relevance.bOpaque || Chunk->Relevance.bMasked) &&
						!Chunk->Relevance.bNormalTranslucency && !Chunk->Relevance.bSeparateTranslucency &&
						!Chunk->Relevance.bPostMotionBlurTranslucency;
					if (!bCanGroup) FlushMeshes();
					FMeshBatch** Existing = bCanGroup ? PendingMeshes.Find(Chunk) : nullptr;
					if (Existing && (*Existing)->Elements.Num() < 32)
					{
						FMeshBatchElement Element = (*Existing)->Elements[0];
						Element.FirstIndex = FirstTriangle * 3;
						Element.NumPrimitives = TriangleCount;
						(*Existing)->Elements.Add(Element);
						GPUSceneDraws += bGPUSceneContents && Chunk->bGPUSceneStreams && FeatureLevel == Chunk->FeatureLevel ? 1 : 0;
						++SubmittedDraws;
						DrawnTriangles += TriangleCount;
						return;
					}
					FMeshBatch& Mesh = InCollector.AllocateMesh();
					OrderedMeshes.Add(&Mesh);
					if (bCanGroup) PendingMeshes.Add(Chunk, &Mesh);
					FMeshBatchElement& Element = Mesh.Elements[0];
					Mesh.VertexFactory = &Chunk->VertexFactory;
					Mesh.MaterialRenderProxy = Chunk->Material;
					Mesh.Type = PT_TriangleList;
					Mesh.DepthPriorityGroup = SDPG_World;
					Mesh.bSelectable = true;
					Mesh.bUseSelectionOutline = IsSelected();
					Element.IndexBuffer = &Chunk->IndexBuffer;
					Element.FirstIndex = FirstTriangle * 3;
					Element.NumPrimitives = TriangleCount;
					Element.MinVertexIndex = 0;
					Element.MaxVertexIndex = Chunk->VertexBuffers.PositionVertexBuffer.GetNumVertices() - 1;
					const bool bUsesGPUScene = bGPUSceneContents && Chunk->bGPUSceneStreams &&
						FeatureLevel == Chunk->FeatureLevel;
					auto& Uniform = Uniforms.FindOrAdd(Chunk);
					if (!Uniform)
					{
						TRACE_CPUPROFILER_EVENT_SCOPE_CONDITIONAL(Voxel_TerrainCreatePrimitiveUniformSample, (UniformBuffers & 1023) == 0);
						if (!bCommonUniformReady)
						{
							TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_TerrainBuildCommonPrimitiveUniform);
							BuildUniformShaderParameters(CommonUniformBuilder);
							bCommonUniformReady = true;
						}
						FPrimitiveUniformShaderParametersBuilder Builder = CommonUniformBuilder;
						Builder.LocalToWorld(Chunk->LocalToWorld).PreviousLocalToWorld(Chunk->LocalToWorld)
							.WorldBounds(FBoxSphereBounds(Chunk->WorldBounds)).LocalBounds(FBoxSphereBounds(Chunk->LocalBounds));
						Uniform = &InCollector.AllocateOneFrameResource<FDynamicPrimitiveUniformBuffer>();
						Uniform->UniformBuffer.BufferUsage = UniformBuffer_SingleFrame;
						Uniform->UniformBuffer.SetContents(InCollector.GetRHICommandList(), Builder.Build());
						++UniformBuffers;
					}
					if (!bUsesGPUScene && !Uniform->UniformBuffer.IsInitialized())
					{
						Uniform->UniformBuffer.InitResource(InCollector.GetRHICommandList());
						++RHIUniforms;
					}
					GPUSceneDraws += bUsesGPUScene ? 1 : 0;
					Element.PrimitiveUniformBufferResource = &Uniform->UniformBuffer;
					++SubmittedDraws;
					DrawnTriangles += TriangleCount;
					DrawnBounds += Chunk->WorldBounds;
					if (!FirstDrawnChunk) FirstDrawnChunk = Chunk;
					if (!bCanGroup) FlushMeshes();
				};
				const auto Draw = [&](const FVoxelTerrainRenderChunk* Chunk,
					const int32 FirstTriangle, const int32 TriangleCount)
				{
					if (!Chunk || TriangleCount == 0) return;
					if (PendingChunk == Chunk)
					{
						++ReusedChunkVisibility;
					}
					else
					{
						if (!Chunk->bReady.Load()) return;
						++ChunkFrustumTests;
						if (!InViews[ViewIndex]->ViewFrustum.IntersectBox(Chunk->WorldBounds.GetCenter(), Chunk->WorldBounds.GetExtent())) return;
					}
					check(FirstTriangle >= 0 && int64(FirstTriangle) + TriangleCount <= Chunk->SourceTriangleCount);
					++DrawRanges;
					if (PendingChunk == Chunk && PendingFirstTriangle + PendingTriangleCount == FirstTriangle)
					{
						PendingTriangleCount += TriangleCount;
						return;
					}
					if (PendingChunk) Submit(PendingChunk, PendingFirstTriangle, PendingTriangleCount);
					PendingChunk = Chunk;
					PendingFirstTriangle = FirstTriangle;
					PendingTriangleCount = TriangleCount;
				};
				const auto FineVisit = [&](auto&& Self, const FAtomNodePtr& Node) -> void
				{
					if (!Node || !InViews[ViewIndex]->ViewFrustum.IntersectBox(Node->Bounds.GetCenter(), Node->Bounds.GetExtent())) return;
					if (Node->Atom)
					{
						if (Node->Atom->bUseDrawRanges)
							for (const auto& Range : Node->Atom->DrawRanges) Draw(Range.Chunk.Get(), Range.FirstTriangle, Range.TriangleCount);
						else for (const auto& Chunk : Node->Atom->Chunks)
							if (Chunk) Draw(Chunk.Get(), 0, Chunk->SourceTriangleCount);
					}
					else for (const FAtomNodePtr& Child : Node->Children) Self(Self, Child);
				};
				FineVisit(FineVisit, Snapshot->Root);
				const auto CoarseVisit = [&](auto&& Self, const FCoarseNodePtr& Node) -> void
				{
					if (!Node) return;
					const FBox Bounds(Node->CellBounds.Min * State->CellSize, Node->CellBounds.Max * State->CellSize);
					if (!InViews[ViewIndex]->ViewFrustum.IntersectBox(Bounds.GetCenter(), Bounds.GetExtent())) return;
					if (!Node->Item)
					{
						for (const auto& Child : Node->Children) Self(Self, Child);
						return;
					}
					const auto& Item = *Node->Item;
					const FVoxelTerrainRenderChunk* SourceCursor = nullptr;
					const auto RangeVisit = [&](auto&& RangeSelf, const int32 RangeIndex,
						const TSharedPtr<const FVoxelTerrainCoarseDrawNode, ESPMode::ThreadSafe>& Patch) -> void
					{
						const auto& Range = Item.Geometry->Ranges[RangeIndex];
						const FBox RangeBounds(Range.WorldCellBounds.Min * State->CellSize, Range.WorldCellBounds.Max * State->CellSize);
						if (!InViews[ViewIndex]->ViewFrustum.IntersectBox(RangeBounds.GetCenter(), RangeBounds.GetExtent())) return;
						if (!Patch)
						{
							const int32 End = Range.FirstTriangle + Range.TriangleCount;
							while (SourceCursor && SourceCursor->FirstSourceTriangle + SourceCursor->SourceTriangleCount <= Range.FirstTriangle)
							{
								SourceCursor = SourceCursor->NextSourceChunk.Get();
								++SourceChunkSteps;
							}
							for (const auto* Chunk = SourceCursor; Chunk && Chunk->FirstSourceTriangle < End; Chunk = Chunk->NextSourceChunk.Get())
							{
								SourceCursor = Chunk;
								++SourceChunkSteps;
								const int32 First = FMath::Max(Range.FirstTriangle, Chunk->FirstSourceTriangle);
								const int32 Last = FMath::Min(End, Chunk->FirstSourceTriangle + Chunk->SourceTriangleCount);
								if (Last > First) Draw(Chunk, First - Chunk->FirstSourceTriangle, Last - First);
							}
						}
						else if (!Range.IsLeaf())
						{
							RangeSelf(RangeSelf, Range.Children[0], Patch->Children[0]);
							RangeSelf(RangeSelf, Range.Children[1], Patch->Children[1]);
						}
						else for (const auto* Piece = Patch->FirstPiece.Get(); Piece; Piece = Piece->Next.Get())
							Draw(Piece->Chunk.Get(), 0, Piece->Chunk->SourceTriangleCount);
					};
					for (int32 Root = 0; Root < Item.Geometry->Roots.Num(); ++Root)
					{
						const int32 RangeIndex = Item.Geometry->Roots[Root];
						SourceCursor = Item.SourceChunks[Item.Geometry->Ranges[RangeIndex].Batch].Get();
						RangeVisit(RangeVisit, RangeIndex, Item.DrawRoots[Root]);
					}
				};
				CoarseVisit(CoarseVisit, Snapshot->CoarseRoot);
				if (PendingChunk) Submit(PendingChunk, PendingFirstTriangle, PendingTriangleCount);
				FlushMeshes();
#if !UE_BUILD_SHIPPING
				const double Now = FPlatformTime::Seconds();
				if (Now - LastDrawDiagnosticSeconds >= 5.0)
				{
					LastDrawDiagnosticSeconds = Now;
					const FPrimitiveViewRelevance Relevance = GetViewRelevance(InViews[ViewIndex]);
					UE_LOG(LogTemp, Display, TEXT("Voxel native draw: triangles=%d camera=%s bounds=%s main=%d opaque=%d masked=%d translucent=%d"),
						DrawnTriangles, *InViews[ViewIndex]->ViewLocation.ToString(), *DrawnBounds.ToString(),
						Relevance.bRenderInMainPass, Relevance.bOpaque, Relevance.bMasked, Relevance.HasTranslucency());
					UE_LOG(LogTemp, Display, TEXT("Voxel native draw ranges: requested=%d submitted=%d merged=%d uniforms=%d reused=%d cacheEntries=%d cacheBytes=%llu"),
						DrawRanges, SubmittedDraws, DrawRanges - SubmittedDraws, UniformBuffers, SubmittedDraws - UniformBuffers,
						Uniforms.Num(), uint64(Uniforms.GetAllocatedSize()));
					UE_LOG(LogTemp, Display, TEXT("Voxel native mesh batches: batches=%d elements=%d grouped=%d"),
						SubmittedBatches, SubmittedDraws, SubmittedDraws - SubmittedBatches);
					UE_LOG(LogTemp, Display, TEXT("Voxel native coarse draw traversal: sourceChunkSteps=%d"), SourceChunkSteps);
					UE_LOG(LogTemp, Display, TEXT("Voxel native chunk visibility: tested=%d reused=%d"), ChunkFrustumTests, ReusedChunkVisibility);
					UE_LOG(LogTemp, Display, TEXT("Voxel native uniform path: gpuSceneDraws=%d legacyDraws=%d rhiUniforms=%d"),
						GPUSceneDraws, SubmittedDraws - GPUSceneDraws, RHIUniforms);
					if (FirstDrawnChunk)
					{
						UE_LOG(LogTemp, Display, TEXT("Voxel native mesh: material=%s firstVertex=%s origin=%s"),
							*FirstDrawnChunk->Material->GetFriendlyName(),
							*FVector(FirstDrawnChunk->FirstVertex).ToString(),
							*FirstDrawnChunk->LocalToWorld.GetOrigin().ToString());
					}
				}
#endif
			}
		}
	private:
		TSharedPtr<FVoxelTerrainRendererState, ESPMode::ThreadSafe> State;
		mutable double LastDrawDiagnosticSeconds = 0.0;
	};
}

UVoxelTerrainPrimitiveComponent::UVoxelTerrainPrimitiveComponent(const FObjectInitializer& InObjectInitializer)
	: Super(InObjectInitializer)
{
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetCanEverAffectNavigation(false);
	bCastDynamicShadow = true;
}

void UVoxelTerrainPrimitiveComponent::InitializeTerrain(TSharedPtr<FVoxelTerrainRendererState, ESPMode::ThreadSafe> InState)
{
	TerrainState = MoveTemp(InState);
}

UVoxelTerrainPrimitiveComponent::~UVoxelTerrainPrimitiveComponent()
{
	OverrideMaterials.Empty();
	MaterialLease.Reset();
}

int32 UVoxelTerrainPrimitiveComponent::GetNumMaterials() const
{
	return OverrideMaterials.Num();
}

UVoxelTerrainPrimitiveComponent* UVoxelTerrainPrimitiveComponent::DetachMaterialKeeper()
{
	check(IsInGameThread());
	// 只迁移材质与授额；退休保持者不拥有场景组件、Actor 或已结束的 World。
	auto* Keeper = NewObject<UVoxelTerrainPrimitiveComponent>(GetTransientPackage());
	Keeper->OverrideMaterials = MoveTemp(OverrideMaterials);
	Keeper->MaterialLease = MoveTemp(MaterialLease);
	return Keeper;
}

int32 UVoxelTerrainPrimitiveComponent::RetainMaterial(UMaterialInterface* InMaterial, EVoxelTerrainResourceAdmission* OutAdmission)
{
	if (OutAdmission) *OutAdmission = EVoxelTerrainResourceAdmission::InvalidRequest;
	if (!TerrainState || !InMaterial) return INDEX_NONE;
	const int32 Existing = OverrideMaterials.IndexOfByKey(InMaterial);
	if (Existing != INDEX_NONE)
	{
		if (OutAdmission) *OutAdmission = EVoxelTerrainResourceAdmission::Granted;
		return Existing;
	}
	if (OverrideMaterials.Num() == MAX_int32)
	{
		if (OutAdmission) *OutAdmission = EVoxelTerrainResourceAdmission::CapacityUnachievable;
		return INDEX_NONE;
	}
	if (OverrideMaterials.Num() == OverrideMaterials.Max())
	{
		const int32 Count = OverrideMaterials.Num() + 1;
		const uint64 Bytes = VoxelBoundedAllocation::ArrayBytes<TObjectPtr<UMaterialInterface>>(Count);
		FRendererBindingInputFloor Floor;
		if (!Floor.Add(TerrainState->InputLease, TerrainState->Resources.Get()) ||
			!Floor.Add(MaterialLease, TerrainState->Resources.Get())) return INDEX_NONE;
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Candidate;
		const auto Admission = TerrainState->Resources->TryAcquire(TerrainState->ControlConsumer,
			EVoxelTerrainResourcePool::Input, Bytes, Candidate, 0, Floor.Bytes, Floor.LeaseCount);
		if (OutAdmission) *OutAdmission = Admission;
		if (Admission != EVoxelTerrainResourceAdmission::Granted) return INDEX_NONE;
		OverrideMaterials.Reserve(Count);
		check(OverrideMaterials.GetAllocatedSize() <= Candidate->Bytes());
		MaterialLease = MoveTemp(Candidate);
		MaterialLease->MarkPublished();
	}
	if (OutAdmission) *OutAdmission = EVoxelTerrainResourceAdmission::Granted;
	const int32 Index = OverrideMaterials.Num();
	// 使用 UE 原材质入口，让流送、PSO 和已有 SceneProxy 的声明同步失效。
	SetMaterial(Index, InMaterial);
	return Index;
}

FPrimitiveSceneProxy* UVoxelTerrainPrimitiveComponent::CreateSceneProxy()
{
	if (!TerrainState || !TerrainState->bActive.Load()) return nullptr;
	TerrainState->bProxyRecreationPending = false;
	FRendererBindingInputFloor Floor;
	if (!Floor.Add(TerrainState->InputLease, TerrainState->Resources.Get()) ||
		!Floor.Add(MaterialLease, TerrainState->Resources.Get()))
	{
		TerrainState->ProxyAdmission = EVoxelTerrainResourceAdmission::InvalidRequest;
		return nullptr;
	}
	TerrainState->Resources->TryGetReleaseVersionExcludingOwner(TerrainState->ControlConsumer, TerrainState->ProxyCapacityVersion);
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	TerrainState->ProxyAdmission = TerrainState->Resources->TryAcquire(TerrainState->ControlConsumer,
		EVoxelTerrainResourcePool::Input, FVoxelTerrainSceneProxy::AllocationBytes(GetNumMaterials()), Lease, 0, Floor.Bytes, Floor.LeaseCount);
	if (TerrainState->ProxyAdmission != EVoxelTerrainResourceAdmission::Granted) return nullptr;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> MaterialScratch;
	if (!Floor.Add(Lease, TerrainState->Resources.Get()))
	{
		TerrainState->ProxyAdmission = EVoxelTerrainResourceAdmission::InvalidRequest;
		return nullptr;
	}
	const uint64 MaterialScratchBytes = FVoxelTerrainSceneProxy::MaterialListAllocationBytes(GetNumMaterials());
	if (MaterialScratchBytes)
	{
		TerrainState->ProxyAdmission = TerrainState->Resources->TryAcquire(TerrainState->ControlConsumer,
			EVoxelTerrainResourcePool::Working, MaterialScratchBytes, MaterialScratch, 0, Floor.Bytes, Floor.LeaseCount);
		if (TerrainState->ProxyAdmission != EVoxelTerrainResourceAdmission::Granted) return nullptr;
	}
	auto* Proxy = new (Lease.ToSharedRef()) FVoxelTerrainSceneProxy(this);
	Lease->MarkPublished();
	return Proxy;
}

FBoxSphereBounds UVoxelTerrainPrimitiveComponent::CalcBounds(const FTransform& InLocalToWorld) const
{
	// 整体 Primitive 保持稳定，实际视锥裁剪由不可变 Atom 树完成。
	return FBoxSphereBounds(FBox(FVector(-1.0e12), FVector(1.0e12)));
}

bool FVoxelTerrainRenderAtom::HasMatchingFineInputs() const
{
	if (Evidence.Owner.Kind != EVoxelFineOwnerKind::F0 && Evidence.Owner.Kind != EVoxelFineOwnerKind::F1Plus) return true;
	if (!FineGeometry || Evidence.GeometryLease.Get() != FineGeometry.Get() || !FineGeometry->bCoverageValidated ||
		!FineGeometry->Boundary || Boundaries.Num() != 1 || Boundaries[0] != FineGeometry->Boundary) return false;
	const auto& Input = FineGeometry->Attempt.Input;
	const auto& Content = Evidence.Content;
	return FineGeometry->Boundary->Input == Input && Evidence.Owner.Incarnation == Input.Incarnation &&
		Content.WorldEpoch == Input.WorldEpoch && Content.GeneratorIdentity == Input.GeneratorIdentity &&
		Content.RegistryRevision == Input.RegistryRevision && Content.ShapeRevision == Input.ShapeRevision &&
		Content.DataRevision == Input.DataRevision && Content.OverlayRevision == Input.OverlayRevision &&
		Content.MeshRevision == Input.MeshRevision && Content.GeometryIdentity == FineGeometry->GeometryIdentity &&
		Content.SampleStepCells == 1;
}

bool FVoxelTerrainRenderAtom::IsResourceReady() const
{
	if (!Evidence.bGeometryReady || !MetadataLease || !Evidence.GeometryLease || !HasMatchingFineInputs()) return false;
	for (const auto& Chunk : Chunks) if (!Chunk || !Chunk->bReady.Load()) return false;
	for (const auto& Range : DrawRanges) if (!Range.Chunk || !Range.Chunk->bReady.Load()) return false;
	return Evidence.bKnownEmpty || !Chunks.IsEmpty();
}

uint64 FVoxelTerrainRenderAtom::GetAllocatedBytes() const
{
	return VoxelBoundedAllocation::WorkerSharedBytes<FVoxelTerrainRenderAtom>() + Chunks.GetAllocatedSize() +
		Boundaries.GetAllocatedSize() + DrawRanges.GetAllocatedSize();
}

bool FVoxelTerrainRenderCoarse::IsResourceReady() const
{
	if (!SourceId.IsValid() || !Geometry || !MetadataLease || !Evidence.bGeometryReady || !OwnershipBounds.IsValid ||
		SourceChunks.Num() != Geometry->Source->Batches.Num()) return false;
	for (int32 Index = 0; Index < SourceChunks.Num(); ++Index)
	{
		if (!Geometry->Source->Batches[Index].Mesh.Triangles.IsEmpty() && !SourceChunks[Index]) return false;
		int32 End = 0;
		for (auto Chunk = SourceChunks[Index]; Chunk; Chunk = Chunk->NextSourceChunk)
		{
			if (!Chunk->bReady.Load() || Chunk->FirstSourceTriangle != End) return false;
			End += Chunk->SourceTriangleCount;
		}
		if (End != Geometry->Source->Batches[Index].Mesh.Triangles.Num() / 3) return false;
	}

	return true;
}

uint64 FVoxelTerrainRenderCoarse::GetAllocatedBytes() const
{
	return VoxelBoundedAllocation::WorkerSharedBytes<FVoxelTerrainRenderCoarse>() + SourceChunks.GetAllocatedSize() + DrawRoots.GetAllocatedSize();
}

#include "VoxelTerrainRendererNativeState.inl"

FVoxelTerrainRenderer::FVoxelTerrainRenderer(UVoxelModule& InModule, FVoxelTaskScheduler& InScheduler, const uint64 InWorldEpoch)
	: BoundModule(&InModule), BoundWorld(InModule.GetWorld()), BoundScheduler(&InScheduler),
		BoundResources(InScheduler.GetTerrainResources()),
		BootstrapConsumer(FVoxelGenerationConsumerKey::Section(InWorldEpoch, FGuid::NewGuid(), 1, FIntVector(MIN_int32, 11, 0), 1))
{
	EnsureState();
}

FVoxelTerrainRenderer::~FVoxelTerrainRenderer()
{
	Reset();
}

TSharedPtr<const FVoxelTerrainRenderChunk, ESPMode::ThreadSafe> FVoxelTerrainRenderer::PrepareChunk(
	const FVoxelGenerationConsumerKey& InConsumer,
	TSharedPtr<const FVoxelSectionMeshResult, ESPMode::ThreadSafe> InMesh,
	const int32 InBatch, const FVector& InWorldOrigin, const double InWorldScale,
	const int32 InFirstTriangle, const int32 InTriangleCount, EVoxelTerrainResourceAdmission* OutAdmission, const bool bInRetainCpuSource)
{
	if (OutAdmission) *OutAdmission = EVoxelTerrainResourceAdmission::InvalidRequest;
	if (!EnsureState())
	{
		if (OutAdmission) *OutAdmission = GetNativeStateAdmission();
		return nullptr;
	}
	if (!State->bActive.Load() || !State->Component.IsValid() || !InMesh || !InMesh->bValidated ||
		!InMesh->Batches.IsValidIndex(InBatch) || !FMath::IsFinite(InWorldScale) || InWorldScale <= 0.0) return nullptr;
	const FVoxelRenderBatch& Batch = InMesh->Batches[InBatch];
	const FVoxelMeshBuffers& Mesh = Batch.Mesh;
	if (Mesh.Triangles.IsEmpty()) return nullptr;
	const int32 TriangleCount = InTriangleCount == INDEX_NONE ? Mesh.Triangles.Num() / 3 - InFirstTriangle : InTriangleCount;
	if (InFirstTriangle < 0 || TriangleCount <= 0 || int64(InFirstTriangle) + TriangleCount > Mesh.Triangles.Num() / 3) return nullptr;
	const int64 RequiredVertices = int64(TriangleCount) * 3;
	if (RequiredVertices > MAX_int32 / 4)
	{
		if (OutAdmission) *OutAdmission = EVoxelTerrainResourceAdmission::CapacityUnachievable;
		return nullptr;
	}
	const int32 VertexCount = int32(RequiredVertices);
	UMaterialInterface* Material = State->Module.GetMaterialSet()->FindMaterial(Batch.Group, Batch.Bank, Batch.MaterialVariant);
	if (!Material) return nullptr;
	const auto Capacity = FVoxelRenderChunkCapacity::Calculate(VertexCount);
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResidentLease;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> WorkingLease;
	const bool bAllowOverlap = HasPresentedCoverage(FBox(InWorldOrigin / State->CellSize + InMesh->LocalBounds.Min * (InWorldScale / State->CellSize),
		InWorldOrigin / State->CellSize + InMesh->LocalBounds.Max * (InWorldScale / State->CellSize)));
	FRendererBindingInputFloor Inputs;
	if (!Inputs.Add(InMesh->ResourceLease, State->Resources.Get()) ||
		!Inputs.Add(State->InputLease, State->Resources.Get()) ||
		!Inputs.Add(State->Component->GetNativeMaterialLease(), State->Resources.Get()) ||
		!Inputs.Add(State->MetadataLease, State->Resources.Get())) return nullptr;
	auto Admission = TryAcquirePublicationResident(State->Resources, InConsumer, Capacity.Resident, ResidentLease,
		0, bAllowOverlap, Inputs.Bytes, Inputs.LeaseCount);
	if (Admission == EVoxelTerrainResourceAdmission::Granted)
	{
		verify(Inputs.Add(ResidentLease, State->Resources.Get()));
		Admission = State->Resources->TryAcquire(InConsumer, EVoxelTerrainResourcePool::Working, Capacity.Working,
			WorkingLease, 0, Inputs.Bytes, Inputs.LeaseCount);
	}
	if (OutAdmission) *OutAdmission = Admission;
	if (Admission != EVoxelTerrainResourceAdmission::Granted) return nullptr;
	if (State->Component->RetainMaterial(Material, OutAdmission) == INDEX_NONE) return nullptr;
	auto Chunk = TSharedPtr<FVoxelTerrainRenderChunk, ESPMode::ThreadSafe>(
		new FVoxelTerrainRenderChunk(State->Module.GetWorld()->GetFeatureLevel()),
		FVoxelRenderChunkDeleter{ResidentLease});
	if (bInRetainCpuSource) Chunk->Source = InMesh;
	Chunk->FirstSourceTriangle = InFirstTriangle;
	Chunk->SourceBatch = InBatch;
	Chunk->SourceTriangleCount = TriangleCount;
	Chunk->ResidentLease = MoveTemp(ResidentLease);
	Chunk->Material = Material->GetRenderProxy();
	Chunk->Relevance = Material->GetRelevance_Concurrent(State->Module.GetWorld()->GetFeatureLevel());
	Chunk->LocalToWorld = FTransform(FQuat::Identity, InWorldOrigin).ToMatrixWithScale();
	Chunk->LocalBounds = FBox(InMesh->LocalBounds.Min * InWorldScale, InMesh->LocalBounds.Max * InWorldScale);
	Chunk->WorldBounds = FBox(InWorldOrigin + Chunk->LocalBounds.Min, InWorldOrigin + Chunk->LocalBounds.Max);
	FVector UniformMin = Chunk->WorldBounds.Min;
	FVector UniformMax = Chunk->WorldBounds.Max;
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		if (UniformMin[Axis] == 0.0) UniformMin[Axis] = 0.0;
		if (UniformMax[Axis] == 0.0) UniformMax[Axis] = 0.0;
	}
	Chunk->PrimitiveUniformHash = HashCombineFast(GetTypeHash(UniformMin), GetTypeHash(UniformMax));
	ENQUEUE_RENDER_COMMAND(VoxelPrepareAtomChunk)(FVoxelPrepareRenderChunkCommand{
		MoveTemp(WorkingLease), Chunk, InMesh, InWorldScale, InBatch, InFirstTriangle, TriangleCount, VertexCount});
	return Chunk;
}

void FVoxelTerrainRenderer::AppendSourceChunk(FVoxelTerrainRenderCoarse& InItem, const int32 InBatch,
	TSharedPtr<const FVoxelTerrainRenderChunk, ESPMode::ThreadSafe>& InOutLast,
	TSharedPtr<const FVoxelTerrainRenderChunk, ESPMode::ThreadSafe> InChunk)
{
	check(IsInGameThread() && InItem.SourceChunks.IsValidIndex(InBatch) && InChunk && !InItem.Patch);
	if (!InOutLast)
	{
		check(!InItem.SourceChunks[InBatch] && InChunk->FirstSourceTriangle == 0);
		InItem.SourceChunks[InBatch] = InChunk;
	}
	else
	{
		check(!InOutLast->NextSourceChunk && InOutLast->FirstSourceTriangle + InOutLast->SourceTriangleCount == InChunk->FirstSourceTriangle);
		ConstCastSharedPtr<FVoxelTerrainRenderChunk>(InOutLast)->NextSourceChunk = InChunk;
	}
	InOutLast = MoveTemp(InChunk);
}

bool FVoxelTerrainRenderer::PrepareAtomTransaction(const TConstArrayView<FAtomPtr> InAtoms,
	const uint64 InDemandRevision, const bool bInRequiresCollision,
	TFunction<bool(const FIntVector&, const FVoxelFineOwnerEvidence&)> InIsCurrent,
	TFunction<bool(const FIntVector&, const FVoxelFineOwnerEvidence&)> InIsCollisionCurrent,
	TFunction<void()> InOnSubmitted, FGuid& OutTransaction,
	const FVoxelTerrainServicePriority& InPriority, const double InFirstQueuedSeconds,
	EVoxelTerrainResourceAdmission* OutAdmission, const FVoxelGenerationConsumerKey& InConsumer)
{
	if (OutAdmission) *OutAdmission = EVoxelTerrainResourceAdmission::Granted;
	const auto Reject = [&](const EVoxelTerrainResourceAdmission Admission)
	{
		if (OutAdmission) *OutAdmission = Admission;
		return false;
	};
	if (!EnsureState()) return Reject(GetNativeStateAdmission());
	if (!State->bActive.Load() || InAtoms.IsEmpty() || InAtoms.Num() > 8 || !InIsCurrent || !InIsCollisionCurrent)
		return Reject(EVoxelTerrainResourceAdmission::InvalidRequest);
	if (State->NextMetadataSerial == MAX_uint64) return Reject(EVoxelTerrainResourceAdmission::CapacityUnachievable);
	if (!State->PrepareMetadata()) return Reject(State->MetadataAdmission);
	const int32 QueueLimit = FVoxelTerrainWorkSelection::IsNear(InPriority.Band) ? VoxelNativePublicationCapacity :
		VoxelNativePublicationCapacity - VoxelNativeNearPublicationReserve;
	if (State->Ready.Num() + State->SubmittedCallbacks.Num() >= QueueLimit) return Reject(EVoxelTerrainResourceAdmission::CapacityBlocked);
	for (const FAtomPtr& Atom : InAtoms)
	{
		if (!Atom || !Atom->HasMatchingFineInputs()) return Reject(EVoxelTerrainResourceAdmission::InvalidRequest);
		if (!Atom->IsResourceReady() || State->Handoff.IsLocked(Atom->Section) ||
			Atom->MetadataLease->Bytes() < Atom->GetAllocatedBytes()) return false;
	}
	const uint64 MetadataBytes = VoxelBoundedAllocation::WorkerSharedBytes<FAtomPublication>() +
		VoxelBoundedAllocation::ArrayBytes<FAtomPtr>(InAtoms.Num()) +
		3 * VoxelBoundedAllocation::ArrayBytes<FVoxelFineOwnershipChange>(InAtoms.Num()) +
		VoxelBoundedAllocation::SharedBytes<FVoxelFineFrozenHandoff>() + sizeof(FReadyTransaction) +
		VoxelBoundedAllocation::WorkerSharedBytes<FPresentedSnapshot>();
	const auto Consumer = InConsumer.IsValid() ? InConsumer : FVoxelGenerationConsumerKey::Section(State->Epoch, State->OwnerId,
		1, InAtoms[0]->Section, State->NextMetadataSerial + 1);
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	FRendererBindingInputFloor Floor;
	if (!Floor.Add(State->InputLease, State->Resources.Get())) return Reject(EVoxelTerrainResourceAdmission::InvalidRequest);
	if (State->Component.IsValid() && !Floor.Add(State->Component->GetNativeMaterialLease(), State->Resources.Get()))
		return Reject(EVoxelTerrainResourceAdmission::InvalidRequest);
	for (const auto& Atom : InAtoms)
		if (!Floor.Add(Atom->MetadataLease, State->Resources.Get())) return Reject(EVoxelTerrainResourceAdmission::InvalidRequest);
	const bool bAllowOverlap = State->PresentedSnapshot && State->LastSealedSnapshot;
	const auto Admission = TryAcquirePublicationResident(State->Resources, Consumer, MetadataBytes, Lease, 1, bAllowOverlap,
		Floor.Bytes, Floor.LeaseCount);
	if (Admission != EVoxelTerrainResourceAdmission::Granted) return Reject(Admission);
	auto Package = VoxelBoundedAllocation::MakeWorkerShared<FAtomPublication>(Lease);
	Package->Lease = MoveTemp(Lease);
	Package->Atoms.Reserve(InAtoms.Num());
	TArray<FVoxelFineOwnershipChange, TInlineAllocator<8>> Changes;
	for (const FAtomPtr& Atom : InAtoms)
	{
		Package->Atoms.Add(Atom);
		FVoxelFineOwnershipChange Change;
		Change.Section = Atom->Section;
		if (const auto* Existing = State->Handoff.FindPresentedAtom(Atom->Section))
		{
			Change.Before = Existing->Evidence.Owner;
			Change.BeforeGeneration = Existing->OwnerGeneration;
		}
		Change.After = Atom->Evidence;
		Change.bRequiresCollision = bInRequiresCollision;
		Changes.Add(MoveTemp(Change));
	}
	FVoxelFineHandoffTransaction Transaction;
	FString Error;
	if (!State->Handoff.Prepare(Changes, InDemandRevision, Transaction, Error, Package->Lease))
	{
		if (Error.StartsWith(TEXT("CapacityBlocked"))) return Reject(EVoxelTerrainResourceAdmission::CapacityBlocked);
		if (Error.StartsWith(TEXT("CapacityUnachievable"))) return Reject(EVoxelTerrainResourceAdmission::CapacityUnachievable);
		return Reject(EVoxelTerrainResourceAdmission::InvalidRequest);
	}
	Package->Transaction = Transaction.Id;
	FReadyTransaction Ready;
	Ready.Id = Transaction.Id;
	Ready.Priority = InPriority;
	if (!FMath::IsFinite(Ready.Priority.DistanceCells) || Ready.Priority.DistanceCells == MAX_dbl) Ready.Priority.DistanceCells = 0.0;
	Ready.FirstQueuedSeconds = InFirstQueuedSeconds >= 0.0 ? InFirstQueuedSeconds : FPlatformTime::Seconds();
	Ready.Sequence = ++State->NextMetadataSerial;
	Ready.Resources = MoveTemp(Package);
	Ready.IsCurrent = MoveTemp(InIsCurrent);
	Ready.IsCollisionCurrent = MoveTemp(InIsCollisionCurrent);
	Ready.OnSubmitted = MoveTemp(InOnSubmitted);
	State->InsertReady(MoveTemp(Ready));
	OutTransaction = Transaction.Id;
	return true;
}

#include "VoxelTerrainRendererCoarse.inl"

bool FVoxelTerrainRenderer::IsTransactionPending(const FGuid& InTransaction) const
{
	return State && (State->SubmittedCallbacks.Contains(InTransaction) ||
		State->Ready.ContainsByPredicate([&InTransaction](const FReadyTransaction& Ready)
		{ return Ready.Id == InTransaction && !Ready.bTerminalFailure; }));
}

bool FVoxelTerrainRenderer::ConsumeTransactionFailure(const FGuid& InTransaction)
{
	check(IsInGameThread());
	if (!State) return false;
	const int32 Index = State->Ready.IndexOfByPredicate([&InTransaction](const FReadyTransaction& Ready)
	{
		return Ready.Id == InTransaction && Ready.bTerminalFailure;
	});
	if (Index == INDEX_NONE) return false;
	State->AdvanceCoarseAdmissionVersion();
	State->ReleasePreparation(State->Ready[Index]);
	State->RemoveReady(Index);
	return true;
}

bool FVoxelTerrainRendererState::BeginRootPreparation(FReadyTransaction& InTransaction)
{
	if (InTransaction.bTerminalFailure) return false;
	if (!InTransaction.Preparation && ActivePreparations.Num() >= VoxelNativeRootPreparationConcurrency) return false;
	if (!InTransaction.PreparationConsumer.IsValid())
	{
		if (NextMetadataSerial == MAX_uint64) { TerminateTransaction(InTransaction); return false; }
		InTransaction.PreparationConsumer = FVoxelGenerationConsumerKey::Section(Epoch, OwnerId, 1, {}, ++NextMetadataSerial);
	}
	const auto Consumer = InTransaction.PreparationConsumer;
	if (InTransaction.bPreparationAdmissionWaiting &&
		!RendererHasExternalRelease(Resources, Consumer, InTransaction.PreparationAdmissionVersion)) return false;
	InTransaction.bPreparationAdmissionWaiting = false;
	const uint64 BeforeAdmission = Resources->ReleaseVersionExcludingOwner(Consumer);
	const auto RejectAdmission = [&](const bool bTerminal)
	{
		if (bTerminal) TerminateTransaction(InTransaction);
		else
		{
			InTransaction.PreparationAdmissionVersion = BeforeAdmission;
			InTransaction.bPreparationAdmissionWaiting = true;
		}
	};
	if (!InTransaction.Preparation)
	{
		const int32 ChangedCount = InTransaction.Resources->Atoms.Num() + InTransaction.Resources->CoarseChanges.Num();
		if (ChangedCount <= 0 || ChangedCount > 8200) { RejectAdmission(true); return false; }
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> PathLease;
		const uint64 PathBytes = 33ull * InTransaction.Resources->Atoms.Num() *
			VoxelBoundedAllocation::WorkerSharedBytes<FAtomTreeNode>() +
			79ull * InTransaction.Resources->CoarseChanges.Num() *
				VoxelBoundedAllocation::WorkerSharedBytes<FCoarseTreeNode>();
		const int32 Splits = 33 * InTransaction.Resources->Atoms.Num() + 79 * InTransaction.Resources->CoarseChanges.Num();
		const bool bAllowOverlap = PresentedSnapshot && LastSealedSnapshot;
		FRendererBindingInputFloor InputFloor;
		if (!InputFloor.Add(InputLease, Resources.Get()) || !InputFloor.Add(InTransaction.Resources->Lease, Resources.Get()) ||
			!InputFloor.Add(InTransaction.Resources->CoarseCallbackLease, Resources.Get()))
		{ RejectAdmission(true); return false; }
		const auto InputAdmission = Resources->TryAcquire(Consumer, EVoxelTerrainResourcePool::Input,
			RenderRootAllocationBytes<FRootPreparation>() + VoxelBoundedAllocation::ArrayBytes<FBox>(ChangedCount),
			Lease, 0, InputFloor.Bytes, InputFloor.LeaseCount);
		if (InputAdmission != EVoxelTerrainResourceAdmission::Granted)
		{
			RejectAdmission(InputAdmission != EVoxelTerrainResourceAdmission::CapacityBlocked);
			return false;
		}
		if (!InputFloor.Add(Lease, Resources.Get())) { RejectAdmission(true); return false; }
		const auto PathAdmission = TryAcquirePublicationResident(Resources, Consumer, PathBytes, PathLease,
			Splits, bAllowOverlap, InputFloor.Bytes, InputFloor.LeaseCount);
		if (PathAdmission != EVoxelTerrainResourceAdmission::Granted)
		{
			RejectAdmission(PathAdmission != EVoxelTerrainResourceAdmission::CapacityBlocked);
			return false;
		}
		InTransaction.Preparation = CreateRenderRoot<FRootPreparation>(MoveTemp(Lease), Resources, Consumer, ChangedCount);
		InTransaction.Preparation->ActiveIndex = ActivePreparations.Add(InTransaction.Preparation.Get());
		InTransaction.Preparation->SchedulingBand = InTransaction.Priority.Band;
		check(RenderRootAllocationBytes<FRootPreparation>() + InTransaction.Preparation->ChangedBounds.GetAllocatedSize() <=
			InTransaction.Preparation->InputLease->Bytes());
		InTransaction.Preparation->ControlInputLease = InputLease;
		InTransaction.Preparation->PrimaryPathLease = MoveTemp(PathLease);
		InTransaction.Preparation->Publication = InTransaction.Resources;
		InTransaction.Preparation->BaseSnapshot = LastSealedSnapshot;
		InTransaction.Preparation->Root = LastSealedSnapshot ? LastSealedSnapshot->Root : nullptr;
		InTransaction.Preparation->CoarseRoot = LastSealedSnapshot ? LastSealedSnapshot->CoarseRoot : nullptr;
		InTransaction.Preparation->CellSize = Module.BlockSize();
		InTransaction.Preparation->bAllowResidentOverlap = bAllowOverlap;
		InTransaction.Preparation->MaximumTextureStretchCells = Module.GetViewSettings().MaximumTextureStretchCells;
		InTransaction.Preparation->MeshPieceLimit = uint64(Module.GetViewSettings().Fine.MaximumMeshPieceMegabytes) * 1024 * 1024;
		InTransaction.Preparation->GeometryLimit = uint64(Module.GetViewSettings().Fine.MaximumShardGeometryMegabytes) * 1024 * 1024;
		InTransaction.Preparation->Registry = Module.GetRegistry();
		InTransaction.Preparation->Shapes = Module.GetShapes();
		InTransaction.Preparation->Config = Module.GetGenerationConfig();
		if (!InTransaction.Preparation->Registry || !InTransaction.Preparation->Shapes || !InTransaction.Preparation->Config)
		{ TerminateTransaction(InTransaction); return false; }
	}
	const auto Work = InTransaction.Preparation;
	if (Work->bEnqueued) return Work->bUploadComplete;
	FVoxelTaskRequest Request;
	Request.Kind = EVoxelTaskKind::BuildViewCoverage;
	Request.Stamp.OwnerId = OwnerId;
	Request.Stamp.OwnerIncarnation = 1;
	Request.Stamp.WorldEpoch = Epoch;
	Request.Stamp.Token = Work->Consumer.RequestSerial;
	Request.Stamp.Section = FIntVector(MIN_int32, 8, 0);
	Request.WorkClass = InTransaction.Priority.Band == EVoxelTerrainServiceBand::P0 ? EVoxelWorkClass::Critical : EVoxelWorkClass::Visible;
	Request.ServiceBand = InTransaction.Priority.Band;
	Request.SourceStableKey = InTransaction.Priority.SourceStableKey;
	Request.SourcePriority = InTransaction.Priority.SourcePriority;
	Request.DistanceScore = InTransaction.Priority.DistanceCells;
	Request.ForwardScore = InTransaction.Priority.ForwardScore;
	Request.bResumable = Request.bPublicationContinuation = true;
	Request.InputBytes = Work->InputLease->Bytes();
	Request.GenerationConsumer = Work->Consumer;
	FRendererBindingInputFloor BindingInput;
	if (!BindingInput.Add(Work->InputLease, Resources.Get()) ||
		!BindingInput.Add(Work->ControlInputLease, Resources.Get()) ||
		!BindingInput.Add(Work->PrimaryPathLease, Resources.Get()) ||
		!BindingInput.Add(Work->Publication->Lease, Resources.Get()))
	{
		RejectAdmission(true);
		return false;
	}
	Request.SetRequiredBindingInput(BindingInput.Bytes, BindingInput.LeaseCount);
	Request.ReservedBytes = 1;
	Request.FirstRunnableSeconds = InTransaction.FirstQueuedSeconds;
	{
		FString CallbackBindingError;
		if (!Request.BindGetBlockReason(Scheduler.GetTerrainResources(), (Request.GenerationConsumer.IsValid() ? Request.GenerationConsumer : Scheduler.GetControlConsumer()),
			[Work]
	{
		return Work->bWaitingCapacity.Load() && !RendererHasExternalRelease(Work->Resources, Work->Consumer, Work->CapacityVersion.Load()) ?
			EVoxelTaskBlockReason::Memory : EVoxelTaskBlockReason::None;
	}, CallbackBindingError))
		{
			Scheduler.ObserveDemand(Request.Kind, Request.Stamp,
				CallbackBindingError.StartsWith(TEXT("Oversized")) || CallbackBindingError.StartsWith(TEXT("CapacityUnachievable"))
					? EVoxelTaskBlockReason::Oversized : EVoxelTaskBlockReason::InputMemory);
			RejectAdmission(!CallbackBindingError.StartsWith(TEXT("CapacityBlocked")));
			return false;
		}
	}
	{
		FString CallbackBindingError;
		if (!Request.BindExecute(Scheduler.GetTerrainResources(), (Request.GenerationConsumer.IsValid() ? Request.GenerationConsumer : Scheduler.GetControlConsumer()),
			[Work](const TAtomic<bool>& Cancel) { return Work->Advance(Cancel); }, CallbackBindingError))
		{
			Scheduler.ObserveDemand(Request.Kind, Request.Stamp,
				CallbackBindingError.StartsWith(TEXT("Oversized")) || CallbackBindingError.StartsWith(TEXT("CapacityUnachievable"))
					? EVoxelTaskBlockReason::Oversized : EVoxelTaskBlockReason::InputMemory);
			RejectAdmission(!CallbackBindingError.StartsWith(TEXT("CapacityBlocked")));
			return false;
		}
	}
	TWeakPtr<FVoxelTerrainRendererState, ESPMode::ThreadSafe> WeakState = AsShared();
	{
		FString CallbackBindingError;
		if (!Request.BindApply(Scheduler.GetTerrainResources(), (Request.GenerationConsumer.IsValid() ? Request.GenerationConsumer : Scheduler.GetControlConsumer()),
			[WeakState, Work](FVoxelTaskResult&& Result)
	{
		const auto Pinned = WeakState.Pin();
		if (!Pinned || !Pinned->bActive.Load()) return;
		const int32 Index = Pinned->Ready.IndexOfByPredicate([&Work](const FReadyTransaction& Pending) { return Pending.Preparation == Work; });
		if (Index == INDEX_NONE) return;
		if (!Result.bSuccess || Result.bCanceled || Result.Stamp.WorldEpoch != Pinned->Epoch ||
			Result.Stamp.OwnerId != Pinned->OwnerId || Result.Stamp.OwnerIncarnation != 1 || Result.Stamp.Token != Work->Consumer.RequestSerial)
		{
			if (Pinned->Scheduler.IsTerminal({Result.Kind, Result.Stamp}) ||
				Result.Error.StartsWith(TEXT("Oversized")) || Result.Error.StartsWith(TEXT("CapacityUnachievable")))
			{
				Pinned->TerminateTransaction(Pinned->Ready[Index]);
				return;
			}
			TOptional<FVoxelFineHandoffTransaction> Canceled;
			Pinned->Handoff.RequestCancel(Work->Publication->Transaction, Canceled);
			Pinned->AdvanceCoarseAdmissionVersion();
			for (const auto& Change : Work->Publication->CoarseChanges) Pinned->PendingCoarseSources.Remove(Change.Item->SourceId);
			Pinned->ReleasePreparation(Pinned->Ready[Index]);
			Pinned->AdvanceCoarseAdmissionVersion();
			Pinned->RemoveReady(Index);
			return;
		}
		Work->bDecodeComplete = true;
		Work->Publication->PreparationReceipt = Result.WorkReceipt;
		Work->Upload = Work->FirstPrepared;
		Work->FineUpload = Work->FirstFine;
	}, CallbackBindingError))
		{
			Scheduler.ObserveDemand(Request.Kind, Request.Stamp,
				CallbackBindingError.StartsWith(TEXT("Oversized")) || CallbackBindingError.StartsWith(TEXT("CapacityUnachievable"))
					? EVoxelTaskBlockReason::Oversized : EVoxelTaskBlockReason::InputMemory);
			RejectAdmission(!CallbackBindingError.StartsWith(TEXT("CapacityBlocked")));
			return false;
		}
	}
	Work->bEnqueued = Scheduler.Enqueue(MoveTemp(Request));
	if (!Work->bEnqueued) RejectAdmission(RendererIsTerminalAdmission(Request.Blocked));
	return false;
}

void FVoxelTerrainRendererState::AdvanceRootUpload(FVoxelTerrainRenderer& Renderer, const double InDeadline)
{
	const double Deadline = FMath::Min(InDeadline, Module.GetTerrainFrameContext().Deadline());
	TArray<FReadyTransaction*, TInlineAllocator<VoxelNativeRootPreparationConcurrency>> Candidates;
	TArray<FReadyTransaction*, TInlineAllocator<VoxelNativeRootPreparationConcurrency>> NextRound;
	for (auto& Pending : Ready)
	{
		if (!Pending.Preparation) continue;
		const auto& Work = *Pending.Preparation;
		if (Work.UploadAdmission != EVoxelTerrainResourceAdmission::Granted &&
			Work.UploadAdmission != EVoxelTerrainResourceAdmission::CapacityBlocked)
		{
			TerminateTransaction(Pending);
			continue;
		}
		if (Work.bDecodeComplete && !Work.bUploadComplete &&
			(Work.UploadAdmission == EVoxelTerrainResourceAdmission::Granted ||
				RendererHasExternalRelease(Resources, Work.Consumer, Work.UploadCapacityVersion))) Candidates.Add(&Pending);
	}
	while (!Candidates.IsEmpty() && FPlatformTime::Seconds() < Deadline)
	{
		FVoxelTerrainServiceCandidateSet CandidateSet;
		for (int32 Index = 0; Index < Candidates.Num(); ++Index)
		{
			const auto& Pending = *Candidates[Index];
			CandidateSet.Add({Index, Pending.Priority, Pending.FirstQueuedSeconds, Pending.Sequence, UploadCostMicroseconds, true});
		}
		FVoxelTerrainServiceCapacity Capacity;
		Capacity.bAllowOverdueOpportunity = Module.GetTerrainFrameContext().IsOverdueOpportunityAvailable();
		Capacity.MaximumWaitSeconds = Module.GetViewSettings().Fine.MaximumBackgroundWaitSeconds;
		FVoxelTerrainServiceCharge Charge;
		const int32 Index = UploadSelection.Select(CandidateSet.GetCandidates(), Capacity, FPlatformTime::Seconds(), Charge);
		if (Index == INDEX_NONE) break;
		if (FPlatformTime::Seconds() >= Deadline)
		{
			verify(UploadSelection.RefundBeforeStart(Charge.Serial));
			break;
		}
		if (Charge.bOverdueOpportunity) Module.GetTerrainFrameContext().ConsumeOverdueOpportunity();
		const double Started = FPlatformTime::Seconds();
		// 先给每个可运行事务一个时间片；仍能推进的事务再共享本帧剩余额度。
		// 容量或 RT 等待立即退出该事务，不重轮询、不延长统一截止时间。
		if (AdvanceRootUpload(Renderer, *Candidates[Index]->Preparation, MAX_int32, FMath::Min(Deadline, Started + 0.00025)))
			NextRound.Add(Candidates[Index]);
		Candidates.RemoveAtSwap(Index, 1, EAllowShrinking::No);
		const double Actual = FMath::Max(1.0, (FPlatformTime::Seconds() - Started) * 1000000.0);
		verify(UploadSelection.Settle(Charge.Serial, Actual));
		UploadCostMicroseconds = FMath::Lerp(UploadCostMicroseconds, Actual, 0.15);
		if (Candidates.IsEmpty())
		{
			Candidates = MoveTemp(NextRound);
			NextRound.Reset();
		}
	}
}

bool FVoxelTerrainRendererState::AdvanceRootUpload(FVoxelTerrainRenderer& Renderer, FRootPreparation& Work,
	const int32 InMaximumSteps, const double InDeadline)
{
	const double Deadline = FMath::Min(InDeadline, Module.GetTerrainFrameContext().Deadline());
	if (Work.UploadAdmission == EVoxelTerrainResourceAdmission::CapacityBlocked &&
		!RendererHasExternalRelease(Work.Resources, Work.Consumer, Work.UploadCapacityVersion)) return false;
	if (Work.UploadAdmission != EVoxelTerrainResourceAdmission::Granted &&
		Work.UploadAdmission != EVoxelTerrainResourceAdmission::CapacityBlocked) return false;
	Work.UploadAdmission = EVoxelTerrainResourceAdmission::Granted;
	const auto ObserveAdmission = [&](const EVoxelTerrainResourceAdmission Admission, const uint64 Before)
	{
		Work.UploadAdmission = Admission;
		Work.UploadCapacityVersion = Before;
		if (Admission != EVoxelTerrainResourceAdmission::Granted)
		{
			FVoxelTaskStamp Stamp;
			Stamp.OwnerId = OwnerId;
			Stamp.OwnerIncarnation = 1;
			Stamp.WorldEpoch = Epoch;
			Stamp.Section = FIntVector(MIN_int32, 8, 0);
			Stamp.Token = Work.Consumer.RequestSerial;
			Scheduler.ObserveDemand(EVoxelTaskKind::BuildViewCoverage, Stamp,
				Admission == EVoxelTerrainResourceAdmission::CapacityBlocked ? EVoxelTaskBlockReason::Memory :
				Admission == EVoxelTerrainResourceAdmission::CapacityUnachievable ? EVoxelTaskBlockReason::Oversized :
				EVoxelTaskBlockReason::TerminalFailure);
		}
		return Admission == EVoxelTerrainResourceAdmission::Granted;
	};
	const auto Transfer = [&](const TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& Lease)
	{
		const uint64 Before = Work.Resources->ReleaseVersionExcludingOwner(Work.Consumer);
		return ObserveAdmission(TryTransferPublicationLease(Work.Resources.Get(), Lease, Work.bAllowResidentOverlap), Before);
	};
	const auto CompleteDraw = [&Work](TSharedPtr<const FVoxelTerrainCoarseDrawNode, ESPMode::ThreadSafe> Node)
	{
		Work.DrawWalk.Pop(EAllowShrinking::No);
		if (Work.DrawWalk.IsEmpty()) Work.Upload->Item->DrawRoots[Work.UploadRoot++] = MoveTemp(Node);
		else Work.DrawWalk.Last().Result->Children[Work.DrawWalk.Last().Child - 1] = MoveTemp(Node);
	};
	int32 Step = 0;
	for (; Step < InMaximumSteps && FPlatformTime::Seconds() < Deadline; ++Step)
	{
		if (Work.FineUpload)
		{
			auto& Fine = *Work.FineUpload;
			// Reused sections retain their sealed draw ranges and still participate in merge input validation.
			if (Fine.bReuseNativeInterfaces) { Work.FineUpload = Fine.Next; continue; }
			if (Fine.Upload)
			{
				const auto Mesh = Fine.Upload->Mesh;
				if (!Mesh->Batches.IsValidIndex(Fine.UploadBatch))
				{ Fine.Upload = Fine.Upload->Next; Fine.UploadBatch = Fine.UploadTriangle = 0; continue; }
				const int32 Triangles = Mesh->Batches[Fine.UploadBatch].Mesh.Triangles.Num() / 3;
				if (Fine.UploadTriangle == Triangles)
				{ ++Fine.UploadBatch; Fine.UploadTriangle = 0; continue; }
				const int32 Required = Fine.Item->DrawRanges.Num() + 1;
				if (Fine.Item->DrawRanges.Max() < Required)
				{
					const int32 Capacity = Required;
					const uint64 NewBytes = VoxelBoundedAllocation::ArrayBytes<FVoxelTerrainAtomDrawRange>(Capacity);
					const uint64 Before = Work.Resources->ReleaseVersionExcludingOwner(Work.Consumer);
					const uint64 RequiredInput = Work.InputLease && Work.InputLease->GetPool() == EVoxelTerrainResourcePool::Input
						? Work.InputLease->Bytes() : 0;
					if (!ObserveAdmission(Fine.Item->MetadataLease->TryResize(Fine.Item->GetAllocatedBytes() + NewBytes,
						RequiredInput, Work.InputLease ? 1 : 0), Before)) return false;
					Fine.Item->DrawRanges.Reserve(Capacity);
					check(Fine.Item->DrawRanges.GetAllocatedSize() <= NewBytes);
					verify(Fine.Item->MetadataLease->Resize(Fine.Item->GetAllocatedBytes()));
				}
				if (!Transfer(Mesh->ResourceLease)) return false;
				const int32 Count = FMath::Min(4096, Triangles - Fine.UploadTriangle);
				EVoxelTerrainResourceAdmission ChunkAdmission = EVoxelTerrainResourceAdmission::None;
				const uint64 Before = Work.Resources->ReleaseVersionExcludingOwner(Work.Consumer);
				auto Chunk = Renderer.PrepareChunk(Work.Consumer, Mesh, Fine.UploadBatch,
					Fine.Upload->Origin * Work.CellSize, Fine.Upload->Scale * Work.CellSize, Fine.UploadTriangle, Count, &ChunkAdmission, false);
				if (!Chunk) { ObserveAdmission(ChunkAdmission, Before); return false; }
				Fine.Item->DrawRanges.Add({MoveTemp(Chunk), 0, Count});
				Fine.UploadTriangle += Count;
				continue;
			}
			// 先建立尚未封存的局部根；全部真实 chunk 的就绪在末尾统一核对。
			if (!Transfer(Fine.Item->MetadataLease)) return false;
			Work.Root = SetAtom(Work.Root, Fine.Item, 31, Fine.PathLease, Work.CellSize);
			Work.FineUpload = Fine.Next;
			continue;
		}
		if (!Work.Upload)
		{
			if (!Work.bUploadReadinessStarted)
			{
				Work.ReadinessFine = Work.FirstFine;
				Work.ReadinessCoarse = Work.FirstPrepared;
				Work.bUploadReadinessStarted = true;
				continue;
			}
			if (Work.ReadinessFine)
			{
				// 保留原 Fine 身份、边界和每个实际 chunk 的完整门禁。
				const auto& Fine = *Work.ReadinessFine;
				const FAtomPtr Atom = Fine.bReuseNativeInterfaces ? Fine.Base : FAtomPtr(Fine.Item);
				if (!Atom || !Atom->IsResourceReady()) return false;
				Work.ReadinessFine = Work.ReadinessFine->Next;
				continue;
			}
			if (Work.ReadinessCoarse)
			{
				const auto& Item = *Work.ReadinessCoarse->Item;
				if (!Work.ReadinessDrawWalk.IsEmpty())
				{
					auto& Walk = Work.ReadinessDrawWalk.Last();
					if (Walk.Child == -1)
					{
						Walk.Piece = Walk.Node->FirstPiece;
						Walk.Child = 0;
						continue;
					}
					if (Walk.Piece)
					{
						if (!Walk.Piece->Chunk || !Walk.Piece->Chunk->bReady.Load()) return false;
						Walk.Piece = Walk.Piece->Next;
						continue;
					}
					if (Walk.Child < 2)
					{
						const auto Child = Walk.Node->Children[Walk.Child++];
						if (Child)
						{
							check(Work.ReadinessDrawWalk.Num() < 64);
							Work.ReadinessDrawWalk.Add({Child});
						}
						continue;
					}
					Work.ReadinessDrawWalk.Pop(EAllowShrinking::No);
					continue;
				}
				// 原始 Source chunk 与裁切 Draw chunk 均必须实际初始化完毕。
				if (Work.ReadinessRoot == 0 && !Item.IsResourceReady()) return false;
				if (Item.DrawRoots.IsValidIndex(Work.ReadinessRoot))
				{
					const auto Root = Item.DrawRoots[Work.ReadinessRoot++];
					if (Root) Work.ReadinessDrawWalk.Add({Root});
					continue;
				}
				Work.ReadinessCoarse = Work.ReadinessCoarse->Next;
				Work.ReadinessRoot = 0;
				continue;
			}
			Work.bUploadComplete = true;
			return false;
		}
		auto& Item = *Work.Upload->Item;
		if (Work.DrawWalk.IsEmpty())
		{
			if (Work.UploadRoot == Item.DrawRoots.Num())
			{
				if (!Transfer(Item.MetadataLease) || !Transfer(Item.Patch->MetadataLease)) return false;
				Work.CoarseRoot = SetCoarse(Work.CoarseRoot, Item.Geometry->Stamp.GeometryIdentity.Key, Item.SourceId,
					Work.Upload->Item, 77, Work.Upload->PathLease);
				Work.Upload = Work.Upload->Next;
				Work.UploadRoot = 0;
				continue;
			}
			const auto Base = Work.Upload->Base->DrawRoots.IsValidIndex(Work.UploadRoot) ? Work.Upload->Base->DrawRoots[Work.UploadRoot] : nullptr;
			Work.DrawWalk.Add({Item.Patch->Roots[Work.UploadRoot], Base});
			continue;
		}
		auto& Walk = Work.DrawWalk.Last();
		if (!Walk.Patch) { CompleteDraw(nullptr); continue; }
		if (Walk.Base && Walk.Base->Patch == Walk.Patch) { CompleteDraw(Walk.Base); continue; }
		if (!Walk.Result)
		{
			TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
			const uint64 Before = Work.Resources->ReleaseVersionExcludingOwner(Work.Consumer);
			if (!ObserveAdmission(TryAcquirePublicationResident(Resources, Work.Consumer,
				VoxelBoundedAllocation::WorkerSharedBytes<FVoxelTerrainCoarseDrawNode>(), Lease, 0, Work.bAllowResidentOverlap), Before) ||
				!Transfer(Walk.Patch->MetadataLease)) return false;
			Walk.Result = VoxelBoundedAllocation::MakeWorkerShared<FVoxelTerrainCoarseDrawNode>(Lease);
			Walk.Result->Lease = MoveTemp(Lease);
			Walk.Result->Patch = Walk.Patch;
			Walk.Piece = Walk.Patch->FirstPiece;
		}
		const auto& Range = Item.Geometry->Ranges[Walk.Patch->Range];
		if (!Range.IsLeaf() && Walk.Child < 2)
		{
			const int32 Child = Walk.Child++;
			Work.DrawWalk.Add({Walk.Patch->Children[Child], Walk.Base ? Walk.Base->Children[Child] : nullptr});
			continue;
		}
		if (Walk.Piece)
		{
			TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
			const uint64 Before = Work.Resources->ReleaseVersionExcludingOwner(Work.Consumer);
			if (!ObserveAdmission(TryAcquirePublicationResident(Resources, Work.Consumer,
				VoxelBoundedAllocation::WorkerSharedBytes<FVoxelTerrainCoarseDrawPiece>(), Lease, 0, Work.bAllowResidentOverlap), Before) ||
				!Transfer(Walk.Piece->MetadataLease) || !Transfer(Walk.Piece->Mesh->ResourceLease)) return false;
			EVoxelTerrainResourceAdmission ChunkAdmission = EVoxelTerrainResourceAdmission::None;
			auto Chunk = Renderer.PrepareChunk(Work.Consumer, Walk.Piece->Mesh, 0,
				Item.Geometry->WorldCellOrigin * Work.CellSize, Item.Geometry->CellScale * Work.CellSize, 0, INDEX_NONE, &ChunkAdmission);
			if (!Chunk) { ObserveAdmission(ChunkAdmission, Before); return false; }
			auto Piece = VoxelBoundedAllocation::MakeWorkerShared<FVoxelTerrainCoarseDrawPiece>(Lease);
			Piece->Lease = MoveTemp(Lease);
			Piece->Piece = Walk.Piece;
			Piece->Chunk = MoveTemp(Chunk);
			if (Walk.LastPiece) Walk.LastPiece->Next = Piece;
			else Walk.Result->FirstPiece = Piece;
			Walk.LastPiece = MoveTemp(Piece);
			Walk.Piece = Walk.Piece->Next;
			continue;
		}
		// 所有上传仍受原 Working/Resident 授额约束；最终扫描前不会封存或发布。
		CompleteDraw(Walk.Result);
	}
	return Step > 0 && !Work.bUploadComplete;
}

void FVoxelTerrainRendererState::ReleasePreparation(FReadyTransaction& InTransaction)
{
	if (RootMerge && RootMerge->Contains(InTransaction.Preparation)) ReleaseRootMerge();
	for (auto& Pending : Ready)
	{
		if (!Pending.SuspendedMerge || !Pending.SuspendedMerge->Contains(InTransaction.Preparation)) continue;
		ReleaseRootMerge(Pending.SuspendedMerge);
		break;
	}
	if (!InTransaction.Preparation) return;
	const uint64 Token = InTransaction.Preparation->Consumer.RequestSerial;
	Scheduler.CancelMatching([this, Token](EVoxelTaskKind Kind, const FVoxelTaskStamp& Stamp)
	{
		return Kind == EVoxelTaskKind::BuildViewCoverage && Stamp.OwnerId == OwnerId &&
			Stamp.OwnerIncarnation == 1 && Stamp.WorldEpoch == Epoch && Stamp.Token == Token;
	});
	FRootPreparation* Preparation = InTransaction.Preparation.Get();
	const int32 ActiveIndex = Preparation->ActiveIndex;
	check(ActivePreparations.IsValidIndex(ActiveIndex) && ActivePreparations[ActiveIndex] == Preparation);
	ActivePreparations.RemoveAtSwap(ActiveIndex, 1, EAllowShrinking::No);
	if (ActivePreparations.IsValidIndex(ActiveIndex)) ActivePreparations[ActiveIndex]->ActiveIndex = ActiveIndex;
	Preparation->ActiveIndex = INDEX_NONE;
	InTransaction.Preparation.Reset();
}

void FVoxelTerrainRendererState::ReleaseRootMerge(TSharedPtr<FRootMerge, ESPMode::ThreadSafe> InMerge)
{
	if (!InMerge) InMerge = RootMerge;
	if (!InMerge) return;
	for (auto& Pending : Ready)
	{
		if (Pending.MergeConsumer != InMerge->Consumer) continue;
		Pending.SuspendedMerge.Reset();
		Pending.MergeConsumer = {};
		Pending.bMergeCapacityBlocked = false;
	}
	const uint64 Token = InMerge->Consumer.RequestSerial;
	Scheduler.CancelMatching([this, Token](EVoxelTaskKind Kind, const FVoxelTaskStamp& Stamp)
	{
		return Kind == EVoxelTaskKind::BuildViewCoverage && Stamp.OwnerId == OwnerId &&
			Stamp.OwnerIncarnation == 1 && Stamp.WorldEpoch == Epoch && Stamp.Token == Token;
	});
	if (RootMerge == InMerge) RootMerge.Reset();
}

void FVoxelTerrainRendererState::TerminateTransaction(FReadyTransaction& InTransaction)
{
	if (InTransaction.bTerminalFailure) return;
	InTransaction.bTerminalFailure = true;
	AdvanceCoarseAdmissionVersion();
	for (const auto& Change : InTransaction.Resources->CoarseChanges) PendingCoarseSources.Remove(Change.Item->SourceId);
	TOptional<FVoxelFineHandoffTransaction> Canceled;
	Handoff.RequestCancel(InTransaction.Id, Canceled);
	if (RootMerge && RootMerge->Contains(InTransaction.Preparation)) ReleaseRootMerge();
	ReleasePreparation(InTransaction);
}

void FVoxelTerrainRendererState::AdvanceRootPreparations(const double InDeadline)
{
	const double Deadline = FMath::Min(InDeadline, Module.GetTerrainFrameContext().Deadline());
	const double PriorityStarted = FPlatformTime::Seconds();
	const double PriorityDeadline = PriorityStarted + (Deadline - PriorityStarted) * 0.125;
	for (int32 Scan = 0; Scan < FMath::Min(32, Ready.Num()) && FPlatformTime::Seconds() < PriorityDeadline; ++Scan)
	{
		if (PreparationPriorityCursor >= Ready.Num())
		{
			PreparationPriorityCursor = 0;
			check(PreparationPriorityPass < MAX_uint64);
			++PreparationPriorityPass;
		}
		const int32 PriorityIndex = PreparationPriorityCursor;
		auto& Pending = Ready[PriorityIndex];
		if (Pending.PriorityPass == PreparationPriorityPass)
		{
			++PreparationPriorityCursor;
			continue;
		}
		Pending.PriorityPass = PreparationPriorityPass;
		const FBox Bounds = Pending.Resources->GetChangedBounds();
		FVoxelTerrainServicePriority Priority;
		for (const auto& Pair : Module.GetCurrentInterest().Sources)
		{
			const auto& Source = Pair.Value.Source;
			if (!Bounds.IsValid || !Source.Has(EVoxelStreamingCapability::FineVisual)) continue;
			FVoxelTerrainServicePriority Candidate = Pending.Priority;
			const FVector Position(Source.Center);
			const FVector Delta = Bounds.GetClosestPointTo(Position) - Position;
			Candidate.DistanceCells = Delta.Size();
			Candidate.ForwardScore = Delta.IsNearlyZero() ? 1.0 :
				FVector::DotProduct(Source.Direction.GetSafeNormal(), Delta.GetSafeNormal());
			Candidate.SourcePriority = Source.GetSchedulingPriority();
			Candidate.SourceStableKey = Source.Id;
			Priority = FVoxelTerrainWorkSelection::Promote(Priority, Candidate);
		}
		const bool bOrderChanged = Priority.DistanceCells != MAX_dbl &&
			(Pending.Priority.Band != Priority.Band || Pending.Priority.DistanceCells != Priority.DistanceCells ||
			 Pending.Priority.SourcePriority != Priority.SourcePriority || Pending.Priority.ForwardScore != Priority.ForwardScore);
		if (Priority.DistanceCells != MAX_dbl) Pending.Priority = Priority;
		if (Pending.Preparation) Pending.Preparation->SchedulingBand = Pending.Priority.Band;
		if (bOrderChanged)
		{
			FReadyTransaction Reordered = MoveTemp(Pending);
			RemoveReady(PriorityIndex);
			InsertReady(MoveTemp(Reordered));
		}
		else ++PreparationPriorityCursor;
	}
#if !UE_BUILD_SHIPPING
	if (FPlatformTime::Seconds() >= NextPreparationDiagnostic)
	{
		NextPreparationDiagnostic = FPlatformTime::Seconds() + 5.0;
		UE_LOG(LogTemp, Display,
			TEXT("Native seal state: ready=%d issued=%d merge=%d mergeSerial=%llu mergeComplete=%d mergeWaiting=%d mergeSuspended=%d sealFault=%d preparationFault=%d uploadFault=%d estimateUs=%.3f"),
			Ready.Num(), PendingIssued.Num(), RootMerge.IsValid(), RootMerge ? RootMerge->Consumer.RequestSerial : 0,
			RootMerge && RootMerge->bComplete, RootMerge && RootMerge->bWaitingCapacity.Load(), RootMerge && RootMerge->bSuspended.Load(),
			SealSelection.HasAccountingFault(), PreparationSelection.HasAccountingFault(), UploadSelection.HasAccountingFault(), SealCostMicroseconds);
		for (const FRootPreparation* Work : ActivePreparations)
		{
			const FIntVector Section = Work->Publication->Atoms.IsEmpty() ? FIntVector::ZeroValue : Work->Publication->Atoms[0]->Section;
			UE_LOG(LogTemp, Display, TEXT("Native active: request=%llu section=%s band=%d enqueued=%d executing=%d completed=%d applied=%d phase=%d fineProgress=%u fineReused=%d fineSourceRangesReused=%d finePrepared=%d fineCompleted=%d fineSection=(%d,%d,%d) capacityWait=%d uploadComplete=%d uploadAdmission=%d fineUpload=%d batch=%d triangle=%d coarseRoot=%d merge=%d ready=%d active=%d heightfieldTiles=%d heightfieldPieces=%d"),
				Work->Consumer.RequestSerial, *Section.ToString(), int32(Work->SchedulingBand), Work->bEnqueued,
				Work->bExecuting.Load(), Work->bExecutionComplete.Load(), Work->bDecodeComplete, Work->DiagnosticPhase.Load(),
				Work->DiagnosticFineProgress.Load(), Work->DiagnosticFineReused.Load(), Work->DiagnosticFineSourceRangesReused.Load(), Work->DiagnosticFinePrepared.Load(),
				Work->DiagnosticFineCompleted.Load(), Work->DiagnosticFineSectionX.Load(),
				Work->DiagnosticFineSectionY.Load(), Work->DiagnosticFineSectionZ.Load(),
				Work->bWaitingCapacity.Load(), Work->bUploadComplete, int32(Work->UploadAdmission),
				bool(Work->FineUpload), Work->FineUpload ? Work->FineUpload->UploadBatch : -1,
				Work->FineUpload ? Work->FineUpload->UploadTriangle : -1, Work->UploadRoot,
				RootMerge && RootMerge->Preparation.Get() == Work, Ready.Num(), ActivePreparations.Num(),
				Work->DiagnosticHeightfieldTiles.Load(), Work->DiagnosticHeightfieldPieces.Load());
		}
		int32 Logged = 0;
		for (int32 Scan = 0; Scan < FMath::Min(32, Ready.Num()) && FPlatformTime::Seconds() < Deadline; ++Scan)
		{
			if (Logged == 8) break;
			PreparationDiagnosticCursor %= Ready.Num();
			const auto& Pending = Ready[PreparationDiagnosticCursor++];
			++Logged;
			const auto Work = Pending.Preparation;
			const FIntVector Section = Pending.Resources->Atoms.IsEmpty() ? FIntVector::ZeroValue : Pending.Resources->Atoms[0]->Section;
			UE_LOG(LogTemp, Display, TEXT("Native publication: section=%s atoms=%d coarse=%d band=%d waiting=%d terminal=%d prep=%d phase=%d enqueued=%d decoded=%d uploaded=%d fineUpload=%d admission=%d merge=%d ready=%d active=%d"),
				*Section.ToString(), Pending.Resources->Atoms.Num(), Pending.Resources->CoarseChanges.Num(),
				int32(Pending.Priority.Band), Pending.bPreparationAdmissionWaiting,
				Pending.bTerminalFailure, bool(Work), Work ? (Work->bEnqueued ? -2 : Work->Phase) : -1, Work && Work->bEnqueued,
				Work && Work->bDecodeComplete, Work && Work->bUploadComplete, Work && Work->FineUpload,
				Work ? int32(Work->UploadAdmission) : -1, RootMerge && RootMerge->Preparation == Work, Ready.Num(), ActivePreparations.Num());
		}

	}
#endif
	// 过期事务即使在容量等待中也必须撤销，不能一直占有 Atom 锁及准备名额。
	for (int32 Scan = 0; Scan < FMath::Min(32, Ready.Num()) &&
		FPlatformTime::Seconds() < Deadline; ++Scan)
	{
		PreparationScanCursor %= Ready.Num();
		const int32 Index = PreparationScanCursor++;
		auto& Pending = Ready[Index];
		if (Pending.bTerminalFailure)
		{
			// Coarse 失败由原发布者消费；Fine 取消已交给 Handoff。
			if (Pending.Resources->CoarseChanges.IsEmpty() ?
				!Pending.IsCurrent || Pending.Resources->Atoms.IsEmpty() ||
				!Pending.IsCurrent(Pending.Resources->Atoms[0]->Section, Pending.Resources->Atoms[0]->Evidence) :
				!Pending.IsCoarseCurrent || !Pending.IsCoarseCurrent())
			{
				AdvanceCoarseAdmissionVersion();
				ReleasePreparation(Pending);
				RemoveReady(Index);
			}
			continue;
		}
		const bool bCurrent = !Pending.Resources->CoarseChanges.IsEmpty() ? Pending.IsCoarseCurrent && Pending.IsCoarseCurrent() :
			!Pending.Resources->Atoms.ContainsByPredicate([&Pending](const FAtomPtr& Atom)
			{
				return !Pending.IsCurrent || !Pending.IsCurrent(Atom->Section, Atom->Evidence);
			});
		if (bCurrent) continue;
		AdvanceCoarseAdmissionVersion();
		for (const auto& Change : Pending.Resources->CoarseChanges) PendingCoarseSources.Remove(Change.Item->SourceId);
		TOptional<FVoxelFineHandoffTransaction> Canceled;
		Handoff.RequestCancel(Pending.Id, Canceled);
		ReleasePreparation(Pending);
		AdvanceCoarseAdmissionVersion();
		RemoveReady(Index);
	}
	FVoxelTerrainServiceCapacity Capacity;
	Capacity.MaximumWorkers = VoxelNativeRootPreparationConcurrency;
	Capacity.NearReservedWorkers = VoxelNativeRootPreparationConcurrency / 2;
	Capacity.MinimumNonNearWorkers = 1;
	Capacity.MaximumWaitSeconds = Module.GetViewSettings().Fine.MaximumBackgroundWaitSeconds;
	for (const FRootPreparation* Preparation : ActivePreparations)
	{
		if (FPlatformTime::Seconds() >= Deadline) return;
		++Capacity.RunningWorkers;
		Capacity.RunningNearWorkers += FVoxelTerrainWorkSelection::IsNear(Preparation->SchedulingBand) ? 1 : 0;
	}
	for (int32 Step = 0; Step < VoxelNativeRootPreparationConcurrency && !Ready.IsEmpty() && FPlatformTime::Seconds() < Deadline; ++Step)
	{
		const double ScanStarted = FPlatformTime::Seconds();
		const double ScanDeadline = ScanStarted + (Deadline - ScanStarted) * 0.5;
		Capacity.bAllowOverdueOpportunity = Module.GetTerrainFrameContext().IsOverdueOpportunityAvailable();
		FVoxelTerrainServiceCandidateSet CandidateSet;
		const auto AddCandidate = [&](const int32 Index)
		{
			const auto& Pending = Ready[Index];
			if (Pending.bTerminalFailure || Pending.Preparation) return;
			for (const FRootPreparation* Preparation : ActivePreparations)
			{
				if (Preparation->Publication->ConflictsWith(*Pending.Resources) ||
					Preparation->Publication->SharesCoarseOwner(*Pending.Resources,
						Preparation->BaseSnapshot ? Preparation->BaseSnapshot->CoarseRoot : nullptr) ||
					Preparation->Publication->SharesCoarseOwner(*Pending.Resources,
						LastSealedSnapshot ? LastSealedSnapshot->CoarseRoot : nullptr)) return;
			}
			if (Pending.bPreparationAdmissionWaiting &&
				!RendererHasExternalRelease(Resources, Pending.PreparationConsumer, Pending.PreparationAdmissionVersion)) return;
			CandidateSet.Add({Index, Pending.Priority, Pending.FirstQueuedSeconds, Pending.Sequence, PreparationCostMicroseconds, true});
		};
		for (int32 Scan = 0; Scan < Ready.Num() && FPlatformTime::Seconds() < ScanDeadline; ++Scan)
		{
			PreparationAdmissionCursor %= Ready.Num();
			const int32 Index = PreparationAdmissionCursor++;
			AddCandidate(Index);
		}
		FVoxelTerrainServiceCharge Charge;
		const int32 Index = PreparationSelection.Select(CandidateSet.GetCandidates(), Capacity, FPlatformTime::Seconds(), Charge);
		if (Index == INDEX_NONE) break;
		if (FPlatformTime::Seconds() >= Deadline)
		{
			verify(PreparationSelection.RefundBeforeStart(Charge.Serial));
			break;
		}
		if (Charge.bOverdueOpportunity) Module.GetTerrainFrameContext().ConsumeOverdueOpportunity();
		const double Started = FPlatformTime::Seconds();
		BeginRootPreparation(Ready[Index]);
		const double Actual = FMath::Max(1.0, (FPlatformTime::Seconds() - Started) * 1000000.0);
		verify(PreparationSelection.Settle(Charge.Serial, Actual));
		PreparationCostMicroseconds = FMath::Lerp(PreparationCostMicroseconds, Actual, 0.15);
		if (Ready[Index].Preparation)
		{
			++Capacity.RunningWorkers;
			Capacity.RunningNearWorkers += FVoxelTerrainWorkSelection::IsNear(Ready[Index].Priority.Band) ? 1 : 0;
		}
	}
	// Scheduler 配额暂缓时保留原准备及年龄，不重新申请路径租约。
	for (int32 Scan = 0; Scan < FMath::Min(32, Ready.Num()) &&
		FPlatformTime::Seconds() < Deadline; ++Scan)
	{
		PreparationRetryCursor %= Ready.Num();
		auto& Pending = Ready[PreparationRetryCursor++];
		if (Pending.Preparation && !Pending.Preparation->bEnqueued) BeginRootPreparation(Pending);
	}
}

bool FVoxelTerrainRendererState::BeginRootMerge(FReadyTransaction& InTransaction, const double InDeadline)
{
	check(InTransaction.Preparation && InTransaction.Preparation->bUploadComplete);
	if (InTransaction.bTerminalFailure) return false;
	if (RootMerge && RootMerge->Current != LastSealedSnapshot) ReleaseRootMerge();
	if (RootMerge) return RootMerge->Contains(InTransaction.Preparation) && RootMerge->bComplete;
	if (InTransaction.SuspendedMerge)
	{
		RootMerge = InTransaction.SuspendedMerge;
		if (RootMerge->Current == LastSealedSnapshot)
		{
			for (auto& Pending : Ready)
				if (Pending.SuspendedMerge == RootMerge) Pending.SuspendedMerge.Reset();
			RootMerge->bSuspended.Store(false);
			return RootMerge->bComplete;
		}
		ReleaseRootMerge();
	}
	TArray<TSharedPtr<FRootPreparation, ESPMode::ThreadSafe>, TInlineAllocator<VoxelNativeRootPreparationConcurrency>> Preparations;
	Preparations.Add(InTransaction.Preparation);
	for (auto& Pending : Ready)
	{
		if (Preparations.Num() == VoxelNativeRootPreparationConcurrency ||
			PendingIssued.Num() + Preparations.Num() >= VoxelNativePublicationCapacity || FPlatformTime::Seconds() >= InDeadline) break;
		if (&Pending == &InTransaction || Pending.bTerminalFailure || !Pending.Preparation ||
			!Pending.Preparation->bUploadComplete || Pending.SuspendedMerge || Pending.bSealCapacityBlocked ||
			Pending.bMergeCapacityBlocked) continue;
		bool bConflict = false;
		for (const auto& Prepared : Preparations)
		{
			bConflict |= Prepared->Publication->ConflictsWith(*Pending.Resources) ||
				Prepared->Publication->SharesCoarseOwner(*Pending.Resources, LastSealedSnapshot ? LastSealedSnapshot->CoarseRoot : nullptr);
		}
		if (!bConflict) Preparations.Add(Pending.Preparation);
	}
	if (Preparations.Num() == 1 && InTransaction.Preparation->BaseSnapshot == LastSealedSnapshot) return true;

	if (!InTransaction.MergeConsumer.IsValid())
	{
		if (NextMetadataSerial == MAX_uint64) { TerminateTransaction(InTransaction); return false; }
		InTransaction.MergeConsumer = FVoxelGenerationConsumerKey::Section(Epoch, OwnerId, 1, {}, ++NextMetadataSerial);
	}
	const auto Consumer = InTransaction.MergeConsumer;
	if (InTransaction.bMergeCapacityBlocked && !RendererHasExternalRelease(Resources, Consumer, InTransaction.MergeCapacityVersion)) return false;
	InTransaction.bMergeCapacityBlocked = false;
	const uint64 BeforeAdmission = Resources->ReleaseVersionExcludingOwner(Consumer);
	const auto RejectAdmission = [&](const bool bTerminal)
	{
		if (bTerminal) TerminateTransaction(InTransaction);
		else
		{
			InTransaction.MergeCapacityVersion = BeforeAdmission;
			InTransaction.bMergeCapacityBlocked = true;
		}
	};
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	FRendererBindingInputFloor InputFloor;
	if (!InputFloor.Add(InputLease, Resources.Get())) { RejectAdmission(true); return false; }
	for (const auto& Prepared : Preparations)
	{
		if (!InputFloor.Add(Prepared->InputLease, Resources.Get()) ||
			!InputFloor.Add(Prepared->Publication->CoarseCallbackLease, Resources.Get()))
		{ RejectAdmission(true); return false; }
	}

	const auto InputAdmission = Resources->TryAcquire(Consumer, EVoxelTerrainResourcePool::Input,
		RenderRootAllocationBytes<FRootMerge>(), Lease, 0, InputFloor.Bytes, InputFloor.LeaseCount);
	if (InputAdmission != EVoxelTerrainResourceAdmission::Granted)
	{
		RejectAdmission(InputAdmission != EVoxelTerrainResourceAdmission::CapacityBlocked);
		return false;
	}
	const auto Work = CreateRenderRoot<FRootMerge>(MoveTemp(Lease), Resources, Consumer);
	Work->Begin(Preparations, LastSealedSnapshot);
	// 根合并独占封帧通道：继承等待事务中的最高服务带，并排在该带距离前沿，避免被同带新任务长期越过。
	EVoxelTerrainServiceBand MergeBand = InTransaction.Priority.Band;
	for (const FReadyTransaction& Waiting : Ready)
	{
		if (MergeBand == EVoxelTerrainServiceBand::P0) break;
		if (!Waiting.bTerminalFailure && uint8(Waiting.Priority.Band) < uint8(MergeBand)) MergeBand = Waiting.Priority.Band;
	}
	FVoxelTaskRequest Request;
	Request.Kind = EVoxelTaskKind::BuildViewCoverage;
	Request.Stamp.OwnerId = OwnerId;
	Request.Stamp.OwnerIncarnation = 1;
	Request.Stamp.WorldEpoch = Epoch;
	Request.Stamp.Token = Consumer.RequestSerial;
	Request.Stamp.Section = FIntVector(MIN_int32, 9, 0);
	Request.WorkClass = MergeBand == EVoxelTerrainServiceBand::P0 ? EVoxelWorkClass::Critical : EVoxelWorkClass::Visible;
	Request.ServiceBand = MergeBand;
	Request.SourceStableKey = InTransaction.Priority.SourceStableKey;
	Request.SourcePriority = InTransaction.Priority.SourcePriority;
	Request.DistanceScore = 0.0;
	Request.ForwardScore = 1.0;
	Request.FirstRunnableSeconds = InTransaction.FirstQueuedSeconds;
	Request.bResumable = Request.bPublicationContinuation = true;
	Request.InputBytes = Work->InputLease->Bytes();
	Request.GenerationConsumer = Work->Consumer;
	FRendererBindingInputFloor BindingInput;
	if (!Work->AddBindingInputs(BindingInput))
	{
		RejectAdmission(true);
		return false;
	}
	Request.SetRequiredBindingInput(BindingInput.Bytes, BindingInput.LeaseCount);
	Request.ReservedBytes = 1;
	{
		FString CallbackBindingError;
		if (!Request.BindGetBlockReason(Scheduler.GetTerrainResources(), (Request.GenerationConsumer.IsValid() ? Request.GenerationConsumer : Scheduler.GetControlConsumer()),
			[Work]
	{
		return Work->bSuspended.Load() || (Work->bWaitingCapacity.Load() &&
			!RendererHasExternalRelease(Work->Resources, Work->Consumer, Work->CapacityVersion.Load())) ?
			EVoxelTaskBlockReason::Memory : EVoxelTaskBlockReason::None;
	}, CallbackBindingError))
		{
			Scheduler.ObserveDemand(Request.Kind, Request.Stamp,
				CallbackBindingError.StartsWith(TEXT("Oversized")) || CallbackBindingError.StartsWith(TEXT("CapacityUnachievable"))
					? EVoxelTaskBlockReason::Oversized : EVoxelTaskBlockReason::InputMemory);
			RejectAdmission(!CallbackBindingError.StartsWith(TEXT("CapacityBlocked")));
			return false;
		}
	}
	{
		FString CallbackBindingError;
		if (!Request.BindExecute(Scheduler.GetTerrainResources(), (Request.GenerationConsumer.IsValid() ? Request.GenerationConsumer : Scheduler.GetControlConsumer()),
			[Work](const TAtomic<bool>& Cancel) { return Work->Advance(Cancel); }, CallbackBindingError))
		{
			Scheduler.ObserveDemand(Request.Kind, Request.Stamp,
				CallbackBindingError.StartsWith(TEXT("Oversized")) || CallbackBindingError.StartsWith(TEXT("CapacityUnachievable"))
					? EVoxelTaskBlockReason::Oversized : EVoxelTaskBlockReason::InputMemory);
			RejectAdmission(!CallbackBindingError.StartsWith(TEXT("CapacityBlocked")));
			return false;
		}
	}
	TWeakPtr<FVoxelTerrainRendererState, ESPMode::ThreadSafe> WeakState = AsShared();
	{
		FString CallbackBindingError;
		if (!Request.BindApply(Scheduler.GetTerrainResources(), (Request.GenerationConsumer.IsValid() ? Request.GenerationConsumer : Scheduler.GetControlConsumer()),
			[WeakState, Work](FVoxelTaskResult&& Result)
	{
		const auto Pinned = WeakState.Pin();
		if (!Pinned || !Pinned->bActive.Load() || Result.Stamp.WorldEpoch != Pinned->Epoch ||
			Result.Stamp.OwnerId != Pinned->OwnerId || Result.Stamp.OwnerIncarnation != 1 ||
			Result.Stamp.Token != Work->Consumer.RequestSerial) return;
		if (Pinned->RootMerge != Work && !Pinned->Ready.ContainsByPredicate([&Work](const FReadyTransaction& Pending)
		{
			return Pending.SuspendedMerge == Work;
		})) return;
		if (Result.bSuccess && !Result.bCanceled)
		{
			Work->bComplete = true;
			for (const auto& Entry : Work->Entries) Entry.Preparation->Publication->MergeReceipt = Result.WorkReceipt;
			return;
		}
		const auto Failed = Work->Entries[FMath::Min(Work->MergeIndex, Work->Entries.Num() - 1)].Preparation;
		Pinned->ReleaseRootMerge(Work);
		const int32 Index = Pinned->Ready.IndexOfByPredicate([&Failed](const FReadyTransaction& Pending)
		{
			return Pending.Preparation == Failed;
		});
		if (Index == INDEX_NONE) return;
		if (Pinned->Scheduler.IsTerminal({Result.Kind, Result.Stamp}) ||
			Result.Error.StartsWith(TEXT("Oversized")) || Result.Error.StartsWith(TEXT("CapacityUnachievable")))
			Pinned->TerminateTransaction(Pinned->Ready[Index]);
		else Pinned->ReleasePreparation(Pinned->Ready[Index]);
	}, CallbackBindingError))
		{
			Scheduler.ObserveDemand(Request.Kind, Request.Stamp,
				CallbackBindingError.StartsWith(TEXT("Oversized")) || CallbackBindingError.StartsWith(TEXT("CapacityUnachievable"))
					? EVoxelTaskBlockReason::Oversized : EVoxelTaskBlockReason::InputMemory);
			RejectAdmission(!CallbackBindingError.StartsWith(TEXT("CapacityBlocked")));
			return false;
		}
	}
	if (!Scheduler.Enqueue(MoveTemp(Request)))
	{
		RejectAdmission(RendererIsTerminalAdmission(Request.Blocked));
		return false;
	}
	for (auto& Pending : Ready)
		if (Work->Contains(Pending.Preparation)) Pending.MergeConsumer = Consumer;
	RootMerge = Work;
	return false;
}

FSnapshotPtr FVoxelTerrainRendererState::SealFrameSnapshot(const uint64 InFrameCounter, const double InDeadline)
{
	check(IsInGameThread());
	const double Deadline = FMath::Min(InDeadline, Module.GetTerrainFrameContext().Deadline());
	if (FPlatformTime::Seconds() >= Deadline) return LastSealedSnapshot;
	if (InFrameCounter == LastFrame || !PrepareMetadata() || Ready.IsEmpty() || PendingIssued.Num() >= VoxelNativePublicationCapacity ||
		NextPresentation == MAX_uint64 || NextMetadataSerial == MAX_uint64) return LastSealedSnapshot;
	FVoxelTerrainServiceCharge Charge;
	int32 Index = INDEX_NONE;
	if (RootMerge)
	{
		Index = Ready.IndexOfByPredicate([this](const FReadyTransaction& ReadyItem) { return ReadyItem.Preparation == RootMerge->Preparation; });
		if (Index != INDEX_NONE && !RootMerge->bComplete) return LastSealedSnapshot;
	}
	if (Index == INDEX_NONE)
	{
		if (RootMerge)
		{
			ReleaseRootMerge();
		}
		FVoxelTerrainServiceCandidateSet CandidateSet;
		const double ScanStarted = FPlatformTime::Seconds();
		const double ScanDeadline = ScanStarted + (Deadline - ScanStarted) * 0.5;
		constexpr int32 BandCount = int32(EVoxelTerrainServiceBand::Count);
		int32 Nearest[BandCount], Oldest[BandCount];
		for (int32 Band = 0; Band < BandCount; ++Band) Nearest[Band] = Oldest[Band] = INDEX_NONE;
		const auto Consider = [&](const int32 CandidateIndex)
		{
			auto& Candidate = Ready[CandidateIndex];
			if (Candidate.bMergeCapacityBlocked && RendererHasExternalRelease(Resources, Candidate.MergeConsumer, Candidate.MergeCapacityVersion))
				Candidate.bMergeCapacityBlocked = false;
			if (Candidate.bSealCapacityBlocked && RendererHasExternalRelease(Resources, Candidate.SealConsumer, Candidate.SealCapacityVersion))
				Candidate.bSealCapacityBlocked = false;
			const bool bRunnable = !Candidate.bTerminalFailure && Candidate.Preparation && Candidate.Preparation->bUploadComplete &&
				!Candidate.bMergeCapacityBlocked && !Candidate.bSealCapacityBlocked;
			if (!bRunnable) return;
			const int32 Band = int32(Candidate.Priority.Band);
			if (Band < 0 || Band >= BandCount) return;
			if (Nearest[Band] == INDEX_NONE || HigherVisibleTransaction(Candidate, Ready[Nearest[Band]])) Nearest[Band] = CandidateIndex;
			if (Oldest[Band] == INDEX_NONE || Candidate.FirstQueuedSeconds < Ready[Oldest[Band]].FirstQueuedSeconds ||
				(Candidate.FirstQueuedSeconds == Ready[Oldest[Band]].FirstQueuedSeconds && Candidate.Sequence < Ready[Oldest[Band]].Sequence))
				Oldest[Band] = CandidateIndex;
		};
		for (int32 Scan = 0; Scan < FMath::Min(64, Ready.Num()) && FPlatformTime::Seconds() < ScanDeadline; ++Scan)
		{
			SealScanCursor %= Ready.Num();
			Consider(SealScanCursor++);
		}
		for (int32 Near = 0; Near < FMath::Min(8, Ready.Num()) && FVoxelTerrainWorkSelection::IsNear(Ready[Near].Priority.Band) &&
			FPlatformTime::Seconds() < ScanDeadline; ++Near) Consider(Near);
		const auto AddHead = [&](const int32 CandidateIndex)
		{
			if (CandidateIndex == INDEX_NONE) return;
			const auto& Candidate = Ready[CandidateIndex];
			CandidateSet.Add({CandidateIndex, Candidate.Priority, Candidate.FirstQueuedSeconds, Candidate.Sequence, SealCostMicroseconds, true});
		};
		for (int32 Band = 0; Band < BandCount; ++Band)
		{
			AddHead(Nearest[Band]);
			if (Oldest[Band] != Nearest[Band]) AddHead(Oldest[Band]);
		}
		FVoxelTerrainServiceCapacity Capacity;
		Capacity.bAllowOverdueOpportunity = false;
		Capacity.MaximumWaitSeconds = Module.GetViewSettings().Fine.MaximumBackgroundWaitSeconds;
		Index = SealSelection.Select(CandidateSet.GetCandidates(), Capacity, FPlatformTime::Seconds(), Charge);
		if (Index == INDEX_NONE) return LastSealedSnapshot;
	}
	if (FPlatformTime::Seconds() >= Deadline)
	{
		if (Charge.Serial) verify(SealSelection.RefundBeforeStart(Charge.Serial));
		return LastSealedSnapshot;
	}
	if (Charge.bOverdueOpportunity) Module.GetTerrainFrameContext().ConsumeOverdueOpportunity();
	const double Started = FPlatformTime::Seconds();
	ON_SCOPE_EXIT
	{
		if (Charge.Serial)
		{
			const double Actual = FMath::Max(1.0, (FPlatformTime::Seconds() - Started) * 1000000.0);
			verify(SealSelection.Settle(Charge.Serial, Actual));
			SealCostMicroseconds = FMath::Lerp(SealCostMicroseconds, Actual, 0.15);
		}
	};
	if (!BeginRootMerge(Ready[Index], Deadline)) return LastSealedSnapshot;
	TArray<FRootMerge::FEntry, TInlineAllocator<VoxelNativeRootPreparationConcurrency>> Entries;
	if (RootMerge) Entries.Append(RootMerge->Entries);
	else Entries.Add({Ready[Index].Preparation, Ready[Index].Preparation->Root, Ready[Index].Preparation->CoarseRoot});
	const auto FindPending = [this](const FRootMerge::FEntry& Entry)
	{
		return Ready.IndexOfByPredicate([&Entry](const FReadyTransaction& Pending)
		{
			return Pending.Preparation == Entry.Preparation;
		});
	};
	const auto InterfacesCurrent = [](const FReadyTransaction& Pending, const FVoxelFineHandoffTransaction& Transaction,
		const FVoxelTaskCustomPayload& Publication)
	{
		if (&Publication != Pending.Resources.Get() || Transaction.Changes.Num() != Pending.Resources->Atoms.Num()) return false;
		for (const auto& Atom : Pending.Resources->Atoms)
		{
			if (!Atom->IsResourceReady()) return false;
			for (const auto& Boundary : Atom->Boundaries)
				if (!Boundary || !Boundary->Validate() || Boundary->Input.WorldEpoch != Transaction.WorldEpoch ||
					Boundary->Input.DataRevision != Atom->Evidence.Content.DataRevision ||
					Boundary->Input.OverlayRevision != Atom->Evidence.Content.OverlayRevision) return false;
		}
		return true;
	};
	// 封存前检查完整批次；任何过期成员都不得出现在待发布的聚合根中。
	for (const auto& Entry : Entries)
	{
		const int32 PendingIndex = FindPending(Entry);
		if (PendingIndex == INDEX_NONE)
		{
			ReleaseRootMerge();
			return LastSealedSnapshot;
		}
		auto& Pending = Ready[PendingIndex];
		FString Error;
		const bool bRemoveCurrent = !Pending.Resources->CoarseChanges.ContainsByPredicate([this](const FVoxelTerrainCoarseMutation& Change)
		{
			return Change.bRemove && (!LastSealedSnapshot || !SameCoarseGeometry(FindCoarseItem(LastSealedSnapshot->CoarseRoot,
				Change.Item->Geometry->Stamp.GeometryIdentity.Key, Change.Item->SourceId), Change.Item));
		});
		const auto* Transaction = Pending.Resources->CoarseChanges.IsEmpty() ? Handoff.FindTransaction(Pending.Id) : nullptr;
		const bool bCurrent = !Pending.Resources->CoarseChanges.IsEmpty() ?
			bRemoveCurrent && Pending.IsCoarseCurrent && Pending.IsCoarseCurrent() :
			Transaction && Handoff.ValidateBeforeSeal(Pending.Id, Pending.IsCurrent, Pending.IsCollisionCurrent, Error) &&
			InterfacesCurrent(Pending, *Transaction, *Pending.Resources);
		if (!bCurrent)
		{
			AdvanceCoarseAdmissionVersion();
			for (const auto& Change : Pending.Resources->CoarseChanges) PendingCoarseSources.Remove(Change.Item->SourceId);
			TOptional<FVoxelFineHandoffTransaction> Canceled;
			Handoff.RequestCancel(Pending.Id, Canceled);
			ReleasePreparation(Pending);
			RemoveReady(PendingIndex);
			return LastSealedSnapshot;
		}
		if (FPlatformTime::Seconds() >= Deadline) return LastSealedSnapshot;
	}
	FReadyTransaction& Head = Ready[Index];
	if (!Head.SealConsumer.IsValid())
		Head.SealConsumer = FVoxelGenerationConsumerKey::Section(Epoch, OwnerId, 1, {}, ++NextMetadataSerial);
	const auto Consumer = Head.SealConsumer;
	if (Head.bSealCapacityBlocked && !RendererHasExternalRelease(Resources, Consumer, Head.SealCapacityVersion)) return LastSealedSnapshot;
	Head.bSealCapacityBlocked = false;
	const uint64 BeforeSeal = Resources->ReleaseVersionExcludingOwner(Consumer);
	const auto ObserveSealAdmission = [&](const EVoxelTerrainResourceAdmission Admission)
	{
		if (Admission == EVoxelTerrainResourceAdmission::Granted) return true;
		if (Admission == EVoxelTerrainResourceAdmission::CapacityBlocked)
		{
			Head.bSealCapacityBlocked = true;
			Head.SealCapacityVersion = BeforeSeal;
		}
		else TerminateTransaction(Head);
		return false;
	};
	FRendererBindingInputFloor Floor;
	if (!Floor.Add(InputLease, Resources.Get()) || (RootMerge && !RootMerge->AddBindingInputs(Floor)))
	{
		ObserveSealAdmission(EVoxelTerrainResourceAdmission::InvalidRequest);
		return LastSealedSnapshot;
	}
	int64 RetiredAtomCount = 0;
	int32 ReceiptAtomCount = 0;
	for (const auto& Entry : Entries)
	{
		const auto& Prepared = *Entry.Preparation;
		if (!ObserveSealAdmission(TryTransferPublicationLease(Resources.Get(), Prepared.Publication->Lease, Prepared.bAllowResidentOverlap))) return LastSealedSnapshot;
		for (const auto& Atom : Prepared.Publication->Atoms)
			if (!ObserveSealAdmission(TryTransferPublicationLease(Resources.Get(), Atom->MetadataLease, Prepared.bAllowResidentOverlap))) return LastSealedSnapshot;
		if (!Floor.Add(Prepared.InputLease, Resources.Get()) || !Floor.Add(Prepared.Publication->CoarseCallbackLease, Resources.Get()) ||
			!Floor.Add(Prepared.PrimaryPathLease, Resources.Get()) || !Floor.Add(Prepared.Publication->Lease, Resources.Get()))
		{
			ObserveSealAdmission(EVoxelTerrainResourceAdmission::InvalidRequest);
			return LastSealedSnapshot;
		}
		RetiredAtomCount += Prepared.Publication->Atoms.Num();
		ReceiptAtomCount = FMath::Max(ReceiptAtomCount, Prepared.Publication->Atoms.Num());
	}
	for (const auto& Issued : PendingIssued)
	{
		const int32 Atoms = Issued.Handoff ? Issued.Handoff->Transaction.Changes.Num() : 0;
		RetiredAtomCount += Atoms;
		ReceiptAtomCount = FMath::Max(ReceiptAtomCount, Atoms);
	}
	if (RetiredAtomCount > MAX_int32)
	{
		ObserveSealAdmission(EVoxelTerrainResourceAdmission::CapacityUnachievable);
		return LastSealedSnapshot;
	}
	const int32 IssuedCapacity = PendingIssued.Num() + Entries.Num();
	const uint64 Bytes = VoxelBoundedAllocation::WorkerSharedBytes<FRenderSnapshot>() +
		VoxelBoundedAllocation::ArrayBytes<FFrozenPublication>(IssuedCapacity) +
		VoxelBoundedAllocation::ArrayBytes<FVoxelFinePresentedAtom>(int32(RetiredAtomCount)) +
		VoxelBoundedAllocation::ArrayBytes<FVoxelFineRenderReceiptAtom>(ReceiptAtomCount) +
		VoxelBoundedAllocation::ArrayBytes<FGuid>(IssuedCapacity);
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	if (!ObserveSealAdmission(TryAcquirePublicationResident(Resources, Consumer, Bytes, Lease, 0,
		Head.Preparation->bAllowResidentOverlap, Floor.Bytes, Floor.LeaseCount))) return LastSealedSnapshot;
	auto Snapshot = VoxelBoundedAllocation::MakeWorkerShared<FRenderSnapshot>(Lease);
	Snapshot->Lease = MoveTemp(Lease);
	Snapshot->Retirement.PreviousAtoms.Reserve(int32(RetiredAtomCount));
	Snapshot->Retirement.ReceiptScratch.Reserve(ReceiptAtomCount);
	Snapshot->Retirement.SettledTransactions.Reserve(IssuedCapacity);
	Snapshot->WorldEpoch = Epoch;
	Snapshot->RendererOwner = OwnerId;
	Snapshot->Serial = ++NextPresentation;
	Snapshot->FrameCounter = InFrameCounter;
	Snapshot->Root = LastSealedSnapshot ? LastSealedSnapshot->Root : nullptr;
	Snapshot->CoarseRoot = LastSealedSnapshot ? LastSealedSnapshot->CoarseRoot : nullptr;
	Snapshot->Issued.Reserve(IssuedCapacity);
	check(VoxelBoundedAllocation::WorkerSharedBytes<FRenderSnapshot>() + Snapshot->Issued.GetAllocatedSize() +
		Snapshot->Retirement.PreviousAtoms.GetAllocatedSize() + Snapshot->Retirement.ReceiptScratch.GetAllocatedSize() +
		Snapshot->Retirement.SettledTransactions.GetAllocatedSize() <= Bytes);
	Snapshot->Issued.Append(PendingIssued);
	const FVoxelFinePublishStamp Stamp{Epoch, Snapshot->Serial, InFrameCounter};
	// 合并输出已完成且仍由本地引用持有；解除在途关联后逐项交给独立回执。
	const auto Batch = MoveTemp(RootMerge);
	for (auto& Pending : Ready)
	{
		if (!Batch || Pending.MergeConsumer != Batch->Consumer) continue;
		Pending.MergeConsumer = {};
		Pending.bMergeCapacityBlocked = false;
		Pending.SuspendedMerge.Reset();
	}
	int32 SealedCount = 0;
	for (const auto& Entry : Entries)
	{
		if (FPlatformTime::Seconds() >= Deadline) break;
		const int32 PendingIndex = FindPending(Entry);
		check(PendingIndex != INDEX_NONE);
		auto& Pending = Ready[PendingIndex];
		TSharedPtr<const FVoxelFineFrozenHandoff, ESPMode::ThreadSafe> Frozen;
		FString Error;
		if (Pending.Resources->CoarseChanges.IsEmpty() && !Handoff.SealTransaction(Pending.Id, Stamp, Pending.Resources,
			Pending.IsCurrent, Pending.IsCollisionCurrent,
			[&Pending, &InterfacesCurrent](const FVoxelFineHandoffTransaction& Transaction, const FVoxelTaskCustomPayload& Publication)
			{
				return InterfacesCurrent(Pending, Transaction, Publication);
			}, Frozen, Error))
		{
			TOptional<FVoxelFineHandoffTransaction> Canceled;
			Handoff.RequestCancel(Pending.Id, Canceled);
			ReleasePreparation(Pending);
			AdvanceCoarseAdmissionVersion();
			RemoveReady(PendingIndex);
			break;
		}
		Pending.Resources->Stamp = Stamp;
		checkf(Pending.Resources->PendingIssuedIndex == INDEX_NONE && !Pending.Resources->bReceiptSettled.Load(),
			TEXT("Publication issued twice: epoch=%llu owner=%s transaction=%s previousIndex=%d pending=%d frame=%llu receiptOwner=%llu"),
			Epoch, *OwnerId.ToString(), *Pending.Resources->Transaction.ToString(), Pending.Resources->PendingIssuedIndex,
			PendingIssued.Num(), Snapshot->Serial, Pending.Resources->ReceiptOwnerFrameSerial);
		Snapshot->Issued.Add({Frozen, Pending.Resources});
		Pending.Resources->PendingIssuedIndex = PendingIssued.Add({Frozen, Pending.Resources});
		SubmittedCallbacks.Add(Pending.Id, MoveTemp(Pending.OnSubmitted));
		Snapshot->Root = Entry.Root;
		Snapshot->CoarseRoot = Entry.CoarseRoot;
		Pending.Resources->Lease->MarkPublished();
		if (Pending.Resources->CoarseCallbackLease) Pending.Resources->CoarseCallbackLease->MarkPublished();
		for (const auto& Atom : Pending.Resources->Atoms) Atom->MetadataLease->MarkPublished();
		ReleasePreparation(Pending);
		AdvanceCoarseAdmissionVersion();
		RemoveReady(PendingIndex);
		++SealedCount;
	}
	if (!SealedCount) return LastSealedSnapshot;
	LastFrame = InFrameCounter;
	LastSealedSnapshot = Snapshot;
	Snapshot->Lease->MarkPublished();
#if !UE_BUILD_SHIPPING
	if (SealedCount > 1)
		UE_LOG(LogTemp, Display, TEXT("Native batch sealed: serial=%llu transactions=%d prepared=%d ready=%d"),
			Snapshot->Serial, SealedCount, Entries.Num(), Ready.Num());
#endif
	return Snapshot;
}

bool FVoxelTerrainRendererState::PrepareMetadata()
{
	if (MetadataLease) return true;
	if (MetadataAdmission != EVoxelTerrainResourceAdmission::None && MetadataAdmission != EVoxelTerrainResourceAdmission::CapacityBlocked)
		return false;
	if (!MetadataConsumer.IsValid())
	{
		if (NextMetadataSerial == MAX_uint64)
		{
			MetadataAdmission = EVoxelTerrainResourceAdmission::CapacityUnachievable;
			return false;
		}
		MetadataConsumer = FVoxelGenerationConsumerKey::Section(Epoch, OwnerId, 1, {}, ++NextMetadataSerial);
	}
	if (MetadataAdmission == EVoxelTerrainResourceAdmission::CapacityBlocked &&
		!RendererHasExternalRelease(Resources, MetadataConsumer, MetadataCapacityVersion)) return false;
	const uint64 Bytes = VoxelBoundedAllocation::ArrayBytes<FReadyTransaction>(VoxelNativePublicationCapacity) +
		VoxelBoundedAllocation::MapBytes<FGuid, TFunction<void()>>(VoxelNativePublicationCapacity) +
		VoxelBoundedAllocation::SetBytes<FGuid>(32768) +
		VoxelBoundedAllocation::ArrayBytes<FFrozenPublication>(VoxelNativePublicationCapacity) + VoxelBoundedAllocation::ArrayBytes<FSnapshotPtr>(8);
	MetadataCapacityVersion = Resources->ReleaseVersionExcludingOwner(MetadataConsumer);
	MetadataAdmission = Resources->TryAcquire(MetadataConsumer, EVoxelTerrainResourcePool::Resident, Bytes, MetadataLease);
	if (MetadataAdmission != EVoxelTerrainResourceAdmission::Granted) return false;
	Ready.Reserve(VoxelNativePublicationCapacity);
	SubmittedCallbacks.Reserve(VoxelNativePublicationCapacity);
	PendingCoarseSources.Reserve(32768);
	PendingIssued.Reserve(VoxelNativePublicationCapacity);
	RetiringBoundSnapshots.Reserve(8);
	MetadataLease->MarkPublished();
	return true;
}

FSnapshotPtr FVoxelTerrainRendererState::BindFrameSnapshot(const uint64 InFrameCounter)
{
	check(IsInGameThread());
	if (LastBoundFrame == InFrameCounter) return BoundSnapshot;
	LastBoundFrame = InFrameCounter;
	if (BoundSnapshot == LastSealedSnapshot || !MetadataLease || RetiringBoundSnapshots.Num() == 8) return BoundSnapshot;
	if (BoundSnapshot) RetiringBoundSnapshots.Add(MoveTemp(BoundSnapshot));
	BoundSnapshot = LastSealedSnapshot;
	return BoundSnapshot;
}

void FVoxelTerrainRenderer::AdvanceFramePublication()
{
	if (!EnsureState()) return;
	auto& Context = State->Module.GetTerrainFrameContext();
	const double Deadline = Context.Deadline();
	while (!State->RetiringBoundSnapshots.IsEmpty() && FPlatformTime::Seconds() < Deadline)
	{
		auto Retiring = State->RetiringBoundSnapshots.Pop(EAllowShrinking::No);
		Retiring.Reset();
	}
	const int32 FirstPhase = State->NextPublicationPhase;
	State->NextPublicationPhase = (FirstPhase + 1) % 3;
	for (int32 Offset = 0; Offset < 3 && FPlatformTime::Seconds() < Deadline; ++Offset)
	{
		const int32 Phase = (FirstPhase + Offset) % 3;
		if (Phase == 0)
		{
			State->AdvanceRootPreparations(Deadline);
		}
		else if (Phase == 1)
		{
			State->AdvanceRootUpload(*this, Deadline);
		}
		else
		{
			// 容量等待只阻塞该事务；释放合并通道，让无需额外合并容量的其他局部根仍可封存。
			if (State->RootMerge)
			{
				const int32 Index = State->Ready.IndexOfByPredicate([this](const FReadyTransaction& Pending)
				{
					return Pending.Preparation == State->RootMerge->Preparation;
				});
				if (Index != INDEX_NONE && (State->RootMerge->bWaitingCapacity.Load() || State->Ready[Index].bSealCapacityBlocked))
				{
					if (State->RootMerge->bWaitingCapacity.Load())
					{
						State->Ready[Index].bMergeCapacityBlocked = true;
						State->Ready[Index].MergeCapacityVersion = State->RootMerge->CapacityVersion.Load();
					}
					State->RootMerge->bSuspended.Store(true);
					for (auto& Pending : State->Ready)
						if (State->RootMerge->Contains(Pending.Preparation))
						{
							Pending.SuspendedMerge = State->RootMerge;
							Pending.bMergeCapacityBlocked = State->Ready[Index].bMergeCapacityBlocked;
							Pending.MergeCapacityVersion = State->Ready[Index].MergeCapacityVersion;
						}
					State->RootMerge.Reset();
				}
				else if (Index == INDEX_NONE) State->ReleaseRootMerge();
			}
			State->SealFrameSnapshot(Context.GetFrameCounter(), Deadline);
		}
	}
}

void FVoxelTerrainRenderer::AdvanceRetirement(TFunction<void(const FIntVector&)> InOnRetired, const double InDeadline)
{
	if (!State || !State->bActive.Load() || !State->PresentedSnapshot) return;
	const double Deadline = FMath::Min(InDeadline, State->Module.GetTerrainFrameContext().Deadline(0.05));
	if (FPlatformTime::Seconds() >= Deadline) return;
	if (!State->CoarseRetirementSnapshot)
	{
		State->CoarseRetirementSnapshot = State->PresentedSnapshot;
		if (State->CoarseRetirementSnapshot->CoarseRoot) State->CoarseRetirementWalk.Add({State->CoarseRetirementSnapshot->CoarseRoot, 0});
	}
	const double CoarseDeadline = FMath::Min(Deadline, State->Module.GetTerrainFrameContext().Deadline(0.025));
	for (int32 Step = 0; Step < 32 && !State->CoarseRetirementWalk.IsEmpty() && FPlatformTime::Seconds() < CoarseDeadline; ++Step)
	{
		auto& Walk = State->CoarseRetirementWalk.Last();
		if (!Walk.Node->Item)
		{
			if (Walk.Child == 8) State->CoarseRetirementWalk.Pop(EAllowShrinking::No);
			else if (const auto Child = Walk.Node->Children[Walk.Child++]) State->CoarseRetirementWalk.Add({Child, 0});
			continue;
		}
		const auto Item = Walk.Node->Item;
		const auto Key = Item->Geometry->Stamp.GeometryIdentity.Key;
		const auto IsDemanded = [this, Key, Bounds = Item->OwnershipBounds]
		{
			const auto SnapshotNeeds = [&Key, &Bounds](const FVoxelInterestSet& Interest)
			{
				if (Interest.Geometry.Contains(Key)) return true;
				for (const auto& Pair : Interest.Sources)
				{
					if (!EnumHasAnyFlags(Pair.Value.Source.Capabilities, EVoxelStreamingCapability::FineVisual)) continue;
					const auto& Source = Pair.Value;
					if (Source.GetCurrentWindow().Serial && Bounds.Intersect(FBox(FVector(Source.GetCurrentWindow().Bounds.Min), FVector(Source.GetCurrentWindow().Bounds.Max)))) return true;
					if (Source.GetPendingWindow().IsSet() && Bounds.Intersect(FBox(FVector(Source.GetPendingWindow()->Bounds.Min), FVector(Source.GetPendingWindow()->Bounds.Max)))) return true;
				}
				return false;
			};
			return SnapshotNeeds(State->Module.GetCurrentInterest()) || SnapshotNeeds(*State->Module.GetDesiredInterestSnapshot());
		};
		auto& Attempt = Item->RetirementAttempt;
		if (Attempt.Transaction.IsValid())
		{
			if (ConsumeTransactionFailure(Attempt.Transaction))
			{
				Attempt.Transaction.Invalidate();
				Attempt.Admission = EVoxelTerrainResourceAdmission::CapacityUnachievable;
			}
			else if (IsTransactionPending(Attempt.Transaction))
			{
				State->CoarseRetirementWalk.Pop(EAllowShrinking::No);
				continue;
			}
			else
			{
				Attempt.Transaction.Invalidate();
				Attempt.Admission = EVoxelTerrainResourceAdmission::None;
			}
		}
		if (IsDemanded()) Attempt = {};
		else if (!State->PendingCoarseSources.Contains(Item->SourceId))
		{
			// 遍历期间呈现根可能已换代；旧版本不能删除同 Source 的新几何。
			if (!State->LastSealedSnapshot || !SameCoarseGeometry(FindCoarseItem(State->LastSealedSnapshot->CoarseRoot,
				Key, Item->SourceId), Item))
			{
				State->CoarseRetirementWalk.Pop(EAllowShrinking::No);
				continue;
			}
			const auto Limits = State->Resources->Snapshot().Limits;
			const bool bSameLimits = Attempt.Limits.InputBytes == Limits.InputBytes && Attempt.Limits.WorkingBytes == Limits.WorkingBytes &&
				Attempt.Limits.StagedBytes == Limits.StagedBytes && Attempt.Limits.ResidentBytes == Limits.ResidentBytes &&
				Attempt.Limits.OverlapBytes == Limits.OverlapBytes;
			if (Attempt.Consumer.IsValid() && !bSameLimits) Attempt = {};
			const bool bTerminal = Attempt.Admission == EVoxelTerrainResourceAdmission::CapacityUnachievable ||
				Attempt.Admission == EVoxelTerrainResourceAdmission::InvalidRequest;
			if (bTerminal || (Attempt.Admission == EVoxelTerrainResourceAdmission::CapacityBlocked &&
				GetCoarseAdmissionVersion() == Attempt.FrontendVersion &&
				!RendererHasExternalRelease(State->Resources, Attempt.Consumer, Attempt.CapacityVersion)))
			{
				State->CoarseRetirementWalk.Pop(EAllowShrinking::No);
				continue;
			}
			if (FPlatformTime::Seconds() >= CoarseDeadline) break;
			if (!Attempt.Consumer.IsValid())
			{
				if (State->NextCoarseAttemptSerial == MAX_uint64)
				{
					Attempt.Admission = EVoxelTerrainResourceAdmission::CapacityUnachievable;
					State->CoarseRetirementWalk.Pop(EAllowShrinking::No);
					continue;
				}
				Attempt.Consumer = FVoxelGenerationConsumerKey::Section(State->Epoch, State->OwnerId, 1,
					FIntVector(MIN_int32, 13, 0), ++State->NextCoarseAttemptSerial);
				Attempt.FirstQueuedSeconds = FPlatformTime::Seconds();
			}
			Attempt.Limits = Limits;
			Attempt.CapacityVersion = State->Resources->ReleaseVersionExcludingOwner(Attempt.Consumer);
			Attempt.FrontendVersion = GetCoarseAdmissionVersion();
			const auto MakeCurrent = [&](TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InLease)
			{
				return [IsDemanded, Lease = MoveTemp(InLease)] { return !IsDemanded(); };
			};
			auto CurrentPrototype = MakeCurrent(nullptr);
			const uint64 CallbackBytes = VoxelBoundedAllocation::FunctionBytes<decltype(CurrentPrototype), false>();
			const FVoxelTerrainCoarseMutation Change{Item, true};
			const FVoxelRendererCoarseSources SourceContext{*this, State->Resources.Get(), MakeArrayView(&Change, 1)};
			const FVoxelGenerationRequiredSourceScope SourceScope(&State->Resources.Get(), &SourceContext, &FVoxelRendererCoarseSources::Enumerate);
			TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> CallbackLease;
			if (CallbackBytes)
			{
				FVoxelGenerationRequiredCapacity Required;
				FString Error;
				if (!FVoxelGenerationRequiredSourceScope::Collect(State->Resources.Get(), nullptr, Required, Error))
				{
					Attempt.Admission = EVoxelTerrainResourceAdmission::CapacityUnachievable;
					State->CoarseRetirementWalk.Pop(EAllowShrinking::No);
					continue;
				}
				Attempt.Admission = State->Resources->TryAcquire(Attempt.Consumer, EVoxelTerrainResourcePool::Input, CallbackBytes,
					CallbackLease, 0, Required.Bytes[static_cast<int32>(EVoxelTerrainResourcePool::Input)], Required.LeaseCount);
				if (Attempt.Admission != EVoxelTerrainResourceAdmission::Granted)
				{
					State->CoarseRetirementWalk.Pop(EAllowShrinking::No);
					continue;
				}
			}
			FVoxelTerrainServicePriority Priority;
			Priority.Band = EVoxelTerrainServiceBand::P6;
			PrepareCoarseTransaction(Item, true, TFunction<bool()>(MakeCurrent(CallbackLease)), {}, Attempt.Transaction,
				Priority, Attempt.FirstQueuedSeconds, &Attempt.Admission, Attempt.Consumer, CallbackLease);
		}
		State->CoarseRetirementWalk.Pop(EAllowShrinking::No);
	}
	if (State->CoarseRetirementWalk.IsEmpty())
	{
		State->CoarseRetirementSnapshot.Reset();
	}
	const auto IsDemanded = [this](const FIntVector& Section)
	{
		const auto SnapshotDemands = [&Section](const FVoxelInterestSet& Interest)
		{
			if (const auto* Exact = Interest.Exact.Find(Section); Exact && Exact->bFineRender) return true;
			for (const int32 Side : {32, 64})
			{
				FIntVector Min;
				if (FVoxelFineSpatial::TrySnap(Section * 16, Side, Min) && Interest.FineShardDemands.Contains({Min, Side})) return true;
			}
			return false;
		};
		return SnapshotDemands(State->Module.GetCurrentInterest()) ||
			SnapshotDemands(*State->Module.GetDesiredInterestSnapshot());
	};
	const auto CanRetireFine = [this, IsDemanded](const FIntVector& Section)
	{
		if (IsDemanded(Section)) return false;
		const auto HasCoarseDemand = [&Section](const FVoxelInterestSet& Interest)
		{
			for (uint8 Level = 0; Level <= VoxelStreamingGeometry::MaximumSpatialLevel; ++Level)
			{
				const int64 Side = int64(1) << Level;
				const FIntVector Coordinate(int32(FVoxelFineSpatial::FloorDivide(Section.X, Side)),
					int32(FVoxelFineSpatial::FloorDivide(Section.Y, Side)), int32(FVoxelFineSpatial::FloorDivide(Section.Z, Side)));
				if (Interest.Geometry.Contains({EVoxelTerrainRepresentation::VoxelProxy, Coordinate, Level}) ||
					Interest.Geometry.Contains({EVoxelTerrainRepresentation::Surface, FIntVector(Coordinate.X, Coordinate.Y, 0), Level}) ||
					Interest.Geometry.Contains({EVoxelTerrainRepresentation::Macro, FIntVector(Coordinate.X, Coordinate.Y, 0), Level})) return true;
			}
			return false;
		};
		return (!HasCoarseDemand(State->Module.GetCurrentInterest()) && !HasCoarseDemand(*State->Module.GetDesiredInterestSnapshot())) ||
			(State->LastSealedSnapshot && FindCoarseEvidence(State->LastSealedSnapshot->CoarseRoot, Section));
	};
	if (!State->RetirementSnapshot)
	{
		State->RetirementSnapshot = State->PresentedSnapshot;
		if (State->RetirementSnapshot->Root) State->RetirementWalk.Add({State->RetirementSnapshot->Root, 0});
	}
	for (int32 Step = 0; Step < 64 && !State->RetirementWalk.IsEmpty() && FPlatformTime::Seconds() < Deadline; ++Step)
	{
		auto& Walk = State->RetirementWalk.Last();
		if (!Walk.Node->Atom)
		{
			if (Walk.Child == 8) State->RetirementWalk.Pop(EAllowShrinking::No);
			else if (const auto Child = Walk.Node->Children[Walk.Child++]) State->RetirementWalk.Add({Child, 0});
			continue;
		}
		const FIntVector Section = Walk.Node->Atom->Section;
		if (!CanRetireFine(Section) || !State->Handoff.FindPresentedAtom(Section) || State->Handoff.IsLocked(Section))
		{
			State->RetirementWalk.Pop(EAllowShrinking::No);
			continue;
		}
		if (FPlatformTime::Seconds() >= Deadline) break;
		check(State->NextMetadataSerial < MAX_uint64);
		const uint64 Serial = ++State->NextMetadataSerial;
		const auto Consumer = FVoxelGenerationConsumerKey::Section(State->Epoch, State->OwnerId, 1, Section, Serial);
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> AtomLease;
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ProofLease;
		if (TryAcquirePublicationResident(State->Resources, Consumer,
			VoxelBoundedAllocation::WorkerSharedBytes<FVoxelTerrainRenderAtom>(), AtomLease, 0, true) != EVoxelTerrainResourceAdmission::Granted ||
			TryAcquirePublicationResident(State->Resources, Consumer,
				VoxelBoundedAllocation::WorkerSharedBytes<FRetirementProof>(), ProofLease, 0, true) != EVoxelTerrainResourceAdmission::Granted) return;
		auto Atom = VoxelBoundedAllocation::MakeWorkerShared<FVoxelTerrainRenderAtom>(AtomLease);
		auto Proof = VoxelBoundedAllocation::MakeWorkerShared<FRetirementProof>(ProofLease);
		Proof->Lease = MoveTemp(ProofLease);
		Atom->Section = Section;
		Atom->MetadataLease = MoveTemp(AtomLease);
		Atom->Evidence.Owner = {EVoxelFineOwnerKind::None, State->OwnerId, Serial};
		Atom->Evidence.Content.WorldEpoch = State->Epoch;
		Atom->Evidence.Content.GeneratorIdentity = State->Module.GetManifest().BaseSampleHash;
		Atom->Evidence.Content.GeometryIdentity = FGuid::NewGuid();
		Atom->Evidence.bGeometryReady = Atom->Evidence.bCanStage = Atom->Evidence.bKnownEmpty = true;
		Atom->Evidence.GeometryLease = Proof;
		const TSharedPtr<const FVoxelTerrainRenderAtom, ESPMode::ThreadSafe> Immutable = Atom;
		FGuid Transaction;
		if (!PrepareAtomTransaction(MakeArrayView(&Immutable, 1), State->Module.GetInterestRevision(), false,
			[CanRetireFine](const FIntVector& Key, const FVoxelFineOwnerEvidence&) { return CanRetireFine(Key); },
			[](const FIntVector&, const FVoxelFineOwnerEvidence&) { return true; },
			[Section, InOnRetired] { if (InOnRetired) InOnRetired(Section); }, Transaction)) return;
		State->RetirementWalk.Pop(EAllowShrinking::No);
	}
	if (State->RetirementWalk.IsEmpty()) State->RetirementSnapshot.Reset();
}

FRetiringFrame& FVoxelTerrainRendererState::RegisterFrameSubmission(const FSnapshotPtr& InSnapshot,
	const FFrameSubmissionData& InData)
{
	check(IsInGameThread() && InSnapshot && InSnapshot->WorldEpoch == Epoch && InSnapshot->RendererOwner == OwnerId);
	auto& Frame = InSnapshot->Retirement;
	if (!InSnapshot->bRetirementRegistered)
	{
		Frame.Snapshot = InSnapshot;
		InSnapshot->bRetirementRegistered = true;
		InSnapshot->RetirementPrevious = RetiringFrameTail;
		if (RetiringFrameTail) RetiringFrameTail->RetirementNext = InSnapshot.Get();
		else RetiringFrameHead = InSnapshot.Get();
		RetiringFrameTail = InSnapshot.Get();
		if (!RetiringFrameCursor) RetiringFrameCursor = RetiringFrameHead;
		++RetiringFrameCount;
	}
	Frame.IncludeGpuSubmission(InData);
	Frame.bHasReceipts |= InData.bHasReceipts;
	Frame.RenderFence.BeginFence(FRenderCommandFence::ESyncDepth::RHIThread);
	return Frame;
}

const FRenderSnapshot* FVoxelTerrainRendererState::NextRetiringFrame()
{
	if (!RetiringFrameCursor) RetiringFrameCursor = RetiringFrameHead;
	const auto* Result = RetiringFrameCursor;
	if (Result) RetiringFrameCursor = Result->RetirementNext ? Result->RetirementNext : RetiringFrameHead;
	return Result;
}

void FVoxelTerrainRendererState::RemoveRetiringFrame(const FRenderSnapshot& InSnapshot)
{
	check(IsInGameThread() && InSnapshot.bRetirementRegistered);
	if (InSnapshot.RetirementPrevious) InSnapshot.RetirementPrevious->RetirementNext = InSnapshot.RetirementNext;
	else RetiringFrameHead = InSnapshot.RetirementNext;
	if (InSnapshot.RetirementNext) InSnapshot.RetirementNext->RetirementPrevious = InSnapshot.RetirementPrevious;
	else RetiringFrameTail = InSnapshot.RetirementPrevious;
	if (RetiringFrameCursor == &InSnapshot)
		RetiringFrameCursor = InSnapshot.RetirementNext ? InSnapshot.RetirementNext : RetiringFrameHead;
	InSnapshot.RetirementPrevious = InSnapshot.RetirementNext = nullptr;
	InSnapshot.bRetirementRegistered = false;
	--RetiringFrameCount;
	// 最后引用只转交预授予的 Worker deleter；不会在 GT 析构整棵根和全部旧 Atom。
	const auto Retiring = MoveTemp(InSnapshot.Retirement.Snapshot);
}

void FVoxelTerrainRendererState::AdvanceFrameReceipt(FRetiringFrame& Retiring, const bool bInClosing)
{
	check(IsInGameThread() && Retiring.bHasReceipts && Retiring.Snapshot->WorldEpoch == Epoch &&
		Retiring.Snapshot->RendererOwner == OwnerId);
	const auto& Frame = Retiring.Snapshot;
	const auto& Frozen = Frame->Issued[Retiring.ReceiptCursor];
	if (!bInClosing && PresentedSnapshot && Frame->Serial == PresentedSnapshot->PresentationSerial)
		Frozen.Resources->Lease->Transfer(EVoxelTerrainResourcePool::Resident);
	if (Frozen.Resources->bReceiptSettled.Load() ||
		(Frozen.Resources->ReceiptOwnerFrameSerial && Frozen.Resources->ReceiptOwnerFrameSerial != Frame->Serial))
	{
		++Retiring.ReceiptCursor;
		return;
	}
	Frozen.Resources->ReceiptOwnerFrameSerial = Frame->Serial;
	const FGuid Transaction = Frozen.Resources->Transaction;
	if (!Frozen.Resources->bReceiptOwnershipCommitted)
	{
		if (Frozen.Handoff && !bInClosing)
		{
			FVoxelFineRenderReceipt Receipt;
			Receipt.Atoms = MoveTemp(Retiring.ReceiptScratch);
			Receipt.Atoms.Reset();
			Receipt.TransactionId = Transaction;
			Receipt.Stamp = Frozen.Handoff->Stamp;
			check(Receipt.Atoms.Max() >= Frozen.Handoff->Transaction.Changes.Num());
			check(Retiring.PreviousAtoms.Max() - Retiring.PreviousAtoms.Num() >= Frozen.Handoff->Transaction.Changes.Num());
			for (const auto& Change : Frozen.Handoff->Transaction.Changes)
				Receipt.Atoms.Add({Change.Section, Change.After.Owner, Change.BeforeGeneration + 1, Change.After.Content, Change.After.bKnownEmpty});
			FString Error;
			const auto Disposition = Handoff.CommitRenderReceipt(Receipt, Retiring.PreviousAtoms, Error);
			Retiring.ReceiptScratch = MoveTemp(Receipt.Atoms);
			if (Disposition != EVoxelFineReceiptDisposition::Applied && Disposition != EVoxelFineReceiptDisposition::Superseded &&
				Disposition != EVoxelFineReceiptDisposition::Duplicate) return;
			check(Retiring.SettledTransactions.Num() < Retiring.SettledTransactions.Max());
			Retiring.SettledTransactions.Add(Transaction);
		}
		Frozen.Resources->bReceiptOwnershipCommitted = true;
		return;
	}
	if (Frozen.Resources->Atoms.IsValidIndex(Frozen.Resources->ReceiptAtomCursor))
	{
		const auto& Atom = Frozen.Resources->Atoms[Frozen.Resources->ReceiptAtomCursor++];
		if (Atom->Evidence.GeometryLease && Atom->Evidence.GeometryLease->WorkReceipt)
			Atom->Evidence.GeometryLease->WorkReceipt->Resolve(true);
		return;
	}
	if (Frozen.Resources->CoarseChanges.IsValidIndex(Frozen.Resources->ReceiptCoarseCursor))
	{
		const auto& Change = Frozen.Resources->CoarseChanges[Frozen.Resources->ReceiptCoarseCursor++];
		if (!Change.bRemove)
		{
			if (Change.Item->Geometry->WorkReceipt) Change.Item->Geometry->WorkReceipt->Resolve(true);
			if (Change.Item->Geometry->Source->WorkReceipt) Change.Item->Geometry->Source->WorkReceipt->Resolve(true);
			if (Change.Item->Geometry->Source->InputWorkReceipt) Change.Item->Geometry->Source->InputWorkReceipt->Resolve(true);
			if (Change.Item->Patch && Change.Item->Patch->WorkReceipt) Change.Item->Patch->WorkReceipt->Resolve(true);
		}
		AdvanceCoarseAdmissionVersion();
		PendingCoarseSources.Remove(Change.Item->SourceId);
		return;
	}
	if (Frozen.Resources->PreparationReceipt) Frozen.Resources->PreparationReceipt->Resolve(true);
	if (Frozen.Resources->MergeReceipt) Frozen.Resources->MergeReceipt->Resolve(true);
	if (!bInClosing)
	{
		const int32 PendingIndex = Frozen.Resources->PendingIssuedIndex;
		checkf(PendingIssued.IsValidIndex(PendingIndex) && PendingIssued[PendingIndex].Resources == Frozen.Resources,
			TEXT("Publication receipt owner mismatch: epoch=%llu owner=%s frame=%llu cursor=%d transaction=%s index=%d pending=%d receiptOwner=%llu actualTransaction=%s"),
			Epoch, *OwnerId.ToString(), Frame->Serial, Retiring.ReceiptCursor, *Transaction.ToString(), PendingIndex,
			PendingIssued.Num(), Frozen.Resources->ReceiptOwnerFrameSerial,
			PendingIssued.IsValidIndex(PendingIndex) ? *PendingIssued[PendingIndex].Resources->Transaction.ToString() : TEXT("none"));
		PendingIssued.RemoveAtSwap(PendingIndex, 1, EAllowShrinking::No);
		if (PendingIssued.IsValidIndex(PendingIndex))
			PendingIssued[PendingIndex].Resources->PendingIssuedIndex = PendingIndex;
	}
	Frozen.Resources->PendingIssuedIndex = INDEX_NONE;
	Frozen.Resources->bReceiptSettled.Store(true);
	++Retiring.ReceiptCursor;
	if (!bInClosing)
	{
		TFunction<void()> Callback;
		AdvanceCoarseAdmissionVersion();
		SubmittedCallbacks.RemoveAndCopyValue(Transaction, Callback);
		if (Callback) Callback();
	}
}

void FVoxelTerrainRenderer::PollRenderReceipts(const double InDeadlineSeconds)
{
	if (!State || !State->bActive.Load()) return;
	const double FrameDeadline = State->Module.GetTerrainFrameContext().Deadline();
	const double Deadline = InDeadlineSeconds > 0.0 ? FMath::Min(FrameDeadline, InDeadlineSeconds) : FrameDeadline;
	if (FPlatformTime::Seconds() >= Deadline) return;
	auto& Budget = State->Module.GetTerrainFrameContext().GetPublishBudget();
	const auto Run = [this, &Budget, Deadline](const EVoxelPublishPhase InPhase, double& InOutEstimate, auto&& InOperation)
	{
		const auto& Policy = State->Module.GetViewSettings().Budget;
		const double Floor = Policy.PublishOperationEstimateFloorMilliseconds;
		const double Ceiling = FMath::Min(double(Policy.PublishOperationEstimateCeilingMilliseconds), double(Policy.TerrainPublishMilliseconds) * 0.5);
		InOutEstimate = FMath::Clamp(InOutEstimate, Floor, Ceiling);
		const double Started = FPlatformTime::Seconds();
		if (Started >= Deadline || Started + InOutEstimate * 0.001 > Deadline ||
			!Budget.TryStart(InPhase, Started, InOutEstimate)) return false;
		InOperation();
		const double Finished = FPlatformTime::Seconds();
		Budget.Finish(InPhase, Started, Finished);
		InOutEstimate = FMath::Clamp(FMath::Lerp(InOutEstimate, (Finished - Started) * 1000.0, 0.15), Floor, Ceiling);
		return true;
	};
	const auto SettleReceipts = [this, Deadline, &Run](FRetiringFrame& Retiring)
	{
		const auto& Frame = Retiring.Snapshot;
		if (!Retiring.bHasReceipts || Retiring.bReceiptsSettled || Frame->WorldEpoch != State->Epoch) return;
		// 真实 RT 回执一次替换完整只读根，包含同批 Fine、粗范围及局部接口。
		if (!State->PresentedSnapshot || Frame->Serial > State->PresentedSnapshot->PresentationSerial)
		{
			if (!Run(EVoxelPublishPhase::Commit, State->ReceiptEstimateMilliseconds, [&]
			{
				check(!Frame->Issued.IsEmpty());
				const auto& Last = Frame->Issued.Last().Resources;
				auto Lease = Last->Lease->Split(VoxelBoundedAllocation::WorkerSharedBytes<FPresentedSnapshot>());
				check(Lease);
				auto Snapshot = VoxelBoundedAllocation::MakeWorkerShared<FPresentedSnapshot>(Lease);
				Snapshot->Lease = MoveTemp(Lease);
				Snapshot->WorldEpoch = State->Epoch;
				Snapshot->Serial = ++State->NextCoverageSerial;
				Snapshot->PresentationSerial = Frame->Serial;
				Snapshot->Root = Frame->Root;
				Snapshot->CoarseRoot = Frame->CoarseRoot;
				const auto Previous = MoveTemp(State->PresentedSnapshot);
				State->PresentedSnapshot = MoveTemp(Snapshot);
				State->PresentedFrame = Frame;
			})) return;
		}
		if (Frame->Serial == State->PresentedSnapshot->PresentationSerial)
		{
			Frame->Lease->Transfer(EVoxelTerrainResourcePool::Resident);
			State->PresentedSnapshot->Lease->Transfer(EVoxelTerrainResourcePool::Resident);
		}
		for (int32 Count = 0; Count < 16 && Retiring.ReceiptCursor < Frame->Issued.Num() && FPlatformTime::Seconds() < Deadline; ++Count)
		{
			if (!Run(EVoxelPublishPhase::Commit, State->ReceiptEstimateMilliseconds, [&]
			{
				State->AdvanceFrameReceipt(Retiring, false);
			})) break;
		}
		Retiring.bReceiptsSettled = Retiring.ReceiptCursor == Frame->Issued.Num();
	};
	FSnapshotPtr SubmittedSnapshot;
	FFrameSubmissionData Submission;
	for (int32 Count = 0; Count < 8 && FPlatformTime::Seconds() < Deadline && !State->Submissions.IsEmpty(); ++Count)
	{
		FRetiringFrame* Retiring = nullptr;
		if (!Run(EVoxelPublishPhase::Commit, State->SubmissionEstimateMilliseconds, [&]
		{
			if (State->Submissions.TryConsume(SubmittedSnapshot, Submission))
				Retiring = &State->RegisterFrameSubmission(SubmittedSnapshot, Submission);
		}) || !Retiring) break;
		SubmittedSnapshot.Reset();
		SettleReceipts(*Retiring);
	}
	const int32 FrameChecks = FMath::Min(32, State->RetiringFrameCount);
	for (int32 Count = 0; Count < FrameChecks && FPlatformTime::Seconds() < Deadline; ++Count)
	{
		const auto* Snapshot = State->NextRetiringFrame();
		if (!Snapshot) break;
		auto& Frame = Snapshot->Retirement;
		SettleReceipts(Frame);
		if (!Run(EVoxelPublishPhase::Retire, State->FrameRetirementEstimateMilliseconds, [&]
		{
			if (!Frame.bReceiptsSettled || !Frame.Snapshot.IsUnique() || !Frame.RenderFence.IsFenceComplete() ||
				!Frame.AreGpuFencesComplete()) return;
			for (int32 Step = 0; Step < 16 && Frame.ForgetCursor < Frame.SettledTransactions.Num(); ++Step)
				State->Handoff.ForgetSettledReceipt(Frame.SettledTransactions[Frame.ForgetCursor++]);
			if (Frame.ForgetCursor == Frame.SettledTransactions.Num()) State->RemoveRetiringFrame(*Snapshot);
		})) break;
	}
	// 只晋升真实呈现根的资源。未封存输出及仍在 Fence 等待的旧帧不借此逃出 overlap 配额。
	if (!State->PresentedSnapshot) return;
	if (!Run(EVoxelPublishPhase::Commit, State->PromotionEstimateMilliseconds, [&]
	{
		if (State->ResidentPromotion && State->ResidentPromotion->Snapshot != State->PresentedSnapshot)
			State->ResidentPromotion->Rebase(State->PresentedSnapshot, State->PresentedFrame);
		if (!State->ResidentPromotion)
		{
			TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
			const auto Consumer = FVoxelGenerationConsumerKey::Section(State->Epoch, State->OwnerId, 1,
				FIntVector(MIN_int32, 9, 0), State->PresentedSnapshot->PresentationSerial);
			if (State->Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Input,
				RenderRootAllocationBytes<FResidentPromotion>(), Lease))
				State->ResidentPromotion = CreateRenderRoot<FResidentPromotion>(MoveTemp(Lease), State->PresentedSnapshot, State->PresentedFrame, State->CellSize);
		}
	})) return;
	for (int32 Step = 0; State->ResidentPromotion && Step < 32 && FPlatformTime::Seconds() < Deadline; ++Step)
		if (!Run(EVoxelPublishPhase::Commit, State->PromotionEstimateMilliseconds,
			[this] { State->ResidentPromotion->Advance(); })) break;
}
const FVoxelFinePresentedAtom* FVoxelTerrainRenderer::FindPresentedAtom(const FIntVector& InSection) const
{
	return State ? State->Handoff.FindPresentedAtom(InSection) : nullptr;
}

TSharedPtr<const IVoxelTerrainPresentedSnapshot, ESPMode::ThreadSafe> FVoxelTerrainRenderer::CapturePresentedSnapshot() const
{
	return State ? State->PresentedSnapshot : nullptr;
}

bool FVoxelTerrainRenderer::HasPresentedCoverage(const FBox& InCellBounds) const
{
	if (!State || !State->PresentedSnapshot || !InCellBounds.IsValid ||
		State->PresentedSnapshot->WorldEpoch != State->Epoch) return false;
	const auto FineVisit = [&](auto&& Self, const FAtomNodePtr& Node) -> bool
	{
		if (!Node || !FBox(Node->Bounds.Min / State->CellSize, Node->Bounds.Max / State->CellSize).Intersect(InCellBounds)) return false;
		if (Node->Atom) return Node->Atom->Evidence.bGeometryReady;
		for (const auto& Child : Node->Children) if (Self(Self, Child)) return true;
		return false;
	};
	const auto CoarseVisit = [&](auto&& Self, const FCoarseNodePtr& Node) -> bool
	{
		if (!Node || !Node->CellBounds.Intersect(InCellBounds)) return false;
		if (Node->Item) return !Node->Item->bBoundaryOnly && Node->Item->Evidence.bGeometryReady;
		for (const auto& Child : Node->Children) if (Self(Self, Child)) return true;
		return false;
	};
	return FineVisit(FineVisit, State->PresentedSnapshot->Root) || CoarseVisit(CoarseVisit, State->PresentedSnapshot->CoarseRoot);
}

bool FVoxelTerrainRenderer::IsLocked(const FIntVector& InSection) const
{
	return State && State->Handoff.IsLocked(InSection);
}

bool FVoxelTerrainRenderer::RequiresF0RuntimeData(const FIntVector& InSection,
	TFunctionRef<bool(const FIntVector&, const FVoxelFineOwnerEvidence&)> InIsReplacementCurrent) const
{
	if (State && !State->Handoff.IsLocked(InSection))
	{
		const auto* Presented = State->Handoff.FindPresentedAtom(InSection);
		// None 来自同帧粗层恢复事务的真实回执；Runtime/Physics/Simulation 的需求另有租约。
		if (Presented && Presented->Evidence.Owner.Kind == EVoxelFineOwnerKind::None) return false;
	}
	return State && !State->Handoff.CanReleaseF0(InSection, false, false, false, false, false,
		InIsReplacementCurrent);
}

bool FVoxelTerrainRenderer::IsBusy() const
{
	return State && (State->RootMerge || !State->Ready.IsEmpty() || !State->SubmittedCallbacks.IsEmpty());
}

void FVoxelTerrainRenderer::Reset()
{
	bStopped = true;
	if (!State || !State->bActive.Exchange(false)) return;
	State->Scheduler.CancelMatching([OwnerId = State->OwnerId, Epoch = State->Epoch](EVoxelTaskKind Kind, const FVoxelTaskStamp& Stamp)
	{
		return Kind == EVoxelTaskKind::BuildViewCoverage && Stamp.OwnerId == OwnerId && Stamp.OwnerIncarnation == 1 && Stamp.WorldEpoch == Epoch;
	});
	if (State->RootMerge)
	{
		State->RootMerge.Reset();
	}
	State->Extension.Reset();
	for (FReadyTransaction& Pending : State->Ready)
	{
		State->ReleasePreparation(Pending);
		TOptional<FVoxelFineHandoffTransaction> Canceled;
		State->Handoff.RequestCancel(Pending.Id, Canceled);
	}
	State->Ready.Reset();
	check(State->ActivePreparations.IsEmpty());
	State->SubmittedCallbacks.Reset();
	State->PendingCoarseSources.Reset();
	State->LastSealedSnapshot.Reset();
	State->BoundSnapshot.Reset();
	State->RetiringBoundSnapshots.Reset();
	State->PendingIssued.Reset();
	State->ResidentPromotion.Reset();
	State->PresentedFrame.Reset();
	State->PresentedSnapshot.Reset();
	State->RetirementSnapshot.Reset();
	State->RetirementWalk.Reset();
	State->CoarseRetirementSnapshot.Reset();
	State->CoarseRetirementWalk.Reset();
	if (auto* Component = State->Component.Get())
	{
		Component->UnregisterComponent();
		Component->TerrainState.Reset();
		State->ComponentPin.Reset(Component->DetachMaterialKeeper());
		Component->DestroyComponent();
	}
	State->Component.Reset();
	if (AActor* Host = State->Host.Get()) Host->Destroy();
	State->Host.Reset();
	// 关闭之后继续由独立退休服务消费已发行帧；不依赖已销毁的 Module。
	FTSTicker::GetCoreTicker().AddTicker(TEXT("VoxelTerrainRetirement"), 0.0f,
		TUniqueFunction<bool(float)>(FVoxelRendererRetirement{State}));
	State.Reset();
}

#include "../Tests/VoxelTerrainRendererSceneIsolationTests.inl"
