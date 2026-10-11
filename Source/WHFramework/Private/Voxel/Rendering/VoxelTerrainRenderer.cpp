#include "Voxel/Rendering/VoxelTerrainRenderer.h"

#include "Async/Async.h"
#include "Components/SceneComponent.h"
#include "Containers/Ticker.h"
#include "DynamicMeshBuilder.h"
#include "Engine/World.h"
#include "LocalVertexFactory.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialRenderProxy.h"
#include "MeshElementCollector.h"
#include "Misc/ScopeExit.h"
#include "PrimitiveSceneProxy.h"
#include "PrimitiveUniformShaderParametersBuilder.h"
#include "RenderCommandFence.h"
#include "RenderGraphBuilder.h"
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
		: VertexFactory(InFeatureLevel, "VoxelTerrainAtom")
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
	FMaterialRenderProxy* Material = nullptr;
	FMaterialRelevance Relevance;
	FBox WorldBounds = FBox(ForceInit);
	FBox LocalBounds = FBox(ForceInit);
	FMatrix LocalToWorld = FMatrix::Identity;
	TSharedPtr<const FVoxelSectionMeshResult, ESPMode::ThreadSafe> Source;
	TSharedPtr<const FVoxelTerrainRenderChunk, ESPMode::ThreadSafe> NextSourceChunk;
	int32 FirstSourceTriangle = 0;
	int32 SourceBatch = INDEX_NONE;
	int32 SourceTriangleCount = 0;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResidentLease;
	TAtomic<bool> bReady{false};
	mutable TAtomic<bool> bResidentResourcesReady{false};
};

namespace
{
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

		void operator()(FRHICommandListImmediate& RHICmdList) const
		{
			const FVoxelMeshBuffers& Input = Mesh->Batches[Batch].Mesh;
			TArray<FDynamicMeshVertex> Vertices;
			Vertices.Reserve(VertexCount);
			for (int32 Corner = FirstTriangle * 3; Corner < (FirstTriangle + TriangleCount) * 3; ++Corner)
			{
				const int32 Index = Input.Triangles[Corner];
				FDynamicMeshVertex Vertex;
				Vertex.Position = FVector3f(Input.Vertices[Index] * WorldScale);
				Vertex.Color = Input.Colors[Index].ToFColor(true);
				Vertex.TextureCoordinate[0] = FVector2f(Input.UV0[Index]);
				Vertex.TextureCoordinate[1] = FVector2f(Input.UV1[Index]);
				Vertex.TextureCoordinate[2] = FVector2f(Input.UV2[Index]);
				const FVector3f TangentX(Input.Tangents[Index].TangentX);
				const FVector3f Normal(Input.Normals[Index]);
				const FVector3f TangentY = FVector3f::CrossProduct(Normal, TangentX) *
					(Input.Tangents[Index].bFlipTangentY ? -1.0f : 1.0f);
				Vertex.SetTangents(TangentX, TangentY, Normal);
				Vertices.Add(Vertex);
			}
			Chunk->IndexBuffer.Indices.Reserve(VertexCount);
			for (int32 Index = 0; Index < VertexCount; ++Index) Chunk->IndexBuffer.Indices.Add(uint32(Index));
			Chunk->VertexBuffers.StaticMeshVertexBuffer.SetUseFullPrecisionUVs(true);
			Chunk->VertexBuffers.StaticMeshVertexBuffer.SetUseHighPrecisionTangentBasis(true);
			Chunk->VertexBuffers.InitFromDynamicVertex(RHICmdList, &Chunk->VertexFactory, Vertices, 3);
			Chunk->IndexBuffer.InitResource(RHICmdList);
			Chunk->bReady.Store(true);
		}
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

		static FVoxelRenderChunkCapacity Calculate(const int32 VertexCount)
		{
			using FTangent = TStaticMeshVertexTangentDatum<FPackedRGBA16N>;
			using FTexCoord = TStaticMeshVertexUVsDatum<FVector2f>;
			using FController = SharedPointerInternals::TReferenceControllerWithDeleter<FVoxelTerrainRenderChunk,
				FVoxelRenderChunkDeleter, ESPMode::ThreadSafe>;
			FVoxelRenderChunkCapacity Capacity;
			Capacity.Resident = FMemory::QuantizeSize(sizeof(FVoxelTerrainRenderChunk), alignof(FVoxelTerrainRenderChunk)) +
				FMemory::QuantizeSize(sizeof(FController), alignof(FController)) +
				VoxelBoundedAllocation::FunctionBytes<FVoxelReleaseRenderChunkCommand>() +
				VertexStorageBytes<FPositionVertex>(VertexCount) + VertexStorageBytes<FColor>(VertexCount) +
				VertexStorageBytes<FTangent>(VertexCount) + VertexStorageBytes<FTexCoord>(VertexCount * 3) +
				VoxelBoundedAllocation::ArrayBytes<uint32>(VertexCount) +
				uint64(VertexCount) * (sizeof(FPositionVertex) + sizeof(FColor) + sizeof(FTangent) + 3 * sizeof(FTexCoord) + sizeof(uint32));
			Capacity.Working = VoxelBoundedAllocation::ArrayBytes<FDynamicMeshVertex>(VertexCount) +
				VoxelBoundedAllocation::FunctionBytes<FVoxelPrepareRenderChunkCommand>();
			return Capacity;
		}

