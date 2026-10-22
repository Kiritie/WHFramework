#include "DWHeightfieldTransitionBuilder.h"
#include "Voxel/Runtime/VoxelChangeHierarchy.h"

namespace
{
	// 仅栈上记录当前强引用链，允许共享前驱并阻止环；最终容量由原 serial union 去重。
	struct FVoxelRendererMeshSourcePath
	{
		const void* Object;
		const FVoxelRendererMeshSourcePath* Previous;
		static bool Contains(const FVoxelRendererMeshSourcePath* Path, const void* Object)
		{
			for (; Path; Path = Path->Previous) if (Path->Object == Object) return true;
			return false;
		}
	};

	struct FVoxelRendererMeshSourceVisitor
	{
		const FVoxelTerrainResources& Resources;
		FVoxelGenerationRequiredSourceScope::FVisit Visit;

		void Lease(const TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& InLease) const
		{
			if (InLease && InLease->IsOwnedBy(Resources)) FVoxelGenerationRequiredSourceScope::VisitLease(InLease.Get(), Visit);
		}

		void Fine(const FVoxelFineShardGeometry* Geometry, const FVoxelRendererMeshSourcePath* Path) const
		{
			if (!Geometry || FVoxelRendererMeshSourcePath::Contains(Path, Geometry)) return;
			const FVoxelRendererMeshSourcePath Current{Geometry, Path};
			Lease(Geometry->MetadataLease);
			FVoxelChangeHierarchy::VisitFineRegistrationSources(Geometry->RegistrationLease, Resources, Visit);
			if (Geometry->OverlayVersion) Lease(Geometry->OverlayVersion->ResourceLease);
			if (Geometry->Boundary) Lease(Geometry->Boundary->ResourceLease);
			for (const auto& Piece : Geometry->Pieces) if (Piece.Mesh) Mesh(*Piece.Mesh, &Current);
		}

		void FineBoundary(const FVoxelFineBoundarySource& Source) const
		{
			if (Source.Boundary) Lease(Source.Boundary->ResourceLease);
			if (Source.OverlayVersion) Lease(Source.OverlayVersion->ResourceLease);
		}

		void Coarse(const FVoxelTerrainCoarseMesh* Geometry, const FVoxelRendererMeshSourcePath* Path) const
		{
			if (!Geometry || FVoxelRendererMeshSourcePath::Contains(Path, Geometry)) return;
			const FVoxelRendererMeshSourcePath Current{Geometry, Path};
			Lease(Geometry->MetadataLease);
			if (Geometry->Source) Mesh(*Geometry->Source, &Current);
		}

		void Mesh(const FVoxelSectionMeshResult& InMesh, const FVoxelRendererMeshSourcePath* Path) const
		{
			if (FVoxelRendererMeshSourcePath::Contains(Path, &InMesh)) return;
			const FVoxelRendererMeshSourcePath Current{&InMesh, Path};
			Lease(InMesh.ResourceLease);
			if (InMesh.FrozenVoxelProxy) Lease(InMesh.FrozenVoxelProxy->ResourceLease);
			if (InMesh.FrozenSurface) Lease(InMesh.FrozenSurface->ResourceLease);
			if (InMesh.FrozenMacro) Lease(InMesh.FrozenMacro->ResourceLease);
			if (InMesh.FrozenWater) Lease(InMesh.FrozenWater->ResourceLease);
			if (const auto* Boundary = InMesh.FrozenBoundaryInputs.Get())
			{
				Lease(Boundary->ResourceLease);
				if (Boundary->OwnerGeometry) Lease(Boundary->OwnerGeometry->ResourceLease);
				FineBoundary(Boundary->FineOwnerSource);
				for (const auto& Patch : Boundary->Patches)
				{
					if (Patch.Neighbor.ProxyGeometry) Lease(Patch.Neighbor.ProxyGeometry->ResourceLease);
					FineBoundary(Patch.Neighbor.FineSource);
				}
				for (const auto& Source : Boundary->SuppressionSources) if (Source) Lease(Source->ResourceLease);
			}
			if (const auto* Heightfield = InMesh.FrozenHeightfieldInputs.Get())
			{
				Lease(Heightfield->ResourceLease);
				for (const auto& Source : Heightfield->Sources)
				{
					if (Source.Surface) Lease(Source.Surface->ResourceLease);
					if (Source.Macro) Lease(Source.Macro->ResourceLease);
				}
			}
			if (InMesh.FrozenNativeInterfaceInputs)
			{
				const auto Inputs = StaticCastSharedPtr<const FNativeHeightfieldInputs>(InMesh.FrozenNativeInterfaceInputs);
				Lease(Inputs->Lease);
				FineBoundary(Inputs->FineSource);
				if (Inputs->Surface) Lease(Inputs->Surface->ResourceLease);
				if (Inputs->Macro) Lease(Inputs->Macro->ResourceLease);
				if (Inputs->Water) Lease(Inputs->Water->ResourceLease);
				if (Inputs->DistantSlice)
				{
					Lease(Inputs->DistantSlice->Lease);
					if (Inputs->DistantSlice->Surface) Lease(Inputs->DistantSlice->Surface->ResourceLease);
					if (Inputs->DistantSlice->Macro) Lease(Inputs->DistantSlice->Macro->ResourceLease);
				}
			}
		}
	};
	struct FVoxelRendererCoarseSources
	{
		const FVoxelTerrainRenderer& Renderer;
		const FVoxelTerrainResources& Resources;
		TConstArrayView<FVoxelTerrainCoarseMutation> Changes;
		const FVoxelTerrainResourceLease* CallbackLease = nullptr;

