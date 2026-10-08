#include "Voxel/Rendering/VoxelFineShardBuild.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"
#include "Voxel/Generation/VoxelGenerationPlanningRead.h"

uint64 FVoxelFineShardGeometry::GetResourceAccountedBytes() const
{
	uint64 Bytes = MetadataLease ? MetadataLease->Bytes() : 0;
	Bytes += Boundary && Boundary->ResourceLease ? Boundary->ResourceLease->Bytes() : 0;
	for (const FVoxelFineMeshPiece& Piece : Pieces)
	{
		// 每个 Piece 在提交时取得独立租约；这里不再临时创建 GT 去重表。
		if (Piece.Mesh && Piece.Mesh->ResourceLease)
		{
			Bytes += Piece.Mesh->ResourceLease->Bytes();
		}
	}
	return Bytes;
}

bool FVoxelFineShardGeometry::TransferResources(const EVoxelTerrainResourcePool InPool) const
{
	bResidentResourcesReady.Store(false);
	bool bTransferred = MetadataLease && MetadataLease->Transfer(InPool);
	bTransferred &= Boundary && Boundary->ResourceLease && Boundary->ResourceLease->Transfer(InPool);
	for (const FVoxelFineMeshPiece& Piece : Pieces)
	{
		bTransferred &= Piece.Mesh && Piece.Mesh->ResourceLease && Piece.Mesh->ResourceLease->Transfer(InPool);
	}
	bResidentResourcesReady.Store(bTransferred && InPool == EVoxelTerrainResourcePool::Resident);
	return bTransferred;
}

uint64 FVoxelFineBoundarySnapshot::GetAllocatedBytes() const
{
	uint64 Bytes = VoxelBoundedAllocation::WorkerSharedBytes<FVoxelFineBoundarySnapshot>();
	for (int32 Face = 0; Face < 6; ++Face)
	{
		Bytes += Owned[Face].GetAllocatedSize() + Halo[Face].GetAllocatedSize();
	}
	Bytes += Atoms.GetAllocatedSize();
	for (const auto& Atom : Atoms)
	{
		for (int32 Face = 0; Face < 6; ++Face) Bytes += Atom.Owned[Face].GetAllocatedSize() + Atom.Halo[Face].GetAllocatedSize();
	}
	return Bytes;
}

bool FVoxelFineBoundarySnapshot::Validate() const
{
	FString Error;
	if (!Input.Shard.Validate(Error) || !ResourceLease || ResourceLease->Bytes() < GetAllocatedBytes())
	{
		return false;
	}
	for (int32 Face = 0; Face < 6; ++Face)
	{
		if ((Owned[Face].Num() != 1 && Owned[Face].Num() != Input.Shard.SideCells * Input.Shard.SideCells) ||
			(Halo[Face].Num() != 1 && Halo[Face].Num() != Input.Shard.SideCells * Input.Shard.SideCells))
		{
			return false;
		}
	}
	const int32 Side = Input.Shard.SideCells / 16;
	if (Side <= 0 || Atoms.Num() != Side * Side * Side) return false;
	for (int32 Index = 0; Index < Atoms.Num(); ++Index)
	{
		const auto& Atom = Atoms[Index];
		if (Atom.Section != Input.Shard.Min / 16 + FIntVector(Index % Side, (Index / Side) % Side, Index / (Side * Side))) return false;
		for (int32 Face = 0; Face < 6; ++Face)
		{
			if ((Atom.Owned[Face].Num() != 1 && Atom.Owned[Face].Num() != 256) ||
				(Atom.Halo[Face].Num() != 1 && Atom.Halo[Face].Num() != 256)) return false;
		}
	}
	return true;
}

