#include "Voxel/Navigation/VoxelNavigationTypes.h"

#include "Voxel/Geometry/VoxelShapeRegistry.h"
#include "Voxel/Collision/VoxelCollisionPresenter.h"
#include "Voxel/Runtime/VoxelRegistry.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"
#include "Misc/ScopeLock.h"

IVoxelNavigationInvalidationConsumer::~IVoxelNavigationInvalidationConsumer()
{
}

FVoxelNavigationInvalidationObserver::FVoxelNavigationInvalidationObserver()
{
}

FVoxelNavigationInvalidationObserver::~FVoxelNavigationInvalidationObserver()
{
	Detach();
}

FVoxelNavigationInvalidationObserver::FVoxelNavigationInvalidationObserver(FVoxelNavigationInvalidationObserver&& InOther)
{
	// 数组仅在订阅前 Reserve；已链接的节点不得被搬移。
	check(!InOther.Source.Load());
}

bool FVoxelNavigationInvalidationObserver::Attach(FVoxelNavigationInvalidationSource& InSource,
	TWeakPtr<IVoxelNavigationInvalidationConsumer, ESPMode::ThreadSafe> InConsumer)
{
	FScopeLock Lock(&InSource.ObserverLock);
	if (Source.Load() || !InSource.IsCurrent()) return false;
	Consumer = MoveTemp(InConsumer);
	Source.Store(&InSource);
	Next = InSource.First;
	if (Next) Next->Previous = this;
	InSource.First = this;
	return true;
}

void FVoxelNavigationInvalidationObserver::Detach()
{
	// 所有者保留 Surface/Capture 到本节点析构之后，Source 的锁仍然存活。
	auto* Current = Source.Load();
	if (!Current) return;
	FScopeLock Lock(&Current->ObserverLock);
	if (Source.Load() == Current) Current->RemoveLocked(*this);
}

void FVoxelNavigationInvalidationSource::RemoveLocked(FVoxelNavigationInvalidationObserver& InObserver)
{
	if (InObserver.Previous) InObserver.Previous->Next = InObserver.Next;
	else First = InObserver.Next;
	if (InObserver.Next) InObserver.Next->Previous = InObserver.Previous;
	InObserver.Previous = nullptr;
	InObserver.Next = nullptr;
	// 最后公布脱链；并发析构看到nullptr后就允许释放节点本身。
	InObserver.Source.Store(nullptr);
}

void FVoxelNavigationInvalidationSource::Publish()
{
	uint8 Prepared = 0;
	State.CompareExchange(Prepared, 1);
}

void FVoxelNavigationInvalidationSource::Revoke()
{
	State.Store(2);
	for (;;)
	{
		TSharedPtr<IVoxelNavigationInvalidationConsumer, ESPMode::ThreadSafe> Pinned;
		{
			FScopeLock Lock(&ObserverLock);
			if (!First) break;
			Pinned = First->Consumer.Pin();
			RemoveLocked(*First);
		}
		// 监听边已脱链，回调和再查询不持有 Source 的链表锁。
		if (Pinned) Pinned->OnNavigationInputRevoked();
	}
}

bool FVoxelNavigationInvalidationSource::IsCurrent() const
{
	return State.Load() == 1;
}

bool FVoxelNavigationSectionConsumers::TryAcquire()
{
	if (!bAdmissionOpen.Load() || !bWorldOpen.Load()) return false;
	const int32 Previous = Readers++;
	check(Previous >= 0 && Previous < MAX_int32);
	// 关闭先于 GT 检查计数；关闭与第一次检查之间的竞争不会产生已获准的读取者。
	if (!bAdmissionOpen.Load() || !bWorldOpen.Load())
	{
		Release();
		return false;
	}
	return true;
}

void FVoxelNavigationSectionConsumers::Release()
{
	const int32 Previous = Readers--;
	check(Previous > 0);
}

