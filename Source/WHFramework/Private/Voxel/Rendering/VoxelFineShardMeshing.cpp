#include "Voxel/Rendering/VoxelFineShardBuild.h"

#include "Voxel/Chunks/VoxelSectionKey.h"
#include "Voxel/Geometry/DWVoxelFaceVisibility.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"

namespace
{
	struct FFineFaceKey
	{
		bool bVisible = false;
		uint32 Packed = 0;
		FVoxelRuntimeFaceRef Texture;
		EVoxelRenderGroup Group = EVoxelRenderGroup::Opaque;
		bool operator==(const FFineFaceKey& InOther) const = default;
	};

	struct FBatchPlan
	{
		uint32 Key = 0;
		int32 Vertices = 0;
		int32 Indices = 0;
	};

	struct FPrimitiveWorkspace
	{
		FVector Vertices[4];
		FVector2D UV[4];
	};

	struct FBuildPlan
	{
		TArray<FBatchPlan> Batches;
		FVoxelFineMeshEstimate Estimate;
	};

	uint32 BatchKey(const EVoxelRenderGroup InGroup, const uint16 InBank)
	{
		return (uint32(InGroup) << 16) | InBank;
	}

	int32 LowerBound(const TArray<FBatchPlan>& InPlans, const uint32 InKey)
	{
		int32 First = 0;
		int32 Last = InPlans.Num();
		while (First < Last)
		{
			const int32 Middle = First + (Last - First) / 2;
			if (InPlans[Middle].Key < InKey)
			{
				First = Middle + 1;
			}
			else
			{
				Last = Middle;
			}
		}
		return First;
	}

	uint64 AttributeBytes(const int32 InVertices, const int32 InIndices)
	{
		using namespace VoxelBoundedAllocation;
		return 2 * ArrayBytes<FVector>(InVertices) + 3 * ArrayBytes<FVector2D>(InVertices) +
			ArrayBytes<FLinearColor>(InVertices) + ArrayBytes<FProcMeshTangent>(InVertices) +
			ArrayBytes<int32>(InIndices);
	}

	bool ReserveMesh(FVoxelMeshBuffers& InMesh, const FBatchPlan& InPlan)
	{
		using namespace VoxelBoundedAllocation;
		return Reserve(InMesh.Vertices, InPlan.Vertices, ArrayBytes<FVector>(InPlan.Vertices)) &&
			Reserve(InMesh.Normals, InPlan.Vertices, ArrayBytes<FVector>(InPlan.Vertices)) &&
			Reserve(InMesh.UV0, InPlan.Vertices, ArrayBytes<FVector2D>(InPlan.Vertices)) &&
			Reserve(InMesh.UV1, InPlan.Vertices, ArrayBytes<FVector2D>(InPlan.Vertices)) &&
			Reserve(InMesh.UV2, InPlan.Vertices, ArrayBytes<FVector2D>(InPlan.Vertices)) &&
			Reserve(InMesh.Colors, InPlan.Vertices, ArrayBytes<FLinearColor>(InPlan.Vertices)) &&
			Reserve(InMesh.Tangents, InPlan.Vertices, ArrayBytes<FProcMeshTangent>(InPlan.Vertices)) &&
			Reserve(InMesh.Triangles, InPlan.Indices, ArrayBytes<int32>(InPlan.Indices));
	}

	FVector2D FaceUV(const uint8 InFace, const FVector& InPosition, const int32 InSide)
	{
		if (InFace < 2)
		{
			return {InPosition.Y, InSide - InPosition.Z};
		}
		if (InFace < 4)
		{
			return {InPosition.X, InSide - InPosition.Z};
		}
		return {InPosition.X, InPosition.Y};
	}

	bool PrimitiveBound(const FVoxelSectionSnapshot& InSource, const FIntVector& InOffset, const int32 InSide,
		const FVoxelRegistrySnapshot& InRegistry, const FVoxelShapeRegistry& InShapes, int32& OutBound)
	{
		uint64 Count = 0;
		for (int32 Z = 0; Z < InSide; ++Z)
		{
			for (int32 Y = 0; Y < InSide; ++Y)
			{
				for (int32 X = 0; X < InSide; ++X)
				{
					FVoxelBlockState Block;
					if (!InSource.TrySample(InOffset + FIntVector(X, Y, Z), Block))
					{
						return false;
					}
					if (Block.IsAir())
					{
						continue;
					}
					const auto* Definition = InRegistry.Find(Block.TypeId);
					const auto* Shape = Definition ? Definition->FindShape(InShapes, Block.State) : nullptr;
					if (!Shape)
					{
						return false;
					}
					Count += Definition->Shape == EVoxelShapeKind::FullCube ? 6 : Shape->Quads.Num();
					if (Count > MAX_int32)
					{
						return false;
					}
				}
			}
		}
		OutBound = int32(Count);
		return true;
	}