bool FVoxelFineBoundarySnapshot::SampleFace(const uint8 InFace, const bool bInHalo,
	const FIntVector& InWorldCell, FVoxelBlockState& OutState) const
{
	if (InFace >= 6) return false;
	const FIntVector Local = InWorldCell - Input.Shard.Min;
	const int32 Side = Input.Shard.SideCells;
	const int32 Axis = InFace / 2;
	const int32 U = (Axis + 1) % 3;
	const int32 V = (Axis + 2) % 3;
	const int32 Plane = bInHalo ? ((InFace & 1) ? -1 : Side) : ((InFace & 1) ? 0 : Side - 1);
	if (Local[Axis] != Plane || Local[U] < 0 || Local[U] >= Side || Local[V] < 0 || Local[V] >= Side)
	{
		return false;
	}
	const TArray<uint32>& Face = bInHalo ? Halo[InFace] : Owned[InFace];
	const int32 Index = Face.Num() == 1 ? 0 : Local[U] + Side * Local[V];
	if (!Face.IsValidIndex(Index)) return false;
	OutState = FVoxelBlockState::Unpack(Face[Index]);
	return true;
}

bool FVoxelFineBoundarySnapshot::SampleAtomFace(const FIntVector& InSection, const uint8 InFace,
	const bool bInHalo, const FIntVector& InWorldCell, FVoxelBlockState& OutState) const
{
	if (InFace >= 6) return false;
	const int32 AtomSide = Input.Shard.SideCells / 16;
	const FIntVector Offset = InSection - Input.Shard.Min / 16;
	if (Offset.GetMin() < 0 || Offset.GetMax() >= AtomSide) return false;
	const int32 AtomIndex = Offset.X + AtomSide * (Offset.Y + AtomSide * Offset.Z);
	if (!Atoms.IsValidIndex(AtomIndex) || Atoms[AtomIndex].Section != InSection) return false;
	const FIntVector Local = InWorldCell - InSection * 16;
	const int32 Axis = InFace / 2;
	const int32 U = (Axis + 1) % 3;
	const int32 V = (Axis + 2) % 3;
	const int32 Plane = bInHalo ? ((InFace & 1) ? -1 : 16) : ((InFace & 1) ? 0 : 15);
	if (Local[Axis] != Plane || Local[U] < 0 || Local[U] >= 16 || Local[V] < 0 || Local[V] >= 16) return false;
	const TArray<uint32>& Face = bInHalo ? Atoms[AtomIndex].Halo[InFace] : Atoms[AtomIndex].Owned[InFace];
	const int32 Index = Face.Num() == 1 ? 0 : Local[U] + 16 * Local[V];
	if (!Face.IsValidIndex(Index)) return false;
	OutState = FVoxelBlockState::Unpack(Face[Index]);
	return true;
}

uint64 FVoxelFineShardSamples::AllocatedBytes() const
{
	uint64 Bytes = VoxelBoundedAllocation::WorkerSharedBytes<FVoxelFineShardSamples>() + Snapshot.Blocks.GetAllocatedSize();
	for (const TArray<uint32>& Face : Snapshot.Halo)
	{
		Bytes += Face.GetAllocatedSize();
	}
	return Bytes;
}

FVoxelFineShardSampler::FVoxelFineShardSampler(TSharedRef<const FVoxelGenerationPipeline, ESPMode::ThreadSafe> InGenerator,
	TSharedRef<const FVoxelOverlaySnapshotSet, ESPMode::ThreadSafe> InOverlay,
	TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
	const FVoxelGenerationConsumerKey& InConsumer, TSharedPtr<const void, ESPMode::ThreadSafe> InRegistrationLease,
	TSharedPtr<const FVoxelFineOverlayVersionProof, ESPMode::ThreadSafe> InOverlayVersion)
	: Generator(InGenerator), Overlay(InOverlay), Resources(InResources), Consumer(InConsumer),
	RegistrationLease(MoveTemp(InRegistrationLease)), OverlayVersion(MoveTemp(InOverlayVersion))
{
}