		uint64 Resident = 0;
		uint64 Working = 0;
	};

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
		// 最多八个局部 Atom，另加 State、Component 和当前原生 Worker 控制来源。
		const FVoxelTerrainResourceLease* Leases[16] = {};
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
		return Lease->Bytes() > Limits.ResidentBytes && (!bAllowOverlap || Lease->Bytes() > Limits.OverlapBytes)
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
			const bool bGranted = ObserveAdmission(Resources->TryAcquire(Consumer, Pool, Bytes, OutLease, 0,
				Floor.Bytes, Floor.LeaseCount), OutError);
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
					Builder = MakeUnique<FVoxelTerrainCoarsePatchBuilder>(Resources, Consumer);
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
					!AcquireNative(EVoxelTerrainResourcePool::Staged, ItemBytes, ItemLease, Result.Error) ||
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
			return Result;
		}

		TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
		FVoxelGenerationConsumerKey Consumer;
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InputLease;
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ControlInputLease;
		TSharedPtr<FAtomPublication, ESPMode::ThreadSafe> Publication;
		FBox SchedulingBounds = FBox(ForceInit);
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
		FCoarsePtr Current;
		TSharedPtr<FVoxelTerrainCoarseExclusions, ESPMode::ThreadSafe> Exclusions;
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> BuilderLease;
		TUniquePtr<FVoxelTerrainCoarsePatchBuilder> Builder;
		TSharedPtr<FPreparedCoarse, ESPMode::ThreadSafe> FirstPrepared;
		TSharedPtr<FPreparedCoarse, ESPMode::ThreadSafe> LastPrepared;
		TSharedPtr<FPreparedCoarse, ESPMode::ThreadSafe> Upload;
		TSharedPtr<FPreparedFine, ESPMode::ThreadSafe> FirstFine, LastFine, FineBuild, FineUpload;
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

		void Begin(const TSharedPtr<FRootPreparation, ESPMode::ThreadSafe>& InPreparation,
			const FSnapshotPtr& InCurrent)
		{
			Preparation = InPreparation;
			Current = InCurrent;
			InputCheck = Preparation->FirstFine;
			Fine.Begin(Preparation->BaseSnapshot ? Preparation->BaseSnapshot->Root : nullptr,
				Current ? Current->Root : nullptr, Preparation->Root);
			Coarse.Begin(Preparation->BaseSnapshot ? Preparation->BaseSnapshot->CoarseRoot : nullptr,
				Current ? Current->CoarseRoot : nullptr, Preparation->CoarseRoot);
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
			if (!Floor.Add(Preparation->ControlInputLease, Resources.Get()) || !Floor.Add(InputLease, Resources.Get()) || !Floor.Add(Preparation->InputLease, Resources.Get()) ||
				!Floor.Add(Preparation->Publication->CoarseCallbackLease, Resources.Get()) ||
				!Floor.Add(Preparation->PrimaryPathLease, Resources.Get()) || !Floor.Add(Preparation->Publication->Lease, Resources.Get()) ||
				!Floor.Add(InPendingLease, Resources.Get()))
			{
				NativeAdmission = EVoxelTerrainResourceAdmission::InvalidRequest;
				return false;
			}
			NativeAdmission = TryAcquirePublicationResident(Resources, Consumer, Bytes, OutLease, 0,
				Preparation->bAllowResidentOverlap, Floor.Bytes, Floor.LeaseCount);
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
					if (!InputCheck->InputsMatch(Current, *Preparation->Publication))
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
					if (Progress == EVoxelPersistentMergeProgress::Complete) { Result.bSuccess = true; return Result; }
				}
				if (Progress == EVoxelPersistentMergeProgress::Conflict)
				{
					Result.Error = TEXT("DependencyNotReady:RenderRootConflict");
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
			return Result;
		}

		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InputLease;
		TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
		FVoxelGenerationConsumerKey Consumer;
		TSharedPtr<FRootPreparation, ESPMode::ThreadSafe> Preparation;
		FSnapshotPtr Current;
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
	bool BeginRootPreparation(FReadyTransaction& InTransaction);
	void AdvanceRootPreparations(double InDeadline);
	void AdvanceRootUpload(FVoxelTerrainRenderer& InRenderer, double InDeadline);
	void AdvanceRootUpload(FVoxelTerrainRenderer& InRenderer, FRootPreparation& InWork, int32 InMaximumSteps, double InDeadline);
	bool BeginRootMerge(FReadyTransaction& InTransaction);
	void ReleasePreparation(FReadyTransaction& InTransaction);
	void TerminateTransaction(FReadyTransaction& InTransaction);
	void ReleaseRootMerge();
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
	int32 PreparationDiagnosticCursor = 0;
	int32 UploadScanCursor = 0;
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
			const FFrameBinding* Binding = InFamily.GetExtentionData<FFrameBinding>();
			if (!Binding || !Binding->Snapshot || !Binding->bProxySubmitted.Load() || Binding->Snapshot->WorldEpoch != State->Epoch) return;
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
			bSupportsGPUScene = false;
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
			if (Binding && Binding->Snapshot && Binding->Snapshot->Root)
				Materials |= Binding->Snapshot->Root->MaterialRelevance;
			if (Binding && Binding->Snapshot && Binding->Snapshot->CoarseRoot)
				Materials |= Binding->Snapshot->CoarseRoot->MaterialRelevance;
			Materials.SetPrimitiveViewRelevance(Relevance);
			return Relevance;
		}

		virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& InViews,
			const FSceneViewFamily& InFamily, uint32 InVisibilityMap, FMeshElementCollector& InCollector) const override
		{
			const FFrameBinding* Binding = InFamily.GetExtentionData<FFrameBinding>();
			if (!Binding || !Binding->Snapshot || Binding->Snapshot->WorldEpoch != State->Epoch) return;
			const FSnapshotPtr Snapshot = Binding->Snapshot;
			if (InVisibilityMap) Binding->bProxySubmitted.Store(true);
			InCollector.AllocateOneFrameResource<FCollectorFrameLease>().Snapshot = Snapshot;
			for (int32 ViewIndex = 0; ViewIndex < InViews.Num(); ++ViewIndex)
			{
				if (!(InVisibilityMap & (1u << ViewIndex))) continue;
				int32 DrawnTriangles = 0;
				FBox DrawnBounds(ForceInit);
				const FVoxelTerrainRenderChunk* FirstDrawnChunk = nullptr;
				const auto Draw = [&](const TSharedPtr<const FVoxelTerrainRenderChunk, ESPMode::ThreadSafe>& Chunk,
					const int32 FirstTriangle, const int32 TriangleCount)
				{
					if (!Chunk || !Chunk->bReady.Load() || TriangleCount == 0 ||
						!InViews[ViewIndex]->ViewFrustum.IntersectBox(Chunk->WorldBounds.GetCenter(), Chunk->WorldBounds.GetExtent())) return;
					check(FirstTriangle >= 0 && (int64(FirstTriangle) + TriangleCount) * 3 <= Chunk->IndexBuffer.Indices.Num());
					FMeshBatch& Mesh = InCollector.AllocateMesh();
					FMeshBatchElement& Element = Mesh.Elements[0];
					Mesh.VertexFactory = &Chunk->VertexFactory;
					Mesh.MaterialRenderProxy = Chunk->Material;
					Mesh.Type = PT_TriangleList;
					Mesh.DepthPriorityGroup = SDPG_World;
					Element.IndexBuffer = &Chunk->IndexBuffer;
					Element.FirstIndex = FirstTriangle * 3;
					Element.NumPrimitives = TriangleCount;
					Element.MinVertexIndex = 0;
					Element.MaxVertexIndex = Chunk->VertexBuffers.PositionVertexBuffer.GetNumVertices() - 1;
					FPrimitiveUniformShaderParametersBuilder Builder;
					BuildUniformShaderParameters(Builder);
					Builder.LocalToWorld(Chunk->LocalToWorld).PreviousLocalToWorld(Chunk->LocalToWorld)
						.WorldBounds(FBoxSphereBounds(Chunk->WorldBounds)).LocalBounds(FBoxSphereBounds(Chunk->LocalBounds));
					auto& Uniform = InCollector.AllocateOneFrameResource<FDynamicPrimitiveUniformBuffer>();
					Uniform.Set(InCollector.GetRHICommandList(), Builder);
					Element.PrimitiveUniformBufferResource = &Uniform.UniformBuffer;
					InCollector.AddMesh(ViewIndex, Mesh);
					DrawnTriangles += TriangleCount;
					DrawnBounds += Chunk->WorldBounds;
					if (!FirstDrawnChunk) FirstDrawnChunk = Chunk.Get();
				};
				const auto FineVisit = [&](auto&& Self, const FAtomNodePtr& Node) -> void
				{
					if (!Node || !InViews[ViewIndex]->ViewFrustum.IntersectBox(Node->Bounds.GetCenter(), Node->Bounds.GetExtent())) return;
					if (Node->Atom)
					{
						if (Node->Atom->bUseDrawRanges)
							for (const auto& Range : Node->Atom->DrawRanges) Draw(Range.Chunk, Range.FirstTriangle, Range.TriangleCount);
						else for (const auto& Chunk : Node->Atom->Chunks)
							if (Chunk) Draw(Chunk, 0, Chunk->IndexBuffer.Indices.Num() / 3);
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
					const auto RangeVisit = [&](auto&& RangeSelf, const int32 RangeIndex,
						const TSharedPtr<const FVoxelTerrainCoarseDrawNode, ESPMode::ThreadSafe>& Patch) -> void
					{
						const auto& Range = Item.Geometry->Ranges[RangeIndex];
						const FBox RangeBounds(Range.WorldCellBounds.Min * State->CellSize, Range.WorldCellBounds.Max * State->CellSize);
						if (!InViews[ViewIndex]->ViewFrustum.IntersectBox(RangeBounds.GetCenter(), RangeBounds.GetExtent())) return;
						if (!Patch)
						{
							const int32 End = Range.FirstTriangle + Range.TriangleCount;
							for (auto Chunk = Item.SourceChunks[Range.Batch]; Chunk && Chunk->FirstSourceTriangle < End; Chunk = Chunk->NextSourceChunk)
							{
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
						else for (auto Piece = Patch->FirstPiece; Piece; Piece = Piece->Next)
							Draw(Piece->Chunk, 0, Piece->Chunk->IndexBuffer.Indices.Num() / 3);
					};
					for (int32 Root = 0; Root < Item.Geometry->Roots.Num(); ++Root)
						RangeVisit(RangeVisit, Item.Geometry->Roots[Root], Item.DrawRoots[Root]);
				};
				CoarseVisit(CoarseVisit, Snapshot->CoarseRoot);
#if !UE_BUILD_SHIPPING
				const double Now = FPlatformTime::Seconds();
				if (Now - LastDrawDiagnosticSeconds >= 5.0)
				{
					LastDrawDiagnosticSeconds = Now;
					const FPrimitiveViewRelevance Relevance = GetViewRelevance(InViews[ViewIndex]);
					UE_LOG(LogTemp, Display, TEXT("Voxel native draw: triangles=%d camera=%s bounds=%s main=%d opaque=%d masked=%d translucent=%d"),
						DrawnTriangles, *InViews[ViewIndex]->ViewLocation.ToString(), *DrawnBounds.ToString(),
						Relevance.bRenderInMainPass, Relevance.bOpaque, Relevance.bMasked, Relevance.HasTranslucency());
					if (FirstDrawnChunk)
					{
						UE_LOG(LogTemp, Display, TEXT("Voxel native mesh: material=%s firstVertex=%s origin=%s"),
							*FirstDrawnChunk->Material->GetFriendlyName(),
							*FVector(FirstDrawnChunk->VertexBuffers.PositionVertexBuffer.VertexPosition(0)).ToString(),
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
	const int32 InFirstTriangle, const int32 InTriangleCount, EVoxelTerrainResourceAdmission* OutAdmission)
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
	if (RequiredVertices > MAX_int32 / 3)
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
	Chunk->Source = InMesh;
	Chunk->FirstSourceTriangle = InFirstTriangle;
	Chunk->SourceBatch = InBatch;
	Chunk->SourceTriangleCount = TriangleCount;
	Chunk->ResidentLease = MoveTemp(ResidentLease);
	Chunk->Material = Material->GetRenderProxy();
	Chunk->Relevance = Material->GetRelevance_Concurrent(State->Module.GetWorld()->GetFeatureLevel());
	Chunk->LocalToWorld = FTransform(FQuat::Identity, InWorldOrigin).ToMatrixWithScale();
	Chunk->LocalBounds = FBox(InMesh->LocalBounds.Min * InWorldScale, InMesh->LocalBounds.Max * InWorldScale);
	Chunk->WorldBounds = FBox(InWorldOrigin + Chunk->LocalBounds.Min, InWorldOrigin + Chunk->LocalBounds.Max);
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
	if (State->Ready.Num() + State->SubmittedCallbacks.Num() >= 512) return Reject(EVoxelTerrainResourceAdmission::CapacityBlocked);
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
	const auto Admission = State->Resources->TryAcquire(Consumer, EVoxelTerrainResourcePool::Staged, MetadataBytes, Lease, 1,
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
	State->Ready.Add(MoveTemp(Ready));
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
	State->Ready.RemoveAtSwap(Index, 1, EAllowShrinking::No);
	return true;
}

bool FVoxelTerrainRendererState::BeginRootPreparation(FReadyTransaction& InTransaction)
{
	if (InTransaction.bTerminalFailure) return false;
	if (!InTransaction.Preparation && ActivePreparations.Num() >= 16) return false;
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
		InTransaction.Preparation->SchedulingBounds = InTransaction.Resources->GetChangedBounds().ExpandBy(32.0);
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
			Pinned->Ready.RemoveAtSwap(Index, 1, EAllowShrinking::No);
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
	for (int32 Step = 0; Step < 16 && FPlatformTime::Seconds() < Deadline; ++Step)
	{
		const double ScanStarted = FPlatformTime::Seconds();
		const double ScanDeadline = ScanStarted + (Deadline - ScanStarted) * 0.5;
		TArray<FVoxelTerrainServiceCandidate, TInlineAllocator<32>> Candidates;
		const auto AddCandidate = [&](const int32 Index)
		{
			auto& Pending = Ready[Index];
			if (Pending.Preparation && Pending.Preparation->UploadAdmission != EVoxelTerrainResourceAdmission::Granted &&
				Pending.Preparation->UploadAdmission != EVoxelTerrainResourceAdmission::CapacityBlocked)
			{
				TerminateTransaction(Pending);
				return;
			}
			const bool bRunnable = Pending.Preparation && Pending.Preparation->bDecodeComplete && !Pending.Preparation->bUploadComplete &&
				(Pending.Preparation->UploadAdmission == EVoxelTerrainResourceAdmission::Granted ||
					RendererHasExternalRelease(Resources, Pending.Preparation->Consumer, Pending.Preparation->UploadCapacityVersion));
			if (bRunnable) Candidates.Add({Index, Pending.Priority, Pending.FirstQueuedSeconds, Pending.Sequence, UploadCostMicroseconds, true});
		};
		int32 F0Candidates = 0;
		for (int32 Index = 0; Index < Ready.Num() && F0Candidates < 32 && FPlatformTime::Seconds() < ScanDeadline; ++Index)
		{
			const auto& Pending = Ready[Index];
			if (Pending.Resources->Atoms.IsEmpty() ||
				Pending.Resources->Atoms[0]->Evidence.Owner.Kind != EVoxelFineOwnerKind::F0) continue;
			const int32 Before = Candidates.Num();
			AddCandidate(Index);
			F0Candidates += Candidates.Num() != Before;
		}
		for (int32 Scan = 0; Scan < FMath::Min(32, Ready.Num()) && FPlatformTime::Seconds() < ScanDeadline; ++Scan)
		{
			UploadScanCursor %= Ready.Num();
			const int32 Index = UploadScanCursor++;
			const auto& Pending = Ready[Index];
			if (!Pending.Resources->Atoms.IsEmpty() &&
				Pending.Resources->Atoms[0]->Evidence.Owner.Kind == EVoxelFineOwnerKind::F0) continue;
			AddCandidate(Index);
		}
		FVoxelTerrainServiceCapacity Capacity;
		Capacity.bAllowOverdueOpportunity = Module.GetTerrainFrameContext().IsOverdueOpportunityAvailable();
		Capacity.MaximumWaitSeconds = Module.GetViewSettings().Fine.MaximumBackgroundWaitSeconds;
		FVoxelTerrainServiceCharge Charge;
		const int32 Index = UploadSelection.Select(Candidates, Capacity, FPlatformTime::Seconds(), Charge);
		if (Index == INDEX_NONE) break;
		if (FPlatformTime::Seconds() >= Deadline)
		{
			verify(UploadSelection.RefundBeforeStart(Charge.Serial));
			break;
		}
		if (Charge.bOverdueOpportunity) Module.GetTerrainFrameContext().ConsumeOverdueOpportunity();
		const double Started = FPlatformTime::Seconds();
		AdvanceRootUpload(Renderer, *Ready[Index].Preparation, 1, Deadline);
		const double Actual = FMath::Max(1.0, (FPlatformTime::Seconds() - Started) * 1000000.0);
		verify(UploadSelection.Settle(Charge.Serial, Actual));
		UploadCostMicroseconds = FMath::Lerp(UploadCostMicroseconds, Actual, 0.15);
	}
}

void FVoxelTerrainRendererState::AdvanceRootUpload(FVoxelTerrainRenderer& Renderer, FRootPreparation& Work,
	const int32 InMaximumSteps, const double InDeadline)
{
	const double Deadline = FMath::Min(InDeadline, Module.GetTerrainFrameContext().Deadline());
	if (Work.UploadAdmission == EVoxelTerrainResourceAdmission::CapacityBlocked &&
		!RendererHasExternalRelease(Work.Resources, Work.Consumer, Work.UploadCapacityVersion)) return;
	if (Work.UploadAdmission != EVoxelTerrainResourceAdmission::Granted &&
		Work.UploadAdmission != EVoxelTerrainResourceAdmission::CapacityBlocked) return;
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
	for (int32 Step = 0; Step < InMaximumSteps && FPlatformTime::Seconds() < Deadline; ++Step)
	{
		if (Work.FineUpload)
		{
			auto& Fine = *Work.FineUpload;
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
						RequiredInput, Work.InputLease ? 1 : 0), Before)) return;
					Fine.Item->DrawRanges.Reserve(Capacity);
					check(Fine.Item->DrawRanges.GetAllocatedSize() <= NewBytes);
					verify(Fine.Item->MetadataLease->Resize(Fine.Item->GetAllocatedBytes()));
				}
				if (!Transfer(Mesh->ResourceLease)) return;
				const int32 Count = FMath::Min(4096, Triangles - Fine.UploadTriangle);
				EVoxelTerrainResourceAdmission ChunkAdmission = EVoxelTerrainResourceAdmission::None;
				const uint64 Before = Work.Resources->ReleaseVersionExcludingOwner(Work.Consumer);
				auto Chunk = Renderer.PrepareChunk(Work.Consumer, Mesh, Fine.UploadBatch,
					Fine.Upload->Origin * Work.CellSize, Fine.Upload->Scale * Work.CellSize, Fine.UploadTriangle, Count, &ChunkAdmission);
				if (!Chunk) { ObserveAdmission(ChunkAdmission, Before); return; }
				Fine.Item->DrawRanges.Add({MoveTemp(Chunk), 0, Count});
				Fine.UploadTriangle += Count;
				continue;
			}
			if (!Fine.Item->IsResourceReady() || !Transfer(Fine.Item->MetadataLease)) return;
			Work.Root = SetAtom(Work.Root, Fine.Item, 31, Fine.PathLease, Work.CellSize);
			Work.FineUpload = Fine.Next;
			continue;
		}
		if (!Work.Upload) { Work.bUploadComplete = true; return; }
		auto& Item = *Work.Upload->Item;
		if (Work.DrawWalk.IsEmpty())
		{
			if (Work.UploadRoot == Item.DrawRoots.Num())
			{
				if (!Transfer(Item.MetadataLease) || !Transfer(Item.Patch->MetadataLease)) return;
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
				!Transfer(Walk.Patch->MetadataLease)) return;
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
				!Transfer(Walk.Piece->MetadataLease) || !Transfer(Walk.Piece->Mesh->ResourceLease)) return;
			EVoxelTerrainResourceAdmission ChunkAdmission = EVoxelTerrainResourceAdmission::None;
			auto Chunk = Renderer.PrepareChunk(Work.Consumer, Walk.Piece->Mesh, 0,
				Item.Geometry->WorldCellOrigin * Work.CellSize, Item.Geometry->CellScale * Work.CellSize, 0, INDEX_NONE, &ChunkAdmission);
			if (!Chunk) { ObserveAdmission(ChunkAdmission, Before); return; }
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
		if (Walk.LastPiece && !Walk.LastPiece->Chunk->bReady.Load()) return;
		CompleteDraw(Walk.Result);
	}
}

void FVoxelTerrainRendererState::ReleasePreparation(FReadyTransaction& InTransaction)
{
	if (InTransaction.SuspendedMerge)
	{
		const uint64 MergeToken = InTransaction.SuspendedMerge->Consumer.RequestSerial;
		Scheduler.CancelMatching([this, MergeToken](EVoxelTaskKind Kind, const FVoxelTaskStamp& Stamp)
		{
			return Kind == EVoxelTaskKind::BuildViewCoverage && Stamp.OwnerId == OwnerId &&
				Stamp.OwnerIncarnation == 1 && Stamp.WorldEpoch == Epoch && Stamp.Token == MergeToken;
		});
		InTransaction.SuspendedMerge.Reset();
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

void FVoxelTerrainRendererState::ReleaseRootMerge()
{
	if (!RootMerge) return;
	// 实际 Current 输入已变或合并被取消时结束原尝试，后续输入使用新消费者。
	if (auto* Pending = Ready.FindByPredicate([this](const FReadyTransaction& Candidate)
		{ return Candidate.Preparation == RootMerge->Preparation && Candidate.MergeConsumer == RootMerge->Consumer; }))
	{
		Pending->MergeConsumer = {};
		Pending->bMergeCapacityBlocked = false;
	}
	const uint64 Token = RootMerge->Consumer.RequestSerial;
	Scheduler.CancelMatching([this, Token](EVoxelTaskKind Kind, const FVoxelTaskStamp& Stamp)
	{
		return Kind == EVoxelTaskKind::BuildViewCoverage && Stamp.OwnerId == OwnerId &&
			Stamp.OwnerIncarnation == 1 && Stamp.WorldEpoch == Epoch && Stamp.Token == Token;
	});
	RootMerge.Reset();
}

void FVoxelTerrainRendererState::TerminateTransaction(FReadyTransaction& InTransaction)
{
	if (InTransaction.bTerminalFailure) return;
	InTransaction.bTerminalFailure = true;
	AdvanceCoarseAdmissionVersion();
	for (const auto& Change : InTransaction.Resources->CoarseChanges) PendingCoarseSources.Remove(Change.Item->SourceId);
	TOptional<FVoxelFineHandoffTransaction> Canceled;
	Handoff.RequestCancel(InTransaction.Id, Canceled);
	if (RootMerge && RootMerge->Preparation == InTransaction.Preparation) ReleaseRootMerge();
	ReleasePreparation(InTransaction);
}

void FVoxelTerrainRendererState::AdvanceRootPreparations(const double InDeadline)
{
	const double Deadline = FMath::Min(InDeadline, Module.GetTerrainFrameContext().Deadline());
	const double PriorityStarted = FPlatformTime::Seconds();
	const double PriorityDeadline = PriorityStarted + (Deadline - PriorityStarted) * 0.125;
	for (int32 Scan = 0; Scan < FMath::Min(32, Ready.Num()) && FPlatformTime::Seconds() < PriorityDeadline; ++Scan)
	{
		PreparationPriorityCursor %= Ready.Num();
		auto& Pending = Ready[PreparationPriorityCursor++];
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
		if (Priority.DistanceCells != MAX_dbl) Pending.Priority = Priority;
		if (Pending.Preparation) Pending.Preparation->SchedulingBand = Pending.Priority.Band;
	}
#if !UE_BUILD_SHIPPING
	static double NextPreparationDiagnostic = 0.0;
	if (FPlatformTime::Seconds() >= NextPreparationDiagnostic)
	{
		NextPreparationDiagnostic = FPlatformTime::Seconds() + 5.0;
		int32 Logged = 0;
		for (int32 Scan = 0; Scan < FMath::Min(32, Ready.Num()) && FPlatformTime::Seconds() < Deadline; ++Scan)
		{
			if (Logged == 8) break;
			PreparationDiagnosticCursor %= Ready.Num();
			const auto& Pending = Ready[PreparationDiagnosticCursor++];
			if (!Pending.Resources->Atoms.ContainsByPredicate([](const FAtomPtr& Atom)
				{ return Atom->Evidence.Owner.Kind == EVoxelFineOwnerKind::F0; })) continue;
			++Logged;
			const auto Work = Pending.Preparation;
			UE_LOG(LogTemp, Display, TEXT("Native birth publication: section=%s band=%d waiting=%d terminal=%d prep=%d phase=%d enqueued=%d decoded=%d uploaded=%d fineUpload=%d admission=%d merge=%d ready=%d"),
				*Pending.Resources->Atoms[0]->Section.ToString(), int32(Pending.Priority.Band), Pending.bPreparationAdmissionWaiting,
				Pending.bTerminalFailure, bool(Work), Work ? Work->Phase : -1, Work && Work->bEnqueued,
				Work && Work->bDecodeComplete, Work && Work->bUploadComplete, Work && Work->FineUpload,
				Work ? int32(Work->UploadAdmission) : -1, RootMerge && RootMerge->Preparation == Work, Ready.Num());
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
				Ready.RemoveAtSwap(Index, 1, EAllowShrinking::No);
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
		Ready.RemoveAtSwap(Index, 1, EAllowShrinking::No);
	}
	FVoxelTerrainServiceCapacity Capacity;
	Capacity.MaximumWorkers = 16;
	Capacity.NearReservedWorkers = Capacity.MinimumNonNearWorkers = 8;
	Capacity.MaximumWaitSeconds = Module.GetViewSettings().Fine.MaximumBackgroundWaitSeconds;
	FBox ActiveBounds[16];
	int32 ActiveBoundsCount = 0;
	for (const FRootPreparation* Preparation : ActivePreparations)
	{
		if (FPlatformTime::Seconds() >= Deadline) return;
		++Capacity.RunningWorkers;
		Capacity.RunningNearWorkers += FVoxelTerrainWorkSelection::IsNear(Preparation->SchedulingBand) ? 1 : 0;
		check(ActiveBoundsCount < UE_ARRAY_COUNT(ActiveBounds));
		ActiveBounds[ActiveBoundsCount++] = Preparation->SchedulingBounds;
	}
	for (int32 Step = 0; Step < 16 && !Ready.IsEmpty() && FPlatformTime::Seconds() < Deadline; ++Step)
	{
		const double ScanStarted = FPlatformTime::Seconds();
		const double ScanDeadline = ScanStarted + (Deadline - ScanStarted) * 0.5;
		Capacity.bAllowOverdueOpportunity = Module.GetTerrainFrameContext().IsOverdueOpportunityAvailable();
		TArray<FVoxelTerrainServiceCandidate, TInlineAllocator<32>> Candidates;
		const auto AddCandidate = [&](const int32 Index)
		{
			const auto& Pending = Ready[Index];
			if (Pending.bTerminalFailure || Pending.Preparation) return;
			const FBox Bounds = Pending.Resources->GetChangedBounds();
			for (int32 ActiveIndex = 0; ActiveIndex < ActiveBoundsCount; ++ActiveIndex)
				if (ActiveBounds[ActiveIndex].Intersect(Bounds)) return;
			if (Pending.bPreparationAdmissionWaiting &&
				!RendererHasExternalRelease(Resources, Pending.PreparationConsumer, Pending.PreparationAdmissionVersion)) return;
			Candidates.Add({Index, Pending.Priority, Pending.FirstQueuedSeconds, Pending.Sequence, PreparationCostMicroseconds, true});
		};
		// F0 项在大批 F1+ 事务中仍进入同一服务选择器，避免 32 项扫描窗口遗漏出生可游玩的 Fine。
		int32 F0Candidates = 0;
		for (int32 Index = 0; Index < Ready.Num() && F0Candidates < 32 && FPlatformTime::Seconds() < ScanDeadline; ++Index)
		{
			const auto& Pending = Ready[Index];
			if (Pending.Resources->Atoms.IsEmpty() ||
				Pending.Resources->Atoms[0]->Evidence.Owner.Kind != EVoxelFineOwnerKind::F0) continue;
			const int32 Before = Candidates.Num();
			AddCandidate(Index);
			F0Candidates += Candidates.Num() != Before;
		}
		for (int32 Scan = 0; Scan < FMath::Min(32, Ready.Num()) && FPlatformTime::Seconds() < ScanDeadline; ++Scan)
		{
			PreparationAdmissionCursor %= Ready.Num();
			const int32 Index = PreparationAdmissionCursor++;
			const auto& Pending = Ready[Index];
			if (!Pending.Resources->Atoms.IsEmpty() &&
				Pending.Resources->Atoms[0]->Evidence.Owner.Kind == EVoxelFineOwnerKind::F0) continue;
			AddCandidate(Index);
		}
		FVoxelTerrainServiceCharge Charge;
		const int32 Index = PreparationSelection.Select(Candidates, Capacity, FPlatformTime::Seconds(), Charge);
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
			check(ActiveBoundsCount < UE_ARRAY_COUNT(ActiveBounds));
			ActiveBounds[ActiveBoundsCount++] = Ready[Index].Preparation->SchedulingBounds;
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

bool FVoxelTerrainRendererState::BeginRootMerge(FReadyTransaction& InTransaction)
{
	check(InTransaction.Preparation && InTransaction.Preparation->bUploadComplete);
	if (InTransaction.bTerminalFailure) return false;
	if (InTransaction.Preparation->BaseSnapshot == LastSealedSnapshot) return true;
	if (RootMerge && RootMerge->Current != LastSealedSnapshot) ReleaseRootMerge();
	if (RootMerge) return RootMerge->Preparation == InTransaction.Preparation && RootMerge->bComplete;
	if (InTransaction.SuspendedMerge)
	{
		if (InTransaction.SuspendedMerge->Current == LastSealedSnapshot)
		{
			RootMerge = MoveTemp(InTransaction.SuspendedMerge);
			RootMerge->bSuspended.Store(false);
			return RootMerge->bComplete;
		}
		// 等待期间 Current 已推进，只撤销过期合并。已上传的局部 Prepared 仍可重用。
		const uint64 Token = InTransaction.SuspendedMerge->Consumer.RequestSerial;
		Scheduler.CancelMatching([this, Token](EVoxelTaskKind Kind, const FVoxelTaskStamp& Stamp)
		{
			return Kind == EVoxelTaskKind::BuildViewCoverage && Stamp.OwnerId == OwnerId &&
				Stamp.OwnerIncarnation == 1 && Stamp.WorldEpoch == Epoch && Stamp.Token == Token;
		});
		InTransaction.SuspendedMerge.Reset();
		InTransaction.MergeConsumer = {};
		InTransaction.bMergeCapacityBlocked = false;
	}
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
	if (!InputFloor.Add(InputLease, Resources.Get()) || !InputFloor.Add(InTransaction.Preparation->InputLease, Resources.Get()) ||
		!InputFloor.Add(InTransaction.Resources->CoarseCallbackLease, Resources.Get()))
	{ RejectAdmission(true); return false; }
	const auto InputAdmission = Resources->TryAcquire(Consumer, EVoxelTerrainResourcePool::Input,
		RenderRootAllocationBytes<FRootMerge>(), Lease, 0, InputFloor.Bytes, InputFloor.LeaseCount);
	if (InputAdmission != EVoxelTerrainResourceAdmission::Granted)
	{
		RejectAdmission(InputAdmission != EVoxelTerrainResourceAdmission::CapacityBlocked);
		return false;
	}
	const auto Work = CreateRenderRoot<FRootMerge>(MoveTemp(Lease), Resources, Consumer);
	Work->Begin(InTransaction.Preparation, LastSealedSnapshot);
	FVoxelTaskRequest Request;
	Request.Kind = EVoxelTaskKind::BuildViewCoverage;
	Request.Stamp.OwnerId = OwnerId;
	Request.Stamp.OwnerIncarnation = 1;
	Request.Stamp.WorldEpoch = Epoch;
	Request.Stamp.Token = Consumer.RequestSerial;
	Request.Stamp.Section = FIntVector(MIN_int32, 9, 0);
	Request.WorkClass = InTransaction.Priority.Band == EVoxelTerrainServiceBand::P0 ? EVoxelWorkClass::Critical : EVoxelWorkClass::Visible;
	Request.ServiceBand = InTransaction.Priority.Band;
	Request.SourceStableKey = InTransaction.Priority.SourceStableKey;
	Request.SourcePriority = InTransaction.Priority.SourcePriority;
	Request.DistanceScore = InTransaction.Priority.DistanceCells;
	Request.ForwardScore = InTransaction.Priority.ForwardScore;
	Request.FirstRunnableSeconds = InTransaction.FirstQueuedSeconds;
	Request.bResumable = Request.bPublicationContinuation = true;
	Request.InputBytes = Work->InputLease->Bytes();
	Request.GenerationConsumer = Work->Consumer;
	FRendererBindingInputFloor BindingInput;
	if (!BindingInput.Add(Work->InputLease, Resources.Get()) ||
		!BindingInput.Add(Work->Preparation->ControlInputLease, Resources.Get()) ||
		!BindingInput.Add(Work->Preparation->InputLease, Resources.Get()) ||
		!BindingInput.Add(Work->Preparation->PrimaryPathLease, Resources.Get()) ||
		!BindingInput.Add(Work->Preparation->Publication->Lease, Resources.Get()))
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
		if (!Pinned || !Pinned->bActive.Load() || Pinned->RootMerge != Work) return;
		if (Result.bSuccess && !Result.bCanceled && Result.Stamp.WorldEpoch == Pinned->Epoch &&
			Result.Stamp.OwnerId == Pinned->OwnerId && Result.Stamp.OwnerIncarnation == 1 &&
			Result.Stamp.Token == Work->Consumer.RequestSerial)
		{
			Work->bComplete = true;
			Work->Preparation->Publication->MergeReceipt = Result.WorkReceipt;
			return;
		}
		// 叶子冲突只重做该事务；其他准备及 Current 不受影响。
		const int32 Index = Pinned->Ready.IndexOfByPredicate([&Work](const FReadyTransaction& Pending) { return Pending.Preparation == Work->Preparation; });
		if (Index != INDEX_NONE)
		{
			if (Pinned->Scheduler.IsTerminal({Result.Kind, Result.Stamp}) ||
				Result.Error.StartsWith(TEXT("Oversized")) || Result.Error.StartsWith(TEXT("CapacityUnachievable")))
				Pinned->TerminateTransaction(Pinned->Ready[Index]);
			else Pinned->ReleasePreparation(Pinned->Ready[Index]);
		}
		Pinned->RootMerge.Reset();
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
	RootMerge = Work;
	return false;
}

FSnapshotPtr FVoxelTerrainRendererState::SealFrameSnapshot(const uint64 InFrameCounter, const double InDeadline)
{
	check(IsInGameThread());
	const double Deadline = FMath::Min(InDeadline, Module.GetTerrainFrameContext().Deadline());
	if (FPlatformTime::Seconds() >= Deadline) return LastSealedSnapshot;
	if (InFrameCounter == LastFrame || !PrepareMetadata() || Ready.IsEmpty() || PendingIssued.Num() == 512 ||
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
		TArray<FVoxelTerrainServiceCandidate, TInlineAllocator<32>> Candidates;
		const double ScanStarted = FPlatformTime::Seconds();
		const double ScanDeadline = ScanStarted + (Deadline - ScanStarted) * 0.5;
		const auto AddCandidate = [&](const int32 CandidateIndex)
		{
			auto& Candidate = Ready[CandidateIndex];
			if (Candidate.bMergeCapacityBlocked && RendererHasExternalRelease(Resources, Candidate.MergeConsumer, Candidate.MergeCapacityVersion))
				Candidate.bMergeCapacityBlocked = false;
			if (Candidate.bSealCapacityBlocked && RendererHasExternalRelease(Resources, Candidate.SealConsumer, Candidate.SealCapacityVersion))
				Candidate.bSealCapacityBlocked = false;
			const bool bRunnable = !Candidate.bTerminalFailure && Candidate.Preparation && Candidate.Preparation->bUploadComplete &&
				!Candidate.bMergeCapacityBlocked && !Candidate.bSealCapacityBlocked;
			if (bRunnable) Candidates.Add({CandidateIndex, Candidate.Priority, Candidate.FirstQueuedSeconds, Candidate.Sequence, SealCostMicroseconds, true});
		};
		int32 F0Candidates = 0;
		for (int32 CandidateIndex = 0; CandidateIndex < Ready.Num() && F0Candidates < 32 && FPlatformTime::Seconds() < ScanDeadline; ++CandidateIndex)
		{
			const auto& Candidate = Ready[CandidateIndex];
			if (Candidate.Resources->Atoms.IsEmpty() ||
				Candidate.Resources->Atoms[0]->Evidence.Owner.Kind != EVoxelFineOwnerKind::F0) continue;
			const int32 Before = Candidates.Num();
			AddCandidate(CandidateIndex);
			F0Candidates += Candidates.Num() != Before;
		}
		for (int32 Scan = 0; Scan < FMath::Min(32, Ready.Num()) && FPlatformTime::Seconds() < ScanDeadline; ++Scan)
		{
			SealScanCursor %= Ready.Num();
			const int32 CandidateIndex = SealScanCursor++;
			const auto& Candidate = Ready[CandidateIndex];
			if (!Candidate.Resources->Atoms.IsEmpty() &&
				Candidate.Resources->Atoms[0]->Evidence.Owner.Kind == EVoxelFineOwnerKind::F0) continue;
			AddCandidate(CandidateIndex);
		}
		FVoxelTerrainServiceCapacity Capacity;
		Capacity.bAllowOverdueOpportunity = Module.GetTerrainFrameContext().IsOverdueOpportunityAvailable();
		Capacity.MaximumWaitSeconds = Module.GetViewSettings().Fine.MaximumBackgroundWaitSeconds;
		Index = SealSelection.Select(Candidates, Capacity, FPlatformTime::Seconds(), Charge);
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
	FReadyTransaction& Pending = Ready[Index];
	if (!Pending.SealConsumer.IsValid())
		Pending.SealConsumer = FVoxelGenerationConsumerKey::Section(Epoch, OwnerId, 1, {}, ++NextMetadataSerial);
	if (Pending.bSealCapacityBlocked && !RendererHasExternalRelease(Resources, Pending.SealConsumer, Pending.SealCapacityVersion))
		return LastSealedSnapshot;
	Pending.bSealCapacityBlocked = false;
	const uint64 BeforeSeal = Resources->ReleaseVersionExcludingOwner(Pending.SealConsumer);
	const auto ObserveSealAdmission = [&](const EVoxelTerrainResourceAdmission Admission)
	{
		if (Admission == EVoxelTerrainResourceAdmission::Granted) return true;
		if (Admission == EVoxelTerrainResourceAdmission::CapacityBlocked)
		{
			Pending.bSealCapacityBlocked = true;
			Pending.SealCapacityVersion = BeforeSeal;
		}
		else TerminateTransaction(Pending);
		return false;
	};
	FString Error;
	const bool bRemoveCurrent = !Pending.Resources->CoarseChanges.ContainsByPredicate([this](const FVoxelTerrainCoarseMutation& Change)
	{
		return Change.bRemove && (!LastSealedSnapshot || !SameCoarseGeometry(FindCoarseItem(LastSealedSnapshot->CoarseRoot,
			Change.Item->Geometry->Stamp.GeometryIdentity.Key, Change.Item->SourceId), Change.Item));
	});
	const bool bCurrent = !Pending.Resources->CoarseChanges.IsEmpty() ? bRemoveCurrent && Pending.IsCoarseCurrent && Pending.IsCoarseCurrent() :
		Handoff.ValidateBeforeSeal(Pending.Id, Pending.IsCurrent, Pending.IsCollisionCurrent, Error);
	if (!bCurrent)
	{
		AdvanceCoarseAdmissionVersion();
		for (const auto& Change : Pending.Resources->CoarseChanges) PendingCoarseSources.Remove(Change.Item->SourceId);
		TOptional<FVoxelFineHandoffTransaction> Canceled;
		Handoff.RequestCancel(Pending.Id, Canceled);
		ReleasePreparation(Pending);
		AdvanceCoarseAdmissionVersion();
		Ready.RemoveAt(Index, 1, EAllowShrinking::No);
		if (RootMerge)
		{
			const uint64 Token = RootMerge->Consumer.RequestSerial;
			Scheduler.CancelMatching([this, Token](EVoxelTaskKind Kind, const FVoxelTaskStamp& Stamp)
			{
				return Kind == EVoxelTaskKind::BuildViewCoverage && Stamp.OwnerId == OwnerId &&
					Stamp.OwnerIncarnation == 1 && Stamp.WorldEpoch == Epoch && Stamp.Token == Token;
			});
			RootMerge.Reset();
		}
		return LastSealedSnapshot;
	}
	if (!ObserveSealAdmission(TryTransferPublicationLease(Resources.Get(), Pending.Resources->Lease,
		Pending.Preparation->bAllowResidentOverlap))) return LastSealedSnapshot;
	for (const auto& Atom : Pending.Resources->Atoms)
		if (!ObserveSealAdmission(TryTransferPublicationLease(Resources.Get(), Atom->MetadataLease,
			Pending.Preparation->bAllowResidentOverlap))) return LastSealedSnapshot;
	if (!BeginRootMerge(Pending)) return LastSealedSnapshot;
	int64 RetiredAtomCount = Pending.Resources->Atoms.Num();
	int32 ReceiptAtomCount = Pending.Resources->Atoms.Num();
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
	// 帧携带固定回执槽、退休头和全部潜在旧 Atom/事务/临时回执数组，先授予再分配。
	const uint64 Bytes = VoxelBoundedAllocation::WorkerSharedBytes<FRenderSnapshot>() +
		VoxelBoundedAllocation::ArrayBytes<FFrozenPublication>(PendingIssued.Num() + 1) +
		VoxelBoundedAllocation::ArrayBytes<FVoxelFinePresentedAtom>(int32(RetiredAtomCount)) +
		VoxelBoundedAllocation::ArrayBytes<FVoxelFineRenderReceiptAtom>(ReceiptAtomCount) +
		VoxelBoundedAllocation::ArrayBytes<FGuid>(PendingIssued.Num() + 1);
	const auto Consumer = Pending.SealConsumer;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	FRendererBindingInputFloor Floor;
	if (!Floor.Add(Pending.Preparation->InputLease, Resources.Get()) ||
		!Floor.Add(InputLease, Resources.Get()) ||
		!Floor.Add(Pending.Resources->CoarseCallbackLease, Resources.Get()) ||
		!Floor.Add(Pending.Preparation->PrimaryPathLease, Resources.Get()) || !Floor.Add(Pending.Resources->Lease, Resources.Get()) ||
		!Floor.Add(RootMerge ? RootMerge->InputLease : nullptr, Resources.Get()))
	{
		ObserveSealAdmission(EVoxelTerrainResourceAdmission::InvalidRequest);
		return LastSealedSnapshot;
	}
	if (!ObserveSealAdmission(TryAcquirePublicationResident(Resources, Consumer, Bytes, Lease, 0,
		Pending.Preparation->bAllowResidentOverlap, Floor.Bytes, Floor.LeaseCount))) return LastSealedSnapshot;
	auto Snapshot = VoxelBoundedAllocation::MakeWorkerShared<FRenderSnapshot>(Lease);
	Snapshot->Lease = MoveTemp(Lease);
	Snapshot->Retirement.PreviousAtoms.Reserve(int32(RetiredAtomCount));
	Snapshot->Retirement.ReceiptScratch.Reserve(ReceiptAtomCount);
	Snapshot->Retirement.SettledTransactions.Reserve(PendingIssued.Num() + 1);
	Snapshot->WorldEpoch = Epoch;
	Snapshot->Serial = ++NextPresentation;
	Snapshot->FrameCounter = InFrameCounter;
	Snapshot->Root = RootMerge ? RootMerge->Fine.GetResult() : Pending.Preparation->Root;
	Snapshot->CoarseRoot = RootMerge ? RootMerge->Coarse.GetResult() : Pending.Preparation->CoarseRoot;
	Snapshot->Issued.Reserve(PendingIssued.Num() + 1);
	check(VoxelBoundedAllocation::WorkerSharedBytes<FRenderSnapshot>() + Snapshot->Issued.GetAllocatedSize() +
		Snapshot->Retirement.PreviousAtoms.GetAllocatedSize() + Snapshot->Retirement.ReceiptScratch.GetAllocatedSize() +
		Snapshot->Retirement.SettledTransactions.GetAllocatedSize() <= Bytes);
	Snapshot->Issued.Append(PendingIssued);
	const FVoxelFinePublishStamp Stamp{Epoch, Snapshot->Serial, InFrameCounter};
	TSharedPtr<const FVoxelFineFrozenHandoff, ESPMode::ThreadSafe> Frozen;
	if (Pending.Resources->CoarseChanges.IsEmpty())
	{
		const auto InterfacesCurrent = [&Pending](const FVoxelFineHandoffTransaction& Transaction, const FVoxelTaskCustomPayload& Publication)
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
		if (!Handoff.SealTransaction(Pending.Id, Stamp, Pending.Resources, Pending.IsCurrent, Pending.IsCollisionCurrent,
			InterfacesCurrent, Frozen, Error))
		{
			TOptional<FVoxelFineHandoffTransaction> Canceled;
			Handoff.RequestCancel(Pending.Id, Canceled);
			ReleasePreparation(Pending);
			AdvanceCoarseAdmissionVersion();
			Ready.RemoveAt(Index, 1, EAllowShrinking::No);
			RootMerge.Reset();
			return LastSealedSnapshot;
		}
	}
	Pending.Resources->Stamp = Stamp;
	Snapshot->Issued.Add({Frozen, Pending.Resources});
	Pending.Resources->PendingIssuedIndex = PendingIssued.Add({Frozen, Pending.Resources});
	SubmittedCallbacks.Add(Pending.Id, MoveTemp(Pending.OnSubmitted));
	ReleasePreparation(Pending);
	AdvanceCoarseAdmissionVersion();
	Ready.RemoveAt(Index, 1, EAllowShrinking::No);
	LastFrame = InFrameCounter;
	LastSealedSnapshot = Snapshot;
	Snapshot->Lease->MarkPublished();
	const auto& Published = Snapshot->Issued.Last().Resources;
	Published->Lease->MarkPublished();
	if (Published->CoarseCallbackLease) Published->CoarseCallbackLease->MarkPublished();
	for (const auto& Atom : Published->Atoms) Atom->MetadataLease->MarkPublished();
	RootMerge.Reset();
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
	const uint64 Bytes = VoxelBoundedAllocation::ArrayBytes<FReadyTransaction>(512) +
		VoxelBoundedAllocation::MapBytes<FGuid, TFunction<void()>>(512) +
		VoxelBoundedAllocation::SetBytes<FGuid>(32768) +
		VoxelBoundedAllocation::ArrayBytes<FFrozenPublication>(512) + VoxelBoundedAllocation::ArrayBytes<FSnapshotPtr>(8);
	MetadataCapacityVersion = Resources->ReleaseVersionExcludingOwner(MetadataConsumer);
	MetadataAdmission = Resources->TryAcquire(MetadataConsumer, EVoxelTerrainResourcePool::Resident, Bytes, MetadataLease);
	if (MetadataAdmission != EVoxelTerrainResourceAdmission::Granted) return false;
	Ready.Reserve(512);
	SubmittedCallbacks.Reserve(512);
	PendingCoarseSources.Reserve(32768);
	PendingIssued.Reserve(512);
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
					State->Ready[Index].SuspendedMerge = MoveTemp(State->RootMerge);
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
		if (!State->Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Staged,
			VoxelBoundedAllocation::WorkerSharedBytes<FVoxelTerrainRenderAtom>(), AtomLease) ||
			!State->Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Staged,
				VoxelBoundedAllocation::WorkerSharedBytes<FRetirementProof>(), ProofLease)) return;
		if (!ProofLease->Transfer(EVoxelTerrainResourcePool::Resident)) return;
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
	check(IsInGameThread() && InSnapshot && InSnapshot->WorldEpoch == Epoch);
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
	check(IsInGameThread() && Retiring.bHasReceipts && Retiring.Snapshot->WorldEpoch == Epoch);
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
		check(PendingIssued.IsValidIndex(PendingIndex) && PendingIssued[PendingIndex].Resources == Frozen.Resources);
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

void FVoxelTerrainRenderer::PollRenderReceipts()
{
	if (!State || !State->bActive.Load()) return;
	const double Deadline = State->Module.GetTerrainFrameContext().Deadline();
	auto& Budget = State->Module.GetTerrainFrameContext().GetPublishBudget();
	const auto Run = [this, &Budget](const EVoxelPublishPhase InPhase, double& InOutEstimate, auto&& InOperation)
	{
		const auto& Policy = State->Module.GetViewSettings().Budget;
		const double Floor = Policy.PublishOperationEstimateFloorMilliseconds;
		const double Ceiling = FMath::Min(double(Policy.PublishOperationEstimateCeilingMilliseconds), double(Policy.TerrainPublishMilliseconds) * 0.5);
		InOutEstimate = FMath::Clamp(InOutEstimate, Floor, Ceiling);
		const double Started = FPlatformTime::Seconds();
		if (!Budget.TryStart(InPhase, Started, InOutEstimate)) return false;
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
	}
	// 关闭之后继续由独立退休服务消费已发行帧；不依赖已销毁的 Module。
	FTSTicker::GetCoreTicker().AddTicker(TEXT("VoxelTerrainRetirement"), 0.0f,
		TUniqueFunction<bool(float)>(FVoxelRendererRetirement{State}));
	State.Reset();
}