	// Count and Build share this traversal. The immutable shape registry is validated
	// for every state/rotation before admission, so all primitives stay in their cell.
	// Every Piece is cell aligned and belongs to one 16^3 Atom; no quad crosses its owner.
	template<typename EmitterType>
	bool VisitFinePrimitives(const FVoxelSectionSnapshot& InSource, const FIntVector& InSampleOffset,
		const FIntVector& InTextureOffset, const int32 InSide, const int32 InTextureSide,
		const double InRepeats, const FVoxelRegistrySnapshot& InRegistry,
		const FVoxelShapeRegistry& InShapes, TArray<FFineFaceKey>& InMask,
		const TAtomic<bool>& InCancel, EmitterType&& InEmit)
	{
		const auto Visible = [&](const FIntVector& Cell, const FVoxelBlockState Block,
			const uint8 Face, const FVoxelShapeQuad* Quad)
		{
			FVoxelBlockState Neighbor;
			return InSource.TrySample(InSampleOffset + Cell + VoxelCoord::Direction(Face), Neighbor) &&
				VoxelFaceVisibility::ShouldRender(InRegistry, InShapes, Block, Neighbor, Face, Quad);
		};
		FPrimitiveWorkspace Workspace;
		for (uint8 Face = 0; Face < 6; ++Face)
		{
			const int32 Axis = Face / 2;
			const int32 U = (Axis + 1) % 3;
			const int32 V = (Axis + 2) % 3;
			for (int32 Slice = 0; Slice < InSide; ++Slice)
			{
				if (InCancel.Load())
				{
					return false;
				}
				for (FFineFaceKey& Entry : InMask)
				{
					Entry.bVisible = false;
				}
				for (int32 Y = 0; Y < InSide; ++Y)
				{
					for (int32 X = 0; X < InSide; ++X)
					{
						FIntVector Cell(0);
						Cell[Axis] = Slice;
						Cell[U] = X;
						Cell[V] = Y;
						FVoxelBlockState Block;
						if (!InSource.TrySample(InSampleOffset + Cell, Block))
						{
							return false;
						}
						const auto* Definition = InRegistry.Find(Block.TypeId);
						if (Block.IsAir() || !Definition || Definition->Shape != EVoxelShapeKind::FullCube ||
							!Visible(Cell, Block, Face, nullptr))
						{
							continue;
						}
						FFineFaceKey& Entry = InMask[X + InSide * Y];
						Entry.bVisible = true;
						Entry.Packed = Block.Pack();
						Entry.Group = Definition->RenderGroup;
						Entry.Texture = Definition->Face(Block.State,
							FVoxelShapeRegistry::RotateFace(Face, uint8((4 - (Block.State & 3)) & 3)));
					}
				}
				for (int32 Y = 0; Y < InSide; ++Y)
				{
					for (int32 X = 0; X < InSide;)
					{
						const FFineFaceKey Key = InMask[X + InSide * Y];
						if (!Key.bVisible)
						{
							++X;
							continue;
						}
						int32 Width = 1;
						int32 Height = 1;
						while (X + Width < InSide && InMask[X + Width + InSide * Y] == Key)
						{
							++Width;
						}
						bool bStop = false;
						while (Y + Height < InSide && !bStop)
						{
							for (int32 DX = 0; DX < Width; ++DX)
							{
								if (InMask[X + DX + InSide * (Y + Height)] != Key)
								{
									bStop = true;
									break;
								}
							}
							if (!bStop)
							{
								++Height;
							}
						}
						const int32 PX[] = {X, X + Width, X + Width, X};
						const int32 PY[] = {Y, Y, Y + Height, Y + Height};
						for (int32 Index = 0; Index < 4; ++Index)
						{
							FVector& Vertex = Workspace.Vertices[Index];
							Vertex = FVector::ZeroVector;
							Vertex[Axis] = Slice + ((Face & 1) ? 0 : 1);
							Vertex[U] = PX[Index];
							Vertex[V] = PY[Index];
							Workspace.UV[Index] = FaceUV(Face, Vertex + FVector(InTextureOffset), InTextureSide) * InRepeats;
						}
						if (Face & 1)
						{
							Swap(Workspace.Vertices[1], Workspace.Vertices[3]);
							Swap(Workspace.UV[1], Workspace.UV[3]);
						}
						if (!InEmit(Key.Group, Key.Texture, Workspace.Vertices, Workspace.UV, FVector::ZeroVector, false))
						{
							return false;
						}
						for (int32 DY = 0; DY < Height; ++DY)
						{
							for (int32 DX = 0; DX < Width; ++DX)
							{
								InMask[X + DX + InSide * (Y + DY)].bVisible = false;
							}
						}
						X += Width;
					}
				}
			}
		}
		for (int32 Z = 0; Z < InSide; ++Z)
		{
			if (InCancel.Load())
			{
				return false;
			}
			for (int32 Y = 0; Y < InSide; ++Y)
			{
				for (int32 X = 0; X < InSide; ++X)
				{
					const FIntVector Cell(X, Y, Z);
					FVoxelBlockState Block;
					if (!InSource.TrySample(InSampleOffset + Cell, Block))
					{
						return false;
					}
					const auto* Definition = InRegistry.Find(Block.TypeId);
					if (Block.IsAir() || !Definition || Definition->Shape == EVoxelShapeKind::FullCube)
					{
						continue;
					}
					const auto* Shape = Definition->FindShape(InShapes, Block.State);
					if (!Shape)
					{
						return false;
					}
					for (const FVoxelShapeQuad& Quad : Shape->Quads)
					{
						if (Quad.bBoundary && !Visible(Cell, Block, Quad.Face, &Quad))
						{
							continue;
						}
						if (!InEmit(Definition->RenderGroup, Definition->Face(Block.State, Quad.MaterialFace),
							Quad.Vertices, Quad.UV, FVector(Cell), Definition->Shape == EVoxelShapeKind::CrossPlant))
						{
							return false;
						}
					}
				}
			}
		}
		return true;
	}
}

FVoxelFineShardMeshing::FVoxelFineShardMeshing(
	TSharedRef<const FVoxelFineShardSamples, ESPMode::ThreadSafe> InSamples,
	TSharedRef<const FVoxelRegistrySnapshot, ESPMode::ThreadSafe> InRegistry,
	TSharedRef<const FVoxelShapeRegistry, ESPMode::ThreadSafe> InShapes,
	TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
	const FVoxelGenerationConsumerKey& InConsumer, const double InTextureRepeatsPerCell)
	: Samples(InSamples), Registry(InRegistry), Shapes(InShapes), Resources(InResources),
	Consumer(InConsumer), TextureRepeatsPerCell(InTextureRepeatsPerCell)
{
}

FVoxelFineMeshEstimate FVoxelFineShardMeshing::MinimumPieceMemory(const int32 InSideCells, const int32 InPrimitiveBound)
{
	check(InSideCells > 0 && InSideCells <= 64 && InPrimitiveBound >= 0);
	using namespace VoxelBoundedAllocation;
	FVoxelFineMeshEstimate Estimate;
	Estimate.MaskBytes = ArrayBytes<FFineFaceKey>(InSideCells * InSideCells);
	Estimate.MetadataBytes = sizeof(FBuildPlan) + sizeof(FPrimitiveWorkspace) + ArrayBytes<FBatchPlan>(InPrimitiveBound);
	Estimate.SliceBytes = sizeof(FVoxelSectionSnapshot) + ArrayBytes<uint32>(InSideCells * InSideCells * InSideCells) +
		6 * ArrayBytes<uint32>(InSideCells * InSideCells);
	Estimate.WorkingBytes = Estimate.SliceBytes + Estimate.MaskBytes;
	Estimate.AdditionalPeakBytes = Estimate.MetadataBytes + Estimate.WorkingBytes;
	return Estimate;
}

void FVoxelFineShardMeshing::VisitResourceSources(FVoxelGenerationRequiredSourceScope::FVisit InVisit) const
{
	const auto Visit = [&](const FVoxelTerrainResourceLease* InLease)
	{
		if (InLease && InLease->IsOwnedBy(*Resources))
		{
			FVoxelGenerationRequiredSourceScope::VisitLease(InLease, InVisit);
		}
	};
	Visit(Samples ? Samples->ResourceLease.Get() : nullptr);
	Visit(Samples && Samples->OverlayVersion ? Samples->OverlayVersion->ResourceLease.Get() : nullptr);
	Visit(PendingLease.Get());
	Visit(CapacityWorkingLease.Get());
	Visit(CapacityResultLease.Get());
	if (Geometry)
	{
		Visit(Geometry->MetadataLease.Get());
		Visit(Geometry->OverlayVersion ? Geometry->OverlayVersion->ResourceLease.Get() : nullptr);
		Visit(Geometry->Boundary ? Geometry->Boundary->ResourceLease.Get() : nullptr);
		for (const FVoxelFineMeshPiece& Piece : Geometry->Pieces)
		{
			Visit(Piece.Mesh ? Piece.Mesh->ResourceLease.Get() : nullptr);
		}
	}
}