bool FVoxelFineShardSampler::GatherRequiredSections(const FVoxelFineShardKey& InKey,
	TArray<FIntVector>& OutSections, FString& OutError)
{
	if (!InKey.Validate(OutError) || InKey.Min.GetMin() <= MIN_int32 + 16 ||
		int64(InKey.Min.GetMax()) + InKey.SideCells + 16 >= MAX_int32)
	{
		OutError = TEXT("Fine Shard 无法安全建立一圈 Halo");
		return false;
	}
	const FVoxelGenerationBounds Core = InKey.Bounds();
	TArray<FVoxelGenerationBounds, TInlineAllocator<7>> Pieces;
	Pieces.Add(Core);
	for (int32 Face = 0; Face < 6; ++Face)
	{
		const int32 Axis = Face / 2;
		FVoxelGenerationBounds Halo = Core;
		if (Face & 1)
		{
			Halo.Max[Axis] = Core.Min[Axis];
			Halo.Min[Axis] = Core.Min[Axis] - 1;
		}
		else
		{
			Halo.Min[Axis] = Core.Max[Axis];
			Halo.Max[Axis] = Core.Max[Axis] + 1;
		}
		Pieces.Add(Halo);
	}
	OutSections.Reset();
	const int32 AxisCount = InKey.SideCells / 16 + 3;
	OutSections.Reserve(AxisCount * AxisCount * AxisCount);
	for (const FVoxelGenerationBounds& Piece : Pieces)
	{
		const FIntVector First(int32(FVoxelFineSpatial::FloorDivide(Piece.Min.X, 16)),
			int32(FVoxelFineSpatial::FloorDivide(Piece.Min.Y, 16)), int32(FVoxelFineSpatial::FloorDivide(Piece.Min.Z, 16)));
		const FIntVector Last(int32(FVoxelFineSpatial::FloorDivide(int64(Piece.Max.X) - 1, 16)),
			int32(FVoxelFineSpatial::FloorDivide(int64(Piece.Max.Y) - 1, 16)),
			int32(FVoxelFineSpatial::FloorDivide(int64(Piece.Max.Z) - 1, 16)));
		for (int32 SectionZ = First.Z; SectionZ <= Last.Z; ++SectionZ)
		{
			for (int32 SectionY = First.Y; SectionY <= Last.Y; ++SectionY)
			{
				for (int32 SectionX = First.X; SectionX <= Last.X; ++SectionX)
				{
					OutSections.AddUnique(FIntVector(SectionX, SectionY, SectionZ));
				}
			}
		}
	}
	OutSections.Sort([](const FIntVector& First, const FIntVector& Second)
	{
		if (First.Z != Second.Z)
		{
			return First.Z < Second.Z;
		}
		if (First.Y != Second.Y)
		{
			return First.Y < Second.Y;
		}
		return First.X < Second.X;
	});
	OutError.Reset();
	return true;
}