		static void Enumerate(const void* InContext, FVoxelGenerationRequiredSourceScope::FVisit InVisit)
		{
			const auto& Context = *static_cast<const FVoxelRendererCoarseSources*>(InContext);
			Context.Renderer.VisitNativeControlSources(InVisit);
			FVoxelGenerationRequiredSourceScope::VisitLease(Context.CallbackLease, InVisit);
			for (const auto& Change : Context.Changes)
				if (Change.Item) FVoxelTerrainRenderer::VisitCoarseSources(*Change.Item, Context.Resources, InVisit);
		}
	};
}

void FVoxelTerrainRenderer::VisitMeshSources(const FVoxelSectionMeshResult& InMesh,
	const FVoxelTerrainResources& InResources, FVoxelGenerationRequiredSourceScope::FVisit InVisit)
{
	FVoxelRendererMeshSourceVisitor{InResources, InVisit}.Mesh(InMesh, nullptr);
}

void FVoxelTerrainRenderer::VisitFineSources(const FVoxelFineShardGeometry& InGeometry,
	const FVoxelTerrainResources& InResources, FVoxelGenerationRequiredSourceScope::FVisit InVisit)
{
	FVoxelRendererMeshSourceVisitor{InResources, InVisit}.Fine(&InGeometry, nullptr);
}

void FVoxelTerrainRenderer::VisitCoarseSources(const FVoxelTerrainRenderCoarse& InCoarse,
	const FVoxelTerrainResources& InResources, FVoxelGenerationRequiredSourceScope::FVisit InVisit)
{
	const auto Visit = [&](const TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& Lease)
	{
		if (Lease && Lease->IsOwnedBy(InResources)) FVoxelGenerationRequiredSourceScope::VisitLease(Lease.Get(), InVisit);
	};
	const auto VisitPiece = [&](const FVoxelTerrainCoarsePatchPiece& Piece)
	{
		Visit(Piece.MetadataLease);
		if (Piece.Mesh) FVoxelTerrainRenderer::VisitMeshSources(*Piece.Mesh, InResources, InVisit);
	};
	Visit(InCoarse.MetadataLease);
	if (InCoarse.Geometry)
	{
		Visit(InCoarse.Geometry->MetadataLease);
		if (InCoarse.Geometry->Source) FVoxelTerrainRenderer::VisitMeshSources(*InCoarse.Geometry->Source, InResources, InVisit);
	}
	const auto VisitChunks = [&](TSharedPtr<const FVoxelTerrainRenderChunk, ESPMode::ThreadSafe> Chunk)
	{
		for (; Chunk; Chunk = Chunk->NextSourceChunk)
		{
			Visit(Chunk->ResidentLease);
			if (Chunk->Source) FVoxelTerrainRenderer::VisitMeshSources(*Chunk->Source, InResources, InVisit);
		}
	};
	for (const auto& Chunk : InCoarse.SourceChunks) VisitChunks(Chunk);
	const auto VisitPatch = [&](auto&& Self, const FVoxelTerrainCoarsePatchNode* Node) -> void
	{
		if (!Node) return;
		Visit(Node->MetadataLease);
		for (auto Piece = Node->FirstPiece; Piece; Piece = Piece->Next) VisitPiece(*Piece);
		for (const auto& Child : Node->Children) Self(Self, Child.Get());
	};
	if (InCoarse.Patch)
	{
		Visit(InCoarse.Patch->MetadataLease);
		for (const auto& Root : InCoarse.Patch->Roots) VisitPatch(VisitPatch, Root.Get());
	}
	const auto VisitDraw = [&](auto&& Self, const FVoxelTerrainCoarseDrawNode* Node) -> void
	{
		if (!Node) return;
		Visit(Node->Lease);
		// Draw 可复用旧 patch；即使本 Item.Patch 已换代，该直接强引用仍属于当前来源。
		VisitPatch(VisitPatch, Node->Patch.Get());
		for (auto Piece = Node->FirstPiece; Piece; Piece = Piece->Next)
		{
			Visit(Piece->Lease);
			if (Piece->Piece) VisitPiece(*Piece->Piece);
			VisitChunks(Piece->Chunk);
		}
		for (const auto& Child : Node->Children) Self(Self, Child.Get());
	};
	for (const auto& Root : InCoarse.DrawRoots) VisitDraw(VisitDraw, Root.Get());
}