bool FVoxelFineShardMeshing::Initialize(const uint64 InMaximumPieceBytes,
	const uint64 InMaximumTotalGeometryBytes, FString& OutError)
{
	if (State != EVoxelFineBuildProgress::None || !Samples || InMaximumPieceBytes == 0 ||
		InMaximumTotalGeometryBytes == 0 || !FMath::IsFinite(TextureRepeatsPerCell) || TextureRepeatsPerCell <= 0.0 ||
		!Consumer.IsValid() || (Consumer.Kind != EVoxelGenerationConsumerKind::FineShard &&
			Consumer.Kind != EVoxelGenerationConsumerKind::F0Section) ||
		Consumer.WorldEpoch != Samples->Attempt.Input.WorldEpoch || Consumer.OwnerIncarnation != Samples->Attempt.Input.Incarnation ||
		Consumer.RequestSerial != Samples->Attempt.Serial ||
		(Consumer.Kind == EVoxelGenerationConsumerKind::FineShard &&
			(Consumer.Min != Samples->Attempt.Input.Shard.Min || Consumer.Max != Samples->Attempt.Input.Shard.Bounds().Max)) ||
		(Consumer.Kind == EVoxelGenerationConsumerKind::F0Section &&
			(Consumer.SpatialKind != EVoxelGenerationSpatialKind::Section || Samples->Attempt.Input.Shard.SideCells != 16 ||
				Consumer.Min * 16 != Samples->Attempt.Input.Shard.Min)) || !Samples->ResourceLease ||
		!Samples->ResourceLease->IsOwnedBy(*Resources) || Samples->ResourceLease->Bytes() < Samples->AllocatedBytes() || !Samples->Attempt.Input.Shard.Validate(OutError) ||
		!Shapes->ValidateCellBounds(OutError))
	{
		if (OutError.IsEmpty())
		{
			OutError = TEXT("Fine mesher requires a complete immutable input and its reservation");
		}
		return false;
	}
	FVoxelGenerationRequiredSourceScope SourceScope(&Resources.Get(), this,
		[](const void* InContext, FVoxelGenerationRequiredSourceScope::FVisit InVisit)
		{
			static_cast<const FVoxelFineShardMeshing*>(InContext)->VisitResourceSources(InVisit);
		});
	const int32 Side = Samples->Attempt.Input.Shard.SideCells;
	if (Samples->Snapshot.GridSide != Side || Samples->Snapshot.Blocks.Num() != Side * Side * Side)
	{
		OutError = TEXT("Fine samples have an invalid grid");
		return false;
	}
	for (const uint32 Packed : Samples->Snapshot.Blocks)
	{
		if (!Registry->IsValid(FVoxelBlockState::Unpack(Packed)))
		{
			OutError = TEXT("Fine samples contain an invalid block");
			return false;
		}
	}
	for (int32 Face = 0; Face < 6; ++Face)
	{
		if (!Samples->Snapshot.Known[Face] || Samples->Snapshot.Halo[Face].Num() != Side * Side)
		{
			OutError = TEXT("DependencyNotReady: Fine halo is incomplete");
			return false;
		}
		for (const uint32 Packed : Samples->Snapshot.Halo[Face])
		{
			if (!Registry->IsValid(FVoxelBlockState::Unpack(Packed)))
			{
				OutError = TEXT("Fine halo contains an invalid block");
				return false;
			}
		}
	}
	MaximumPieceBytes = InMaximumPieceBytes;
	MaximumTotalGeometryBytes = InMaximumTotalGeometryBytes;
	const int32 PieceSide = FMath::Min(Side, 16);
	const int32 Count = (Side / PieceSide) * (Side / PieceSide) * (Side / PieceSide);
	const uint64 MetadataBytes = VoxelBoundedAllocation::WorkerSharedBytes<FVoxelFineShardGeometry>() +
		VoxelBoundedAllocation::ArrayBytes<FVoxelFineMeshPiece>(Count);
	const uint64 BoundaryBytes = VoxelBoundedAllocation::WorkerSharedBytes<FVoxelFineBoundarySnapshot>() +
		12 * VoxelBoundedAllocation::ArrayBytes<uint32>(Side * Side) +
		VoxelBoundedAllocation::ArrayBytes<FVoxelFineAtomBoundary>(Count) +
		12ull * Count * VoxelBoundedAllocation::ArrayBytes<uint32>(256);
	if (MetadataBytes + BoundaryBytes > MaximumTotalGeometryBytes)
	{
		CapacityReason = EVoxelFineCapacityReason::CapacityUnachievable;
		OutError = TEXT("CapacityUnachievable: Fine 边界与元数据超过单 Shard 上限");
		return false;
	}
	if (VoxelFineShardCapacity::TryReserve(Resources, Consumer, EVoxelTerrainResourcePool::Working,
		VoxelBoundedAllocation::ArrayBytes<FVoxelFineShardKey>(Count), PendingLease,
		OutError, CapacityVersion, 0, MetadataBytes + BoundaryBytes, 2) != EVoxelTerrainResourceAdmission::Granted)
	{
		return false;
	}
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> MetadataLease;
	if (VoxelFineShardCapacity::TryReserve(Resources, Consumer, EVoxelTerrainResourcePool::Staged,
		MetadataBytes, MetadataLease, OutError, CapacityVersion, 0, BoundaryBytes, 1) != EVoxelTerrainResourceAdmission::Granted)
	{
		PendingLease.Reset();
		return false;
	}
	Geometry = VoxelBoundedAllocation::MakeWorkerShared<FVoxelFineShardGeometry>(MetadataLease);
	Geometry->MetadataLease = MoveTemp(MetadataLease);
	Geometry->Attempt = Samples->Attempt;
	Geometry->GeometryIdentity = FGuid::NewGuid();
	Geometry->RegistrationLease = Samples->RegistrationLease;
	Geometry->OverlayVersion = Samples->OverlayVersion;
	Geometry->bKnownEmpty = true;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> BoundaryLease;
	if (VoxelFineShardCapacity::TryReserve(Resources, Consumer, EVoxelTerrainResourcePool::Staged,
		BoundaryBytes, BoundaryLease, OutError, CapacityVersion) != EVoxelTerrainResourceAdmission::Granted)
	{
		Abort();
		return false;
	}
	const auto Boundary = VoxelBoundedAllocation::MakeWorkerShared<FVoxelFineBoundarySnapshot>(BoundaryLease);
	Boundary->Input = Samples->Attempt.Input;
	Boundary->ResourceLease = MoveTemp(BoundaryLease);
	const auto CaptureFace = [&](const FIntVector& Offset, const int32 FaceSide, const int32 Face,
		const bool bHalo, TArray<uint32>& OutFace)
	{
		const int32 Axis = Face / 2;
		const int32 U = (Axis + 1) % 3;
		const int32 V = (Axis + 2) % 3;
		const auto Read = [&](const int32 X, const int32 Y, uint32& OutPacked)
		{
			FIntVector Cell = Offset;
			Cell[Axis] += bHalo ? ((Face & 1) ? -1 : FaceSide) : ((Face & 1) ? 0 : FaceSide - 1);
			Cell[U] += X;
			Cell[V] += Y;
			FVoxelBlockState State;
			if (!Samples->Snapshot.TrySample(Cell, State)) return false;
			OutPacked = State.Pack();
			return true;
		};
		uint32 First = 0;
		if (!Read(0, 0, First)) return false;
		bool bUniform = true;
		for (int32 Y = 0; Y < FaceSide && bUniform; ++Y)
		{
			for (int32 X = 0; X < FaceSide; ++X)
			{
				uint32 Packed;
				if (!Read(X, Y, Packed)) return false;
				if (Packed != First) { bUniform = false; break; }
			}
		}
		// 单值面是完整采样后的无损编码；长度一不表示未知或缺失。
		const int32 Values = bUniform ? 1 : FaceSide * FaceSide;
		if (!VoxelBoundedAllocation::Reserve(OutFace, Values, VoxelBoundedAllocation::ArrayBytes<uint32>(Values))) return false;
		if (bUniform) OutFace.Add(First);
		else for (int32 Y = 0; Y < FaceSide; ++Y)
		{
			for (int32 X = 0; X < FaceSide; ++X)
			{
				uint32 Packed;
				if (!Read(X, Y, Packed)) return false;
				OutFace.Add(Packed);
			}
		}
		return true;
	};
	for (int32 Face = 0; Face < 6; ++Face)
	{
		if (!CaptureFace(FIntVector::ZeroValue, Side, Face, false, Boundary->Owned[Face]) ||
			!CaptureFace(FIntVector::ZeroValue, Side, Face, true, Boundary->Halo[Face]))
		{
			Abort();
			OutError = TEXT("Fine Shard 外侧边界采样不完整");
			return false;
		}
	}
	if (!VoxelBoundedAllocation::Reserve(Boundary->Atoms, Count, VoxelBoundedAllocation::ArrayBytes<FVoxelFineAtomBoundary>(Count)))
	{
		Abort();
		OutError = TEXT("Fine Atom 边界元数据分配超过资源预留");
		return false;
	}
	const int32 AtomSide = FMath::Min(16, Side);
	const int32 AtomsPerAxis = Side / AtomSide;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const FIntVector Offset = FIntVector(Index % AtomsPerAxis, (Index / AtomsPerAxis) % AtomsPerAxis,
			Index / (AtomsPerAxis * AtomsPerAxis)) * AtomSide;
		auto& Atom = Boundary->Atoms.AddDefaulted_GetRef();
		Atom.Section = (Boundary->Input.Shard.Min + Offset) / 16;
		for (int32 Face = 0; Face < 6; ++Face)
		{
			if (!CaptureFace(Offset, AtomSide, Face, false, Atom.Owned[Face]) ||
				!CaptureFace(Offset, AtomSide, Face, true, Atom.Halo[Face]))
			{
				Abort();
				OutError = TEXT("Fine 内部 Atom 边界采样不完整");
				return false;
			}
		}
	}
	verify(Boundary->ResourceLease->TryResize(Boundary->GetAllocatedBytes()) == EVoxelTerrainResourceAdmission::Granted);
	Geometry->Boundary = Boundary;
	if (!ReserveMetadata(Count, Count, OutError))
	{
		Abort();
		return false;
	}
	for (int32 Z = 0; Z < Side; Z += PieceSide)
	{
		for (int32 Y = 0; Y < Side; Y += PieceSide)
		{
			for (int32 X = 0; X < Side; X += PieceSide)
			{
				Pending.Add({Samples->Attempt.Input.Shard.Min + FIntVector(X, Y, Z), PieceSide});
			}
		}
	}
	GeometryBytes = Geometry->GetAllocatedBytes();
	CapacityVersion = Resources->ReleaseVersionExcludingOwner(Consumer);
	State = EVoxelFineBuildProgress::Working;
	OutError.Reset();
	return true;
}