bool FVoxelNavigationSectionConsumers::TryAcquireContinuation()
{
	if (!bWorldOpen.Load()) return false;
	const int32 Previous = Readers++;
	check(Previous >= 0 && Previous < MAX_int32);
	if (Previous == 0 || !bWorldOpen.Load())
	{
		Release();
		return false;
	}
	return true;
}

void FVoxelNavigationSectionConsumers::SetAdmissionOpen(const bool bInOpen)
{
	bAdmissionOpen.Store(bInOpen);
}

void FVoxelNavigationSectionConsumers::CloseWorld()
{
	bWorldOpen.Store(false);
	bAdmissionOpen.Store(false);
}

int32 FVoxelNavigationSectionConsumers::Num() const
{
	return Readers.Load();
}

bool FVoxelNavigationSectionConsumers::IsWorldOpen() const
{
	return bWorldOpen.Load();
}

FVoxelNavigationSurfaceAccess::FVoxelNavigationSurfaceAccess(
	TSharedRef<FVoxelNavigationSectionConsumers, ESPMode::ThreadSafe> InConsumers)
	: Consumers(MoveTemp(InConsumers))
{
}

bool FVoxelNavigationSurfaceAccess::TryAcquire()
{
	if (!IsCurrent() || !Consumers->TryAcquire()) return false;
	if (!IsCurrent())
	{
		Consumers->Release();
		return false;
	}
	return true;
}

void FVoxelNavigationSurfaceAccess::Release()
{
	Consumers->Release();
}

bool FVoxelNavigationSurfaceAccess::TryAcquireContinuation(const FVoxelNavigationSurfaceAccess& InPrevious)
{
	if (Consumers != InPrevious.Consumers || !IsCurrent() || !Consumers->TryAcquireContinuation()) return false;
	if (!IsCurrent())
	{
		Consumers->Release();
		return false;
	}
	return true;
}

void FVoxelNavigationSurfaceAccess::Publish()
{
	Invalidation.Publish();
}

void FVoxelNavigationSurfaceAccess::Revoke()
{
	Invalidation.Revoke();
}

bool FVoxelNavigationSurfaceAccess::IsCurrent() const
{
	return Invalidation.IsCurrent() && Consumers->IsWorldOpen();
}

bool FVoxelNavigationSurfaceAccess::Observe(FVoxelNavigationInvalidationObserver& InObserver,
	TWeakPtr<IVoxelNavigationInvalidationConsumer, ESPMode::ThreadSafe> InConsumer)
{
	return IsCurrent() && InObserver.Attach(Invalidation, MoveTemp(InConsumer));
}

FVoxelNavigationSurfaceReadLease::FVoxelNavigationSurfaceReadLease()
{
}

FVoxelNavigationSurfaceReadLease::~FVoxelNavigationSurfaceReadLease()
{
	Reset();
}

FVoxelNavigationSurfaceReadLease::FVoxelNavigationSurfaceReadLease(FVoxelNavigationSurfaceReadLease&& InOther)
	: Surface(MoveTemp(InOther.Surface))
{
}

FVoxelNavigationSurfaceReadLease& FVoxelNavigationSurfaceReadLease::operator=(FVoxelNavigationSurfaceReadLease&& InOther)
{
	if (this != &InOther)
	{
		Reset();
		Surface = MoveTemp(InOther.Surface);
	}
	return *this;
}

bool FVoxelNavigationSurfaceReadLease::Acquire(TSharedPtr<const FVoxelNavigationSurface, ESPMode::ThreadSafe> InSurface)
{
	if (Surface || !InSurface || !InSurface->Access || !InSurface->Access->TryAcquire()) return false;
	Surface = MoveTemp(InSurface);
	return true;
}

bool FVoxelNavigationSurfaceReadLease::IsCurrent() const
{
	return Surface && Surface->Access && Surface->Access->IsCurrent();
}