uint64 FVoxelTerrainRenderer::GetCoarseAdmissionVersion() const
{
	return State ? State->NativeFrontendRevision : 0;
}

void FVoxelTerrainRenderer::VisitNativeControlSources(FVoxelGenerationRequiredSourceScope::FVisit InVisit) const
{
	if (!State) return;
	const auto Visit = [&](const TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& Lease)
	{
		if (Lease && Lease->IsOwnedBy(State->Resources.Get())) FVoxelGenerationRequiredSourceScope::VisitLease(Lease.Get(), InVisit);
	};
	Visit(State->InputLease);
	Visit(State->MetadataLease);
	if (auto* Component = State->Component.Get()) Visit(Component->GetNativeMaterialLease());
}

bool FVoxelTerrainRenderer::PrepareCoarseTransaction(const FCoarsePtr InCoarse, const bool bInRemove,
	TFunction<bool()> InIsCurrent, TFunction<void()> InOnSubmitted, FGuid& OutTransaction,
	const FVoxelTerrainServicePriority& InPriority, const double InFirstQueuedSeconds,
	EVoxelTerrainResourceAdmission* OutAdmission, const FVoxelGenerationConsumerKey& InConsumer,
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InCallbackLease)
{
	const FVoxelTerrainCoarseMutation Change{InCoarse, bInRemove};
	return PrepareCoarseTransaction(MakeArrayView(&Change, 1), MoveTemp(InIsCurrent), MoveTemp(InOnSubmitted),
		OutTransaction, InPriority, InFirstQueuedSeconds, OutAdmission, InConsumer, MoveTemp(InCallbackLease));
}