bool FVoxelFineShardMeshing::ReserveMetadata(const int32 InPendingCount, const int32 InPieceCount, FString& OutError)
{
	using namespace VoxelBoundedAllocation;
	const uint64 PendingBytes = FMath::Max<uint64>(Pending.GetAllocatedSize(), ArrayBytes<FVoxelFineShardKey>(InPendingCount));
	const uint64 PiecesBytes = FMath::Max<uint64>(Geometry->Pieces.GetAllocatedSize(), ArrayBytes<FVoxelFineMeshPiece>(InPieceCount));
	const uint64 GeometryBase = WorkerSharedBytes<FVoxelFineShardGeometry>();
	const uint64 ExistingResults = Geometry->GetAllocatedBytes() - GeometryBase - Geometry->Pieces.GetAllocatedSize();
	if (ExistingResults + GeometryBase + PiecesBytes > MaximumTotalGeometryBytes)
	{
		CapacityReason = EVoxelFineCapacityReason::CapacityUnachievable;
		OutError = TEXT("CapacityUnachievable: Fine subdivision metadata exceeds Shard limit");
		return false;
	}
	if (InPendingCount > Pending.Max())
	{
		const uint64 Peak = Pending.GetAllocatedSize() + PendingBytes;
		const uint64 ResultPeak = GeometryBase + Geometry->Pieces.GetAllocatedSize() + PiecesBytes;
		const uint64 AdditionalResult = InPieceCount > Geometry->Pieces.Max()
			? ResultPeak - Geometry->MetadataLease->Bytes() : 0;
		const auto Admission = VoxelFineShardCapacity::TryReserve(Resources, Consumer, EVoxelTerrainResourcePool::Working,
			Peak, PendingLease, OutError, CapacityVersion, 0, AdditionalResult);
		if (Admission != EVoxelTerrainResourceAdmission::Granted)
		{
			CapacityReason = Admission == EVoxelTerrainResourceAdmission::CapacityBlocked
				? EVoxelFineCapacityReason::WorkingTokenUnavailable : EVoxelFineCapacityReason::CapacityUnachievable;
			RequiredWorkingBytes = Peak - PendingLease->Bytes();
			return false;
		}
		if (!Reserve(Pending, InPendingCount, PendingBytes))
		{
			CapacityReason = EVoxelFineCapacityReason::CapacityUnachievable;
			OutError = TEXT("EstimateMismatch: Fine pending allocator capacity");
			return false;
		}
		verify(PendingLease->TryResize(Pending.GetAllocatedSize()) == EVoxelTerrainResourceAdmission::Granted);
	}
	if (InPieceCount > Geometry->Pieces.Max())
	{
		const uint64 Peak = GeometryBase + Geometry->Pieces.GetAllocatedSize() + PiecesBytes;
		const auto Admission = VoxelFineShardCapacity::TryReserve(Resources, Consumer, EVoxelTerrainResourcePool::Staged,
			Peak, Geometry->MetadataLease, OutError, CapacityVersion);
		if (Admission != EVoxelTerrainResourceAdmission::Granted)
		{
			CapacityReason = Admission == EVoxelTerrainResourceAdmission::CapacityBlocked
				? EVoxelFineCapacityReason::ResultTokenUnavailable : EVoxelFineCapacityReason::CapacityUnachievable;
			RequiredResultBytes = Peak - Geometry->MetadataLease->Bytes();
			return false;
		}
		if (!Reserve(Geometry->Pieces, InPieceCount, PiecesBytes))
		{
			CapacityReason = EVoxelFineCapacityReason::CapacityUnachievable;
			OutError = TEXT("EstimateMismatch: Fine pieces allocator capacity");
			return false;
		}
		verify(Geometry->MetadataLease->TryResize(GeometryBase + Geometry->Pieces.GetAllocatedSize()) == EVoxelTerrainResourceAdmission::Granted);
	}
	GeometryBytes = Geometry->GetAllocatedBytes();
	return true;
}