bool FVoxelFineShardSampler::Initialize(const FVoxelFineBuildAttempt& InAttempt, const uint64 InRecipeHash,
	const int32 InWorldMinZ, const int32 InWorldMaxZ, FString& OutError)
{
	if (bInitialized || Samples || InputLease || SamplingLease || InAttempt.Serial == 0 || InAttempt.Input.WorldEpoch == 0 || InAttempt.Input.Incarnation == 0 ||
		InWorldMinZ >= InWorldMaxZ || InWorldMinZ % 16 != 0 || InWorldMaxZ % 16 != 0 ||
		!Consumer.IsValid() || Consumer.Kind != EVoxelGenerationConsumerKind::FineShard ||
		Consumer.WorldEpoch != InAttempt.Input.WorldEpoch || Consumer.OwnerIncarnation != InAttempt.Input.Incarnation ||
		Consumer.RequestSerial != InAttempt.Serial || Consumer.Min != InAttempt.Input.Shard.Min ||
		Consumer.Max != InAttempt.Input.Shard.Bounds().Max || !InAttempt.Input.Shard.Validate(OutError))
	{
		if (OutError.IsEmpty())
		{
			OutError = TEXT("Fine 采样状态不能初始化");
		}
		return false;
	}
	WorldMinZ = InWorldMinZ;
	WorldMaxZ = InWorldMaxZ;
	using namespace VoxelBoundedAllocation;
	const int32 Side = InAttempt.Input.Shard.SideCells;
	const int32 AxisCount = Side / 16 + 3;
	const uint64 SampleBytes = WorkerSharedBytes<FVoxelFineShardSamples>() + ArrayBytes<uint32>(Side * Side * Side) +
		6 * ArrayBytes<uint32>(Side * Side);
	// The pipeline produces one temporary 16^3 section and a 16-symbol column.
	// Its own generation/overlay workers retain their independent fact reservations.
	const uint64 SamplingBytes = sizeof(*this) + ArrayBytes<FIntVector>(AxisCount * AxisCount * AxisCount) +
		InitialGrowBytes<FVoxelBlockState>(4096) + 2 * InitialGrowBytes<uint32>(16);
	if (!Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Input, SampleBytes, InputLease) ||
		!Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Working, SamplingBytes, SamplingLease))
	{
		InputLease.Reset();
		SamplingLease.Reset();
		OutError = TEXT("CapacityBlocked: Fine sampling reservation unavailable");
		return false;
	}
	if (!GatherRequiredSections(InAttempt.Input.Shard, RequiredSections, OutError))
	{
		RequiredSections.Empty();
		InputLease.Reset();
		SamplingLease.Reset();
		return false;
	}
	Samples = MakeWorkerShared<FVoxelFineShardSamples>(InputLease);
	Samples->RegistrationLease = RegistrationLease;
	Samples->OverlayVersion = OverlayVersion;
	Samples->Attempt = InAttempt;
	Samples->ResourceLease = InputLease;
	auto& Snapshot = Samples->Snapshot;
	Snapshot.GridSide = Side;
	Snapshot.Section = FIntVector(int32(FVoxelFineSpatial::FloorDivide(InAttempt.Input.Shard.Min.X, 16)),
		int32(FVoxelFineSpatial::FloorDivide(InAttempt.Input.Shard.Min.Y, 16)),
		int32(FVoxelFineSpatial::FloorDivide(InAttempt.Input.Shard.Min.Z, 16)));
	Snapshot.Stamp.Epoch = InAttempt.Input.WorldEpoch;
	Snapshot.Stamp.Token = InAttempt.Input.Incarnation;
	Snapshot.Stamp.RecipeHash = InRecipeHash;
	Snapshot.Revision = InAttempt.Input.MeshRevision;
	Snapshot.Blocks.Reserve(Side * Side * Side);
	Snapshot.Blocks.Init(0, Side * Side * Side);
	for (int32 Face = 0; Face < 6; ++Face)
	{
		Snapshot.Halo[Face].Reserve(Side * Side);
		Snapshot.Halo[Face].Init(0, Side * Side);
		Snapshot.Known[Face] = false;
	}
	NextSection = 0;
	bComplete = false;
	if (Samples->AllocatedBytes() > SampleBytes || RequiredSections.GetAllocatedSize() > ArrayBytes<FIntVector>(AxisCount * AxisCount * AxisCount))
	{
		OutError = TEXT("EstimateMismatch: Fine sampling allocator capacity");
		Samples.Reset();
		RequiredSections.Empty();
		InputLease.Reset();
		SamplingLease.Reset();
		return false;
	}
	bInitialized = true;
	OutError.Reset();
	return true;
}

void FVoxelFineShardSampler::CopySection(const FIntVector& InSection, TConstArrayView<FVoxelBlockState> InBlocks)
{
	check(InBlocks.Num() == 4096 && Samples);
	auto& Snapshot = Samples->Snapshot;
	const int32 Side = Snapshot.GridSide;
	const FIntVector SectionMin = InSection * 16;
	const FIntVector ShardMin = Samples->Attempt.Input.Shard.Min;
	for (int32 CellZ = 0; CellZ < 16; ++CellZ)
	{
		for (int32 CellY = 0; CellY < 16; ++CellY)
		{
			for (int32 CellX = 0; CellX < 16; ++CellX)
			{
				const FIntVector World = SectionMin + FIntVector(CellX, CellY, CellZ);
				const FIntVector Local = World - ShardMin;
				FVoxelBlockState State = InBlocks[CellX + 16 * (CellY + 16 * CellZ)];
				if (World.Z >= WorldMinZ && World.Z < WorldMaxZ)
				{
					Overlay->ApplyAt(World, State);
				}
				else
				{
					State = FVoxelBlockState();
				}
				if (Local.X >= 0 && Local.X < Side && Local.Y >= 0 && Local.Y < Side && Local.Z >= 0 && Local.Z < Side)
				{
					Snapshot.Blocks[Local.X + Side * (Local.Y + Side * Local.Z)] = State.Pack();
					continue;
				}
				for (int32 Face = 0; Face < 6; ++Face)
				{
					const int32 Axis = Face / 2;
					const int32 HorizontalAxis = (Axis + 1) % 3;
					const int32 VerticalAxis = (Axis + 2) % 3;
					const int32 Plane = (Face & 1) ? -1 : Side;
					if (Local[Axis] == Plane && Local[HorizontalAxis] >= 0 && Local[HorizontalAxis] < Side &&
						Local[VerticalAxis] >= 0 && Local[VerticalAxis] < Side)
					{
						Snapshot.Halo[Face][Local[HorizontalAxis] + Side * Local[VerticalAxis]] = State.Pack();
					}
				}
			}
		}
	}
}