bool FVoxelNavigationSurfaceReadLease::AcquireContinuation(
	TSharedPtr<const FVoxelNavigationSurface, ESPMode::ThreadSafe> InSurface,
	const FVoxelNavigationSurfaceReadLease& InPrevious)
{
	if (Surface || !InPrevious.Surface || !InPrevious.Surface->Access || !InSurface || !InSurface->Access ||
		InPrevious.Surface->InputVersion.WorldEpoch != InSurface->InputVersion.WorldEpoch ||
		InPrevious.Surface->InputVersion.Section != InSurface->InputVersion.Section ||
		InPrevious.Surface->InputVersion.SectionIncarnation != InSurface->InputVersion.SectionIncarnation ||
		!InSurface->Access->TryAcquireContinuation(*InPrevious.Surface->Access)) return false;
	Surface = MoveTemp(InSurface);
	return true;
}

const FVoxelNavigationSurface* FVoxelNavigationSurfaceReadLease::Get() const
{
	return Surface.Get();
}

void FVoxelNavigationSurfaceReadLease::Reset()
{
	if (!Surface) return;
	Surface->Access->Release();
	Surface.Reset();
}

bool FVoxelNavigationInputVersion::MatchesPhysicsReceipt(const FVoxelPhysicsReceipt& InReceipt) const
{
	return WorldEpoch != 0 && SectionIncarnation != 0 && SectionToken != 0 && PhysicsSceneEpoch.IsValid() &&
		PhysicsOwnerId.IsValid() && CollisionBodyGeneration != 0 && InReceipt.PhysicsStep != INDEX_NONE &&
		WorldEpoch == InReceipt.WorldEpoch && Section == InReceipt.Section &&
		SectionIncarnation == InReceipt.SectionIncarnation && SectionToken == InReceipt.SectionToken &&
		SectionRevision == InReceipt.DataRevision && LocalGeometryRevision == InReceipt.ContentRevision &&
		RegistryRevision == InReceipt.RegistryRevision && ShapeRevision == InReceipt.ShapeRevision &&
		PhysicsSceneEpoch == InReceipt.SceneEpoch && PhysicsOwnerId == InReceipt.PresenterOwnerId &&
		CollisionBodyGeneration == InReceipt.BodyGeneration;
}