EVoxelFineBuildProgress FVoxelFineShardMeshing::Block(const EVoxelFineCapacityReason InReason,
	const uint64 InWorkingBytes, const uint64 InResultBytes)
{
	CapacityReason = InReason;
	RequiredWorkingBytes = InWorkingBytes;
	RequiredResultBytes = InResultBytes;
	bCapacityResumable = InReason != EVoxelFineCapacityReason::CapacityUnachievable;
	State = bCapacityResumable ? EVoxelFineBuildProgress::CapacityBlocked : EVoxelFineBuildProgress::Failed;
	if (!bCapacityResumable)
	{
		Abort();
	}
	return State;
}

bool FVoxelFineShardMeshing::SplitPending(FString& OutError)
{
	const FVoxelFineShardKey Key = Pending.Last();
	if (Key.SideCells == 1)
	{
		Block(EVoxelFineCapacityReason::CapacityUnachievable, RequiredWorkingBytes, RequiredResultBytes);
		OutError = TEXT("CapacityUnachievable: minimum Fine piece exceeds its hard limit");
		return false;
	}
	if (!ReserveMetadata(Pending.Num() + 7, Geometry->Pieces.Num() + Pending.Num() + 7, OutError))
	{
		BlockedPendingCount = Pending.Num() + 7;
		BlockedPieceCount = Geometry->Pieces.Num() + Pending.Num() + 7;
		Block(CapacityReason, RequiredWorkingBytes, RequiredResultBytes);
		return false;
	}
	Pending.Pop(EAllowShrinking::No);
	const int32 ChildSide = Key.SideCells / 2;
	for (int32 Child = 0; Child < 8; ++Child)
	{
		Pending.Add({Key.Min + FIntVector((Child & 1) ? ChildSide : 0,
			(Child & 2) ? ChildSide : 0, (Child & 4) ? ChildSide : 0), ChildSide});
	}
	OutError.Reset();
	return true;
}