EVoxelFineBuildProgress FVoxelFineShardSampler::Advance(const int32 InMaximumSections,
	const TAtomic<bool>& InCancel, FString& OutError)
{
	FVoxelGenerationConsumerScope ConsumerScope(Consumer);
	if (!Samples || InMaximumSections <= 0)
	{
		OutError = TEXT("Fine 采样任务没有有效执行窗口");
		return EVoxelFineBuildProgress::Failed;
	}
	if (InCancel.Load())
	{
		OutError = TEXT("Canceled");
		return EVoxelFineBuildProgress::Canceled;
	}
	if (bComplete)
	{
		return EVoxelFineBuildProgress::Complete;
	}
	for (int32 Work = 0; Work < InMaximumSections && RequiredSections.IsValidIndex(NextSection); ++Work)
	{
		const FIntVector Key = RequiredSections[NextSection];
		TArray<FVoxelBlockState> Blocks;
		const int64 SectionMinZ = int64(Key.Z) * 16;
		if (SectionMinZ >= WorldMinZ && SectionMinZ + 16 <= WorldMaxZ)
		{
			if (!Generator->GenerateSection(Key, Blocks, OutError, &InCancel))
			{
				if (InCancel.Load())
				{
					return EVoxelFineBuildProgress::Canceled;
				}
				if (OutError.StartsWith(TEXT("CapacityBlocked"))) return EVoxelFineBuildProgress::CapacityBlocked;
				return OutError.StartsWith(TEXT("DependencyNotReady")) ?
					EVoxelFineBuildProgress::DependencyNotReady : EVoxelFineBuildProgress::Failed;
			}
		}
		else
		{
			Blocks.Init(FVoxelBlockState(), 4096);
		}
		if (Blocks.Num() != 4096)
		{
			OutError = TEXT("精确生成器返回的 Section 大小错误");
			return EVoxelFineBuildProgress::Failed;
		}
		CopySection(Key, Blocks);
		++NextSection;
		if (InCancel.Load())
		{
			OutError = TEXT("Canceled");
			return EVoxelFineBuildProgress::Canceled;
		}
	}
	if (NextSection == RequiredSections.Num())
	{
		for (bool& bKnown : Samples->Snapshot.Known)
		{
			bKnown = true;
		}
		bComplete = true;
	}
	OutError.Reset();
	return bComplete ? EVoxelFineBuildProgress::Complete : EVoxelFineBuildProgress::Working;
}

TSharedPtr<const FVoxelFineShardSamples, ESPMode::ThreadSafe> FVoxelFineShardSampler::TakeResult()
{
	if (!bComplete || !Samples)
	{
		return nullptr;
	}
	TSharedPtr<const FVoxelFineShardSamples, ESPMode::ThreadSafe> Result = Samples;
	bComplete = false;
	Samples.Reset();
	RequiredSections.Empty();
	InputLease->Resize(Result->AllocatedBytes());
	InputLease.Reset();
	SamplingLease.Reset();
	return Result;
}

uint64 FVoxelFineShardSampler::AllocatedBytes() const
{
	return sizeof(*this) + RequiredSections.GetAllocatedSize() + Overlay->GetAllocatedBytes() +
		(Samples ? Samples->AllocatedBytes() : 0) + 4096ull * sizeof(FVoxelBlockState);
}

const TArray<FIntVector>& FVoxelFineShardSampler::GetRequiredSections() const
{
	return RequiredSections;
}