bool FVoxelNavigationSurfaceBuilder::Build(const FVoxelSectionSnapshot& InSection, const FVoxelSectionSnapshot* InAbove,
	const FVoxelRegistrySnapshot& InRegistry, const FVoxelShapeRegistry& InShapes,
	double InCellSize, double InClearance, FVoxelNavigationSurface& OutSurface, const TAtomic<bool>* InCancel,
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
	const FVoxelGenerationConsumerKey& InConsumer, FString* OutError, const FVoxelSectionSnapshot* InSecondAbove)
{
	if (OutError) OutError->Reset();
	if (!FMath::IsFinite(InCellSize) || !FMath::IsFinite(InClearance) || InCellSize <= 0.0 ||
		InClearance <= 0.0 || InClearance / InCellSize > 32.0 || InSection.GridSide != 16 ||
		(InAbove && InAbove->GridSide != 16) || (InSecondAbove && InSecondAbove->GridSide != 16))
	{
		return false;
	}
	FVoxelNavigationSurface Result;
	Result.MetadataLease = OutSurface.MetadataLease;
	const int32 ClearanceCells = FMath::CeilToInt(InClearance / InCellSize);
	auto Read = [&](const FIntVector& InLocal, FVoxelBlockState& OutState)
	{
		if (InLocal.Z >= 32)
		{
			return InSecondAbove && InLocal.Z < 48 && InSecondAbove->TrySample(InLocal - FIntVector(0, 0, 32), OutState);
		}
		if (InLocal.Z >= 16)
		{
			return InAbove && InLocal.Z < 32 && InAbove->TrySample(InLocal - FIntVector(0, 0, 16), OutState);
		}
		return InLocal.Z >= 0 && InSection.TrySample(InLocal, OutState);
	};
	const auto VisitFaces = [&](auto&& InEmit)
	{
		for (int32 Height = 0; Height < 16; ++Height)
		{
			if (InCancel && InCancel->Load())
			{
				return false;
			}
			for (int32 Row = 0; Row < 16; ++Row)
			{
				for (int32 Column = 0; Column < 16; ++Column)
				{
					const FIntVector Cell(Column, Row, Height);
					FVoxelBlockState State;
					if (!Read(Cell, State))
					{
						continue;
					}
					const FVoxelRuntimeDefinition* Block = InRegistry.Find(State.TypeId);
					if (!Block || !Block->bSolid || Block->Shape == EVoxelShapeKind::Fluid || Block->Shape == EVoxelShapeKind::Door)
					{
						continue;
					}
					bool bClear = true;
					for (int32 Above = 1; Above <= ClearanceCells; ++Above)
					{
						FVoxelBlockState Overhead;
						if (!Read(Cell + FIntVector(0, 0, Above), Overhead))
						{
							bClear = false;
							break;
						}
						const FVoxelRuntimeDefinition* OverheadBlock = InRegistry.Find(Overhead.TypeId);
						if (!OverheadBlock || OverheadBlock->Shape == EVoxelShapeKind::Fluid ||
							(OverheadBlock->bSolid && !InShapes.Get(OverheadBlock->Shape, Overhead.State).CollisionBoxes.IsEmpty()))
						{
							bClear = false;
							break;
						}
					}
					if (!bClear)
					{
						continue;
					}
					for (const FVoxelShapeQuad& Quad : InShapes.Get(Block->Shape, State.State).Quads)
					{
						if (Quad.Face == 4 && !InEmit(Cell, Quad))
						{
							return false;
						}
					}
				}
			}
		}
		return true;
	};
	int32 QuadCount = 0;
	if (!VisitFaces([&](const FIntVector&, const FVoxelShapeQuad&)
	{
		if (QuadCount >= MAX_int32 / 6) return false;
		++QuadCount;
		return true;
	}))
	{
		if (OutError) *OutError = InCancel && InCancel->Load() ? TEXT("Canceled") : TEXT("Oversized: navigation face count");
		return false;
	}
	const uint64 Bytes = VoxelBoundedAllocation::ArrayBytes<FVector>(QuadCount * 4) +
		VoxelBoundedAllocation::ArrayBytes<int32>(QuadCount * 6);
	if (InResources && !InResources->Acquire(InConsumer, EVoxelTerrainResourcePool::Staged,
		FMath::Max<uint64>(1, Bytes), Result.ResourceLease))
	{
		if (OutError) *OutError = Bytes > InResources->Snapshot().Limits.StagedBytes ?
			TEXT("Oversized: navigation output") : TEXT("CapacityBlocked: navigation output");
		return false;
	}
	Result.Vertices.Reserve(QuadCount * 4);
	Result.Indices.Reserve(QuadCount * 6);
	if (!VisitFaces([&](const FIntVector& Cell, const FVoxelShapeQuad& Quad)
	{
		const int32 FirstVertex = Result.Vertices.Num();
		if (FirstVertex > QuadCount * 4 - 4 || Result.Indices.Num() > QuadCount * 6 - 6) return false;
		for (const FVector& Vertex : Quad.Vertices)
		{
			const FVector Position = (FVector(Cell) + Vertex) * InCellSize;
			Result.Vertices.Add(Position);
			Result.Bounds += Position;
		}
		const int32 Indices[] = {FirstVertex, FirstVertex + 2, FirstVertex + 1, FirstVertex, FirstVertex + 3, FirstVertex + 2};
		Result.Indices.Append(Indices, UE_ARRAY_COUNT(Indices));
		return true;
	}) || Result.Vertices.Num() != QuadCount * 4 || Result.Indices.Num() != QuadCount * 6)
	{
		if (OutError) *OutError = InCancel && InCancel->Load() ? TEXT("Canceled") : TEXT("EstimateMismatch: navigation Count/Build");
		return false;
	}
	check(Result.Vertices.GetAllocatedSize() + Result.Indices.GetAllocatedSize() <= Bytes);
	OutSurface = MoveTemp(Result);
	return true;
}