EVoxelFineBuildProgress FVoxelFineShardMeshing::Advance(const TAtomic<bool>& InCancel, FString& OutError)
{
	if (State == EVoxelFineBuildProgress::Canceled || State == EVoxelFineBuildProgress::Failed ||
		State == EVoxelFineBuildProgress::ResultTaken)
	{
		return State;
	}
	if (InCancel.Load())
	{
		Abort();
		State = EVoxelFineBuildProgress::Canceled;
		OutError = TEXT("Canceled");
		return State;
	}
	if (State == EVoxelFineBuildProgress::CapacityBlocked || State == EVoxelFineBuildProgress::Complete)
	{
		return State;
	}
	if (State != EVoxelFineBuildProgress::Working || Pending.IsEmpty() || !Geometry || !Samples)
	{
		OutError = TEXT("Fine mesher has no admitted work");
		Abort();
		State = EVoxelFineBuildProgress::Failed;
		return State;
	}
	FVoxelGenerationRequiredSourceScope SourceScope(&Resources.Get(), this,
		[](const void* InContext, FVoxelGenerationRequiredSourceScope::FVisit InVisit)
		{
			static_cast<const FVoxelFineShardMeshing*>(InContext)->VisitResourceSources(InVisit);
		});
	const FVoxelFineShardKey Key = Pending.Last();
	const FIntVector Offset = Key.Min - Samples->Attempt.Input.Shard.Min;
	int32 Bound = 0;
	if (!PrimitiveBound(Samples->Snapshot, Offset, Key.SideCells, *Registry, *Shapes, Bound))
	{
		OutError = TEXT("Invalid Fine primitive input");
		Abort();
		State = EVoxelFineBuildProgress::Failed;
		return State;
	}
	using namespace VoxelBoundedAllocation;
	const FVoxelFineMeshEstimate Minimum = MinimumPieceMemory(Key.SideCells, Bound);
	RequiredWorkingBytes = Minimum.MetadataBytes + Minimum.MaskBytes;
	RequiredResultBytes = 0;
	if (RequiredWorkingBytes > MaximumPieceBytes)
	{
		SplitPending(OutError);
		return State;
	}
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> WorkingLease = MoveTemp(CapacityWorkingLease);
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResultLease = MoveTemp(CapacityResultLease);
	struct FLocalSources
	{
		const FVoxelTerrainResources* Resources;
		const TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>* Working;
		const TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>* Result;
	};
	const FLocalSources LocalSources{&Resources.Get(), &WorkingLease, &ResultLease};
	FVoxelGenerationRequiredSourceScope LocalScope(&Resources.Get(), &LocalSources,
		[](const void* InContext, FVoxelGenerationRequiredSourceScope::FVisit InVisit)
		{
			const auto& Local = *static_cast<const FLocalSources*>(InContext);
			for (const auto* Lease : {Local.Working->Get(), Local.Result->Get()})
			{
				if (Lease && Lease->IsOwnedBy(*Local.Resources))
				{
					FVoxelGenerationRequiredSourceScope::VisitLease(Lease, InVisit);
				}
			}
		});
	const uint64 MinimumResult = WorkerSharedBytes<FVoxelSectionMeshResult>();
	const uint64 AdditionalMinimumResult = ResultLease && ResultLease->Bytes() >= MinimumResult
		? 0 : MinimumResult - (ResultLease ? ResultLease->Bytes() : 0);
	EVoxelTerrainResourcePool UnachievablePool = EVoxelTerrainResourcePool::Count;
	const auto CountAdmission = VoxelFineShardCapacity::TryReserve(Resources, Consumer, EVoxelTerrainResourcePool::Working,
		RequiredWorkingBytes, WorkingLease, OutError, CapacityVersion, 0, AdditionalMinimumResult, ResultLease ? 0 : 1,
		&UnachievablePool);
	if (CountAdmission != EVoxelTerrainResourceAdmission::Granted)
	{
		if (UnachievablePool == EVoxelTerrainResourcePool::Working && Key.SideCells > 1)
		{
			WorkingLease.Reset();
			ResultLease.Reset();
			SplitPending(OutError);
			return State;
		}
		return Block(CountAdmission == EVoxelTerrainResourceAdmission::CapacityBlocked
			? EVoxelFineCapacityReason::WorkingTokenUnavailable : EVoxelFineCapacityReason::CapacityUnachievable,
			RequiredWorkingBytes, 0);
	}
	FBuildPlan Plan;
	Plan.Estimate = Minimum;
	TArray<FFineFaceKey> Mask;
	if (!Reserve(Plan.Batches, Bound, ArrayBytes<FBatchPlan>(Bound)) ||
		!Reserve(Mask, Key.SideCells * Key.SideCells, Minimum.MaskBytes))
	{
		OutError = TEXT("EstimateMismatch: Fine count capacity");
		Abort();
		State = EVoxelFineBuildProgress::Failed;
		return State;
	}
	Mask.SetNum(Key.SideCells * Key.SideCells, EAllowShrinking::No);
	const auto CountEmitter = [&](EVoxelRenderGroup Group, const FVoxelRuntimeFaceRef& Texture,
		const FVector*, const FVector2D*, const FVector&, bool)
	{
		const uint32 Id = BatchKey(Group, Texture.Bank);
		const int32 Index = LowerBound(Plan.Batches, Id);
		if (!Plan.Batches.IsValidIndex(Index) || Plan.Batches[Index].Key != Id)
		{
			if (Plan.Batches.Num() >= Bound)
			{
				return false;
			}
			Plan.Batches.Insert({Id, 0, 0}, Index);
		}
		FBatchPlan& Batch = Plan.Batches[Index];
		if (Batch.Vertices > MAX_int32 - 4 || Batch.Indices > MAX_int32 - 6)
		{
			return false;
		}
		Batch.Vertices += 4;
		Batch.Indices += 6;
		Plan.Estimate.MaximumVertices += 4;
		Plan.Estimate.MaximumIndices += 6;
		return true;
	};
	if (!VisitFinePrimitives(Samples->Snapshot, Offset, Offset, Key.SideCells, Samples->Snapshot.GridSide,
		TextureRepeatsPerCell, *Registry, *Shapes, Mask, InCancel, CountEmitter))
	{
		OutError = InCancel.Load() ? TEXT("Canceled") : TEXT("EstimateMismatch: Fine count traversal");
		Abort();
		State = InCancel.Load() ? EVoxelFineBuildProgress::Canceled : EVoxelFineBuildProgress::Failed;
		return State;
	}
	const uint64 ActualMetadata = sizeof(Plan) + sizeof(FPrimitiveWorkspace) + Plan.Batches.GetAllocatedSize();
	const uint64 CountPeak = ActualMetadata + Mask.GetAllocatedSize();
	Mask.Empty();
	Plan.Estimate.ResultCapacityBytes = WorkerSharedBytes<FVoxelSectionMeshResult>() + ArrayBytes<FVoxelRenderBatch>(Plan.Batches.Num());
	for (const FBatchPlan& Batch : Plan.Batches)
	{
		Plan.Estimate.ResultCapacityBytes += AttributeBytes(Batch.Vertices, Batch.Indices);
	}
	Plan.Estimate.AdditionalPeakBytes = Minimum.MetadataBytes + Minimum.WorkingBytes + Plan.Estimate.ResultCapacityBytes;
	RequiredWorkingBytes = Minimum.MetadataBytes + Minimum.WorkingBytes;
	RequiredResultBytes = Plan.Estimate.ResultCapacityBytes;
	if (Plan.Estimate.AdditionalPeakBytes > MaximumPieceBytes || Plan.Estimate.MaximumVertices > 262144)
	{
		Plan.Batches.Empty();
		WorkingLease.Reset();
		ResultLease.Reset();
		SplitPending(OutError);
		return State;
	}
	if (GeometryBytes > MaximumTotalGeometryBytes || RequiredResultBytes > MaximumTotalGeometryBytes - GeometryBytes)
	{
		OutError = TEXT("CapacityUnachievable: counted Fine geometry exceeds the Shard hard limit");
		return Block(EVoxelFineCapacityReason::CapacityUnachievable, RequiredWorkingBytes, RequiredResultBytes);
	}
	const uint64 AdditionalResult = ResultLease && ResultLease->Bytes() >= RequiredResultBytes
		? 0 : RequiredResultBytes - (ResultLease ? ResultLease->Bytes() : 0);
	const auto WorkingAdmission = VoxelFineShardCapacity::TryReserve(Resources, Consumer, EVoxelTerrainResourcePool::Working,
		RequiredWorkingBytes, WorkingLease, OutError, CapacityVersion, 0, AdditionalResult, ResultLease ? 0 : 1,
		&UnachievablePool);
	if (WorkingAdmission != EVoxelTerrainResourceAdmission::Granted)
	{
		if (UnachievablePool == EVoxelTerrainResourcePool::Working && Key.SideCells > 1)
		{
			Plan.Batches.Empty();
			WorkingLease.Reset();
			ResultLease.Reset();
			SplitPending(OutError);
			return State;
		}
		return Block(WorkingAdmission == EVoxelTerrainResourceAdmission::CapacityBlocked
			? EVoxelFineCapacityReason::WorkingTokenUnavailable : EVoxelFineCapacityReason::CapacityUnachievable,
			RequiredWorkingBytes, RequiredResultBytes);
	}
	const auto ResultAdmission = VoxelFineShardCapacity::TryReserve(Resources, Consumer, EVoxelTerrainResourcePool::Staged,
		RequiredResultBytes, ResultLease, OutError, CapacityVersion);
	if (ResultAdmission != EVoxelTerrainResourceAdmission::Granted)
	{
		return Block(ResultAdmission == EVoxelTerrainResourceAdmission::CapacityBlocked
			? EVoxelFineCapacityReason::ResultTokenUnavailable : EVoxelFineCapacityReason::CapacityUnachievable,
			RequiredWorkingBytes, RequiredResultBytes);
	}
	FVoxelSectionSnapshot SliceSnapshot;
	if (!Slice(*Samples, Key, SliceSnapshot, OutError) ||
		!Reserve(Mask, Key.SideCells * Key.SideCells, Minimum.MaskBytes))
	{
		Abort();
		State = EVoxelFineBuildProgress::Failed;
		return State;
	}
	Mask.SetNum(Key.SideCells * Key.SideCells, EAllowShrinking::No);
	const uint64 ActualSlice = sizeof(SliceSnapshot) + SliceSnapshot.Blocks.GetAllocatedSize() +
		SliceSnapshot.Halo[0].GetAllocatedSize() + SliceSnapshot.Halo[1].GetAllocatedSize() +
		SliceSnapshot.Halo[2].GetAllocatedSize() + SliceSnapshot.Halo[3].GetAllocatedSize() +
		SliceSnapshot.Halo[4].GetAllocatedSize() + SliceSnapshot.Halo[5].GetAllocatedSize();
	if (ActualSlice > Minimum.SliceBytes)
	{
		OutError = TEXT("EstimateMismatch: Fine Slice allocator capacity");
		Abort();
		State = EVoxelFineBuildProgress::Failed;
		return State;
	}
	auto Candidate = MakeWorkerShared<FVoxelSectionMeshResult>(ResultLease);
	Candidate->Stamp.WorldEpoch = Samples->Attempt.Input.WorldEpoch;
	Candidate->Stamp.Token = Samples->Attempt.Input.Incarnation;
	Candidate->Stamp.Revision = Samples->Attempt.Input.MeshRevision;
	Candidate->Stamp.Section = SliceSnapshot.Section;
	Candidate->ResourceLease = ResultLease;
	if (!Reserve(Candidate->Batches, Plan.Batches.Num(), ArrayBytes<FVoxelRenderBatch>(Plan.Batches.Num())))
	{
		OutError = TEXT("EstimateMismatch: Fine batch capacity");
		Abort();
		State = EVoxelFineBuildProgress::Failed;
		return State;
	}
	for (const FBatchPlan& Batch : Plan.Batches)
	{
		FVoxelRenderBatch& Render = Candidate->Batches.AddDefaulted_GetRef();
		Render.Group = static_cast<EVoxelRenderGroup>(Batch.Key >> 16);
		Render.Bank = uint16(Batch.Key);
		if (!ReserveMesh(Render.Mesh, Batch))
		{
			OutError = TEXT("EstimateMismatch: Fine attribute capacity");
			Abort();
			State = EVoxelFineBuildProgress::Failed;
			return State;
		}
	}
	const auto BuildEmitter = [&](const EVoxelRenderGroup Group, const FVoxelRuntimeFaceRef& Texture,
		const FVector* Vertices, const FVector2D* UV, const FVector& Cell, const bool bPlant)
	{
		const uint32 Id = BatchKey(Group, Texture.Bank);
		const int32 Index = LowerBound(Plan.Batches, Id);
		if (!Plan.Batches.IsValidIndex(Index) || Plan.Batches[Index].Key != Id)
		{
			return false;
		}
		FVoxelMeshBuffers& Mesh = Candidate->Batches[Index].Mesh;
		const FBatchPlan& Batch = Plan.Batches[Index];
		const int32 Base = Mesh.Vertices.Num();
		if (Base > Batch.Vertices - 4 || Mesh.Triangles.Num() > Batch.Indices - 6)
		{
			return false;
		}
		const FVector Normal = FVector::CrossProduct(Vertices[1] - Vertices[0], Vertices[2] - Vertices[0]).GetSafeNormal();
		const FVector Tangent = (Vertices[1] - Vertices[0]).GetSafeNormal();
		for (int32 Vertex = 0; Vertex < 4; ++Vertex)
		{
			Mesh.Vertices.Add(Vertices[Vertex] + Cell);
			Mesh.Normals.Add(Normal);
			Mesh.UV0.Add(UV[Vertex]);
			Mesh.UV1.Add({double(Texture.Layer), double(Texture.Frames)});
			Mesh.UV2.Add({double(Texture.FPS), 0.0});
			const float Wind = bPlant && Group == EVoxelRenderGroup::Foliage ? float(FMath::Clamp(Vertices[Vertex].Z, 0.0, 1.0)) : 0.0f;
			Mesh.Colors.Add(Group == EVoxelRenderGroup::Opaque ? Texture.Tint : FLinearColor(Wind, 0, 1, 1));
			Mesh.Tangents.Add(FProcMeshTangent(Tangent, false));
		}
		const int32 Indices[] = {Base, Base + 2, Base + 1, Base, Base + 3, Base + 2};
		Mesh.Triangles.Append(Indices, UE_ARRAY_COUNT(Indices));
		return true;
	};
	if (!VisitFinePrimitives(SliceSnapshot, FIntVector::ZeroValue, Offset, Key.SideCells, Samples->Snapshot.GridSide,
		TextureRepeatsPerCell, *Registry, *Shapes, Mask, InCancel, BuildEmitter))
	{
		OutError = InCancel.Load() ? TEXT("Canceled") : TEXT("EstimateMismatch: Fine bounded build");
		Abort();
		State = InCancel.Load() ? EVoxelFineBuildProgress::Canceled : EVoxelFineBuildProgress::Failed;
		return State;
	}
	uint64 ActualResult = WorkerSharedBytes<FVoxelSectionMeshResult>() + Candidate->Batches.GetAllocatedSize();
	bool bHasTriangles = false;
	for (int32 Index = 0; Index < Candidate->Batches.Num(); ++Index)
	{
		const FVoxelMeshBuffers& Mesh = Candidate->Batches[Index].Mesh;
		if (Mesh.Vertices.Num() != Plan.Batches[Index].Vertices || Mesh.Triangles.Num() != Plan.Batches[Index].Indices)
		{
			OutError = TEXT("EstimateMismatch: Fine Count and Build disagree");
			Abort();
			State = EVoxelFineBuildProgress::Failed;
			return State;
		}
		ActualResult += Mesh.Vertices.GetAllocatedSize() + Mesh.Normals.GetAllocatedSize() + Mesh.Triangles.GetAllocatedSize() +
			Mesh.UV0.GetAllocatedSize() + Mesh.UV1.GetAllocatedSize() + Mesh.UV2.GetAllocatedSize() +
			Mesh.Colors.GetAllocatedSize() + Mesh.Tangents.GetAllocatedSize();
		bHasTriangles |= !Mesh.Triangles.IsEmpty();
	}
	if (ActualResult > RequiredResultBytes || !Candidate->ValidateAndCache() || InCancel.Load())
	{
		OutError = InCancel.Load() ? TEXT("Canceled") : TEXT("EstimateMismatch: Fine result validation");
		Abort();
		State = InCancel.Load() ? EVoxelFineBuildProgress::Canceled : EVoxelFineBuildProgress::Failed;
		return State;
	}
	if (!Candidate->TryCompactAttributes(Resources, Consumer, OutError, &InCancel))
	{
		Abort();
		State = InCancel.Load() ? EVoxelFineBuildProgress::Canceled : EVoxelFineBuildProgress::Failed;
		return State;
	}
	ActualResult = WorkerSharedBytes<FVoxelSectionMeshResult>() + Candidate->AllocatedBytes();
	verify(ResultLease->TryResize(ActualResult) == EVoxelTerrainResourceAdmission::Granted);
	check(Geometry->Pieces.Num() < Geometry->Pieces.Max());
	Geometry->Pieces.Add({Key, Candidate});
	Geometry->bKnownEmpty &= !bHasTriangles;
	Geometry->PeakTemporaryBytes = FMath::Max(Geometry->PeakTemporaryBytes,
		FMath::Max(CountPeak, ActualMetadata + ActualSlice + Mask.GetAllocatedSize()));
	++Geometry->BuildMicrosteps;
	GeometryBytes = Geometry->GetAllocatedBytes();
	CompletedVolume += uint64(Key.SideCells) * Key.SideCells * Key.SideCells;
	Pending.Pop(EAllowShrinking::No);
	if (Pending.IsEmpty())
	{
		const uint64 Side = Samples->Attempt.Input.Shard.SideCells;
		// Initial Atoms are disjoint; subdivision replaces one pending cube by its
		// eight exact children. Each committed Piece is popped once, conserving volume.
		bCoverageValidated = CompletedVolume == Side * Side * Side && GeometryBytes <= MaximumTotalGeometryBytes;
		if (!bCoverageValidated)
		{
			OutError = TEXT("Fine Piece partition does not cover the canonical Shard");
			Abort();
			State = EVoxelFineBuildProgress::Failed;
			return State;
		}
		Geometry->bCoverageValidated = true;
		State = EVoxelFineBuildProgress::Complete;
	}
	OutError.Reset();
	return State;
}