uint64 FVoxelFineShardGeometry::GetAllocatedBytes() const
{
	uint64 Bytes = VoxelBoundedAllocation::WorkerSharedBytes<FVoxelFineShardGeometry>() + Pieces.GetAllocatedSize();
	Bytes += Boundary ? Boundary->GetAllocatedBytes() : 0;
	for (const FVoxelFineMeshPiece& Piece : Pieces)
	{
		if (!Piece.Mesh)
		{
			continue;
		}
		Bytes += VoxelBoundedAllocation::WorkerSharedBytes<FVoxelSectionMeshResult>() + Piece.Mesh->Batches.GetAllocatedSize();
		for (const FVoxelRenderBatch& Batch : Piece.Mesh->Batches)
		{
			Bytes += Batch.Mesh.Vertices.GetAllocatedSize() + Batch.Mesh.Normals.GetAllocatedSize() +
				Batch.Mesh.Triangles.GetAllocatedSize() + Batch.Mesh.UV0.GetAllocatedSize() +
				Batch.Mesh.UV1.GetAllocatedSize() + Batch.Mesh.UV2.GetAllocatedSize() +
				Batch.Mesh.Colors.GetAllocatedSize() + Batch.Mesh.Tangents.GetAllocatedSize();
		}
	}
	return Bytes;
}

bool FVoxelFineShardMeshing::Slice(const FVoxelFineShardSamples& InSource, const FVoxelFineShardKey& InWindow,
	FVoxelSectionSnapshot& OutSnapshot, FString& OutError)
{
	if (!InWindow.Validate(OutError) || !FVoxelFineSpatial::ContainsBounds(InSource.Attempt.Input.Shard.Bounds(), InWindow.Bounds()))
	{
		OutError = TEXT("Fine 子分片不在原始精确快照内");
		return false;
	}
	FVoxelSectionSnapshot Candidate;
	Candidate.Stamp = InSource.Snapshot.Stamp;
	Candidate.Revision = InSource.Snapshot.Revision;
	Candidate.Section = InSource.Snapshot.Section;
	Candidate.GridSide = InWindow.SideCells;
	const int32 Side = Candidate.GridSide;
	const FIntVector Offset = InWindow.Min - InSource.Attempt.Input.Shard.Min;
	Candidate.Blocks.Reserve(Side * Side * Side);
	Candidate.Blocks.SetNumUninitialized(Side * Side * Side);
	for (int32 CellZ = 0; CellZ < Side; ++CellZ)
	{
		for (int32 CellY = 0; CellY < Side; ++CellY)
		{
			for (int32 CellX = 0; CellX < Side; ++CellX)
			{
				FVoxelBlockState State;
				if (!InSource.Snapshot.TrySample(Offset + FIntVector(CellX, CellY, CellZ), State))
				{
					OutError = TEXT("Fine 子分片中心数据不完整");
					return false;
				}
				Candidate.Blocks[CellX + Side * (CellY + Side * CellZ)] = State.Pack();
			}
		}
	}
	for (int32 Face = 0; Face < 6; ++Face)
	{
		const int32 Axis = Face / 2;
		const int32 HorizontalAxis = (Axis + 1) % 3;
		const int32 VerticalAxis = (Axis + 2) % 3;
		Candidate.Halo[Face].Reserve(Side * Side);
		Candidate.Halo[Face].SetNumUninitialized(Side * Side);
		for (int32 Vertical = 0; Vertical < Side; ++Vertical)
		{
			for (int32 Horizontal = 0; Horizontal < Side; ++Horizontal)
			{
				FIntVector Local = Offset;
				Local[Axis] += (Face & 1) ? -1 : Side;
				Local[HorizontalAxis] += Horizontal;
				Local[VerticalAxis] += Vertical;
				FVoxelBlockState State;
				if (!InSource.Snapshot.TrySample(Local, State))
				{
					OutError = TEXT("Fine 子分片 Halo 不完整");
					return false;
				}
				Candidate.Halo[Face][Horizontal + Side * Vertical] = State.Pack();
			}
		}
		Candidate.Known[Face] = true;
	}
	OutSnapshot = MoveTemp(Candidate);
	OutError.Reset();
	return true;
}