bool FVoxelTerrainRenderer::PrepareCoarseTransaction(const TConstArrayView<FVoxelTerrainCoarseMutation> InChanges,
	TFunction<bool()> InIsCurrent, TFunction<void()> InOnSubmitted, FGuid& OutTransaction,
	const FVoxelTerrainServicePriority& InPriority, const double InFirstQueuedSeconds,
	EVoxelTerrainResourceAdmission* OutAdmission, const FVoxelGenerationConsumerKey& InConsumer,
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InCallbackLease)
{
	if (OutAdmission) *OutAdmission = EVoxelTerrainResourceAdmission::Granted;
	const auto Reject = [&](const EVoxelTerrainResourceAdmission Admission)
	{
		if (OutAdmission) *OutAdmission = Admission;
		return false;
	};
	if (!EnsureState()) return Reject(GetNativeStateAdmission());
	if (!State->bActive.Load() || InChanges.IsEmpty() || !InIsCurrent) return Reject(EVoxelTerrainResourceAdmission::InvalidRequest);
	if (InChanges.Num() > 8192 || State->NextMetadataSerial == MAX_uint64 || State->NativeFrontendRevision == MAX_uint64)
		return Reject(EVoxelTerrainResourceAdmission::CapacityUnachievable);
	if (InConsumer.IsValid() && InConsumer.WorldEpoch != State->Epoch) return Reject(EVoxelTerrainResourceAdmission::InvalidRequest);
	if (InCallbackLease && (!InCallbackLease->IsOwnedBy(State->Resources.Get()) || InCallbackLease->GetPool() != EVoxelTerrainResourcePool::Input))
		return Reject(EVoxelTerrainResourceAdmission::InvalidRequest);
	if (!State->PrepareMetadata()) return Reject(State->MetadataAdmission);
	const int32 QueueLimit = FVoxelTerrainWorkSelection::IsNear(InPriority.Band) ? VoxelNativePublicationCapacity :
		VoxelNativePublicationCapacity - VoxelNativeNearPublicationReserve;
	if (State->Ready.Num() + State->SubmittedCallbacks.Num() >= QueueLimit || State->PendingCoarseSources.Num() + InChanges.Num() > 32768)
		return Reject(EVoxelTerrainResourceAdmission::CapacityBlocked);
	TArray<FGuid, TInlineAllocator<8192>> Sources;
	for (const auto& Change : InChanges)
	{
		const auto& Item = Change.Item;
		if (!Item || !Item->Geometry || !Item->SourceId.IsValid() || (!Change.bRemove && !Item->IsResourceReady()) ||
			Sources.Contains(Item->SourceId)) return Reject(EVoxelTerrainResourceAdmission::InvalidRequest);
		if (State->PendingCoarseSources.Contains(Item->SourceId)) return Reject(EVoxelTerrainResourceAdmission::CapacityBlocked);
		if (Change.bRemove && (!State->LastSealedSnapshot || !SameCoarseGeometry(
			FindCoarseItem(State->LastSealedSnapshot->CoarseRoot, Item->Geometry->Stamp.GeometryIdentity.Key, Item->SourceId), Item)))
			return Reject(EVoxelTerrainResourceAdmission::InvalidRequest);
		Sources.Add(Item->SourceId);
	}
	const auto Consumer = InConsumer.IsValid() ? InConsumer : FVoxelGenerationConsumerKey::Section(State->Epoch, State->OwnerId, 1,
		InChanges[0].Item->Geometry->Stamp.GeometryIdentity.Key.Coordinate, State->NextMetadataSerial + 1);
	const FVoxelRendererCoarseSources SourceContext{*this, State->Resources.Get(), InChanges, InCallbackLease.Get()};
	const FVoxelGenerationRequiredSourceScope SourceScope(&State->Resources.Get(), &SourceContext, &FVoxelRendererCoarseSources::Enumerate);
	FVoxelGenerationRequiredCapacity Required;
	FString Error;
	if (!FVoxelGenerationRequiredSourceScope::Collect(State->Resources.Get(), nullptr, Required, Error))
		return Reject(EVoxelTerrainResourceAdmission::CapacityUnachievable);
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	const uint64 Bytes = VoxelBoundedAllocation::WorkerSharedBytes<FAtomPublication>() +
		VoxelBoundedAllocation::ArrayBytes<FVoxelTerrainCoarseMutation>(InChanges.Num()) +
		VoxelBoundedAllocation::WorkerSharedBytes<FPresentedSnapshot>();
	const bool bAllowOverlap = State->PresentedSnapshot && State->LastSealedSnapshot;
	const auto Admission = TryAcquirePublicationResident(State->Resources, Consumer, Bytes, Lease, 1,
		bAllowOverlap, Required.Bytes[static_cast<int32>(EVoxelTerrainResourcePool::Input)], Required.LeaseCount);
	if (Admission != EVoxelTerrainResourceAdmission::Granted) return Reject(Admission);
	auto Package = VoxelBoundedAllocation::MakeWorkerShared<FAtomPublication>(Lease);
	Package->Lease = MoveTemp(Lease);
	Package->CoarseCallbackLease = MoveTemp(InCallbackLease);
	Package->CoarseChanges.Reserve(InChanges.Num());
	Package->CoarseChanges.Append(InChanges.GetData(), InChanges.Num());
	Package->Transaction = FGuid::NewGuid();
	FReadyTransaction Ready;
	Ready.Id = Package->Transaction;
	Ready.Priority = InPriority;
	if (!FMath::IsFinite(Ready.Priority.DistanceCells) || Ready.Priority.DistanceCells == MAX_dbl) Ready.Priority.DistanceCells = 0.0;
	Ready.FirstQueuedSeconds = InFirstQueuedSeconds;
	Ready.Sequence = ++State->NextMetadataSerial;
	Ready.Resources = MoveTemp(Package);
	Ready.IsCoarseCurrent = MoveTemp(InIsCurrent);
	Ready.OnSubmitted = MoveTemp(InOnSubmitted);
	OutTransaction = Ready.Id;
	for (const auto& Change : InChanges) State->PendingCoarseSources.Add(Change.Item->SourceId);
	State->InsertReady(MoveTemp(Ready));
	return true;
}