bool FVoxelFineShardMeshing::ResumeCapacity(const uint64 InCapacityVersion, FString& OutError)
{
	if (State != EVoxelFineBuildProgress::CapacityBlocked || !bCapacityResumable)
	{
		OutError = TEXT("InvalidRequest: Fine capacity reason is not resumable");
		return false;
	}
	if (InCapacityVersion <= CapacityVersion ||
		Resources->ReleaseVersionExcludingOwner(Consumer) < InCapacityVersion)
	{
		OutError = TEXT("CapacityBlocked: Fine capacity requires a newer external resource event");
		return false;
	}
	FVoxelGenerationRequiredSourceScope SourceScope(&Resources.Get(), this,
		[](const void* InContext, FVoxelGenerationRequiredSourceScope::FVisit InVisit)
		{
			static_cast<const FVoxelFineShardMeshing*>(InContext)->VisitResourceSources(InVisit);
		});
	if (BlockedPendingCount > 0)
	{
		if (!ReserveMetadata(BlockedPendingCount, BlockedPieceCount, OutError))
		{
			Block(CapacityReason, RequiredWorkingBytes, RequiredResultBytes);
			return false;
		}
		BlockedPendingCount = 0;
		BlockedPieceCount = 0;
		RequiredWorkingBytes = 0;
		RequiredResultBytes = 0;
	}
	else
	{
		if (RequiredWorkingBytes > 0)
		{
			const uint64 AdditionalResult = CapacityResultLease && CapacityResultLease->Bytes() >= RequiredResultBytes
				? 0 : RequiredResultBytes - (CapacityResultLease ? CapacityResultLease->Bytes() : 0);
			EVoxelTerrainResourcePool UnachievablePool = EVoxelTerrainResourcePool::Count;
			const auto Admission = VoxelFineShardCapacity::TryReserve(Resources, Consumer, EVoxelTerrainResourcePool::Working,
				RequiredWorkingBytes, CapacityWorkingLease, OutError, CapacityVersion, 0, AdditionalResult,
				RequiredResultBytes && !CapacityResultLease ? 1 : 0, &UnachievablePool);
			if (Admission != EVoxelTerrainResourceAdmission::Granted)
			{
				CapacityWorkingLease.Reset();
				CapacityResultLease.Reset();
				if (UnachievablePool == EVoxelTerrainResourcePool::Working && !Pending.IsEmpty() && Pending.Last().SideCells > 1)
				{
					if (!SplitPending(OutError))
					{
						return false;
					}
					CapacityReason = EVoxelFineCapacityReason::None;
					bCapacityResumable = false;
					State = EVoxelFineBuildProgress::Working;
					return true;
				}
				Block(Admission == EVoxelTerrainResourceAdmission::CapacityBlocked
					? EVoxelFineCapacityReason::WorkingTokenUnavailable : EVoxelFineCapacityReason::CapacityUnachievable,
					RequiredWorkingBytes, RequiredResultBytes);
				return false;
			}
		}
		if (RequiredResultBytes > 0)
		{
			const auto Admission = VoxelFineShardCapacity::TryReserve(Resources, Consumer, EVoxelTerrainResourcePool::Staged,
				RequiredResultBytes, CapacityResultLease, OutError, CapacityVersion);
			if (Admission != EVoxelTerrainResourceAdmission::Granted)
			{
				CapacityWorkingLease.Reset();
				CapacityResultLease.Reset();
				Block(Admission == EVoxelTerrainResourceAdmission::CapacityBlocked
					? EVoxelFineCapacityReason::ResultTokenUnavailable : EVoxelFineCapacityReason::CapacityUnachievable,
					RequiredWorkingBytes, RequiredResultBytes);
				return false;
			}
		}
	}
	CapacityReason = EVoxelFineCapacityReason::None;
	bCapacityResumable = false;
	State = EVoxelFineBuildProgress::Working;
	OutError.Reset();
	return true;
}

TSharedPtr<const FVoxelFineShardGeometry, ESPMode::ThreadSafe> FVoxelFineShardMeshing::TakeResult()
{
	if (State != EVoxelFineBuildProgress::Complete || !Pending.IsEmpty() || !bCoverageValidated || !Geometry ||
		Geometry->GetAllocatedBytes() != GeometryBytes || GeometryBytes > MaximumTotalGeometryBytes)
	{
		return nullptr;
	}
	Geometry->MeshCapacityBytes = GeometryBytes;
	for (const auto& Piece : Geometry->Pieces)
	{
		Geometry->VertexCount += Piece.Mesh->VertexCount;
		Geometry->TriangleCount += Piece.Mesh->TriangleCount;
	}
	TSharedPtr<const FVoxelFineShardGeometry, ESPMode::ThreadSafe> Result = MoveTemp(Geometry);
	Pending.Empty();
	PendingLease.Reset();
	CapacityWorkingLease.Reset();
	CapacityResultLease.Reset();
	Samples.Reset();
	State = EVoxelFineBuildProgress::ResultTaken;
	return Result;
}

void FVoxelFineShardMeshing::Abort()
{
	Geometry.Reset();
	Pending.Empty();
	PendingLease.Reset();
	CapacityWorkingLease.Reset();
	CapacityResultLease.Reset();
	Samples.Reset();
	GeometryBytes = 0;
	CompletedVolume = 0;
	bCoverageValidated = false;
}

uint64 FVoxelFineShardMeshing::AllocatedBytes() const
{
	return sizeof(*this) + Pending.GetAllocatedSize() + (Samples ? Samples->AllocatedBytes() : 0) +
		(Geometry ? Geometry->GetAllocatedBytes() : 0);
}

bool FVoxelFineShardMeshing::IsCapacityResumable() const
{
	return State == EVoxelFineBuildProgress::CapacityBlocked && bCapacityResumable;
}

EVoxelFineCapacityReason FVoxelFineShardMeshing::GetCapacityReason() const
{
	return CapacityReason;
}
