// 仅由 VoxelTerrainRenderer.cpp 引入；所有输入来自同一冻结的呈现根。
FAtomPtr FindFineAtom(const FAtomNodePtr& Root, const FIntVector& Section)
{
	const FAtomTreeNode* Node = Root.Get();
	for (int32 Bit = 31; Bit >= 0 && Node; --Bit) Node = Node->Children[Branch(Section, Bit)].Get();
	return Node ? Node->Atom : nullptr;
}

FCoarsePtr FindCoarseOwner(const FCoarseNodePtr& Root, const FIntVector& Section,
	const FAtomPublication* Publication = nullptr)
{
	const FVector Min = FVector(Section) * 16.0;
	const FBox Box(Min, Min + FVector(16.0));
	FCoarsePtr Best;
	const auto Consider = [&Best, &Box](const FCoarsePtr& Item)
	{
		if (Item && !Item->bBoundaryOnly && Item->OwnershipBounds.IsInsideOrOn(Box.Min) &&
			Item->OwnershipBounds.IsInsideOrOn(Box.Max) && (!Best ||
			FinerCoarse(Item->Geometry->Stamp.GeometryIdentity, Best->Geometry->Stamp.GeometryIdentity))) Best = Item;
	};
	const auto Visit = [&](auto&& Self, const FCoarseNodePtr& Node) -> void
	{
		if (!Node || !Node->CellBounds.Intersect(Box)) return;
		if (Node->Item)
		{
			if (!Publication || !Publication->CoarseChanges.ContainsByPredicate([&Node](const auto& Change)
				{ return Change.Item->SourceId == Node->Item->SourceId; })) Consider(Node->Item);
		}
		else for (const auto& Child : Node->Children) Self(Self, Child);
	};
	Visit(Visit, Root);
	if (Publication) for (const auto& Change : Publication->CoarseChanges) if (!Change.bRemove) Consider(Change.Item);
	return Best;
}

FAtomPtr FineAfterPublication(const FAtomNodePtr& Root, const FIntVector& Section, const FAtomPublication& Publication)
{
	for (const auto& Atom : Publication.Atoms)
		if (Atom->Section == Section) return Atom->Evidence.Owner.Kind == EVoxelFineOwnerKind::None ? nullptr : Atom;
	return FindFineAtom(Root, Section);
}

bool SameFineInput(const FAtomPtr& A, const FAtomPtr& B)
{
	return A == B || (A && B && A->Section == B->Section && A->Evidence.Owner == B->Evidence.Owner &&
		A->Evidence.Content == B->Evidence.Content && A->FineGeometry == B->FineGeometry);
}

struct FNativeInterfacePiece
{
	TSharedPtr<const FVoxelSectionMeshResult, ESPMode::ThreadSafe> Mesh;
	FVector Origin = FVector::ZeroVector;
	double Scale = 1.0;
	TSharedPtr<FNativeInterfacePiece, ESPMode::ThreadSafe> Next;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
};

struct FPreparedFine
{
	FPreparedFine(TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
		const FVoxelGenerationConsumerKey& InConsumer) : Resources(MoveTemp(InResources)), Consumer(InConsumer) {}

	bool Initialize(const FAtomPtr& InAtom, const FAtomNodePtr& FineRoot, const FCoarseNodePtr& CoarseRoot,
		const double InCellSize, FString& OutError)
	{
		Base = InAtom;
		CellSize = InCellSize;
		if (!Base || !Base->HasMatchingFineInputs())
		{ OutError = TEXT("DependencyNotReady:NativeFineBoundary"); return false; }
		for (uint8 Face = 0; Face < 6; ++Face)
		{
			const FIntVector Section = Base->Section + VoxelCoord::Direction(Face);
			NeighborFine[Face] = FindFineAtom(FineRoot, Section);
			if (NeighborFine[Face] && !NeighborFine[Face]->HasMatchingFineInputs())
			{ OutError = TEXT("DependencyNotReady:NativeNeighborBoundary"); return false; }
			if (!NeighborFine[Face]) NeighborCoarse[Face] = FindCoarseOwner(CoarseRoot, Section);
			const auto Source = NeighborCoarse[Face] ? NeighborCoarse[Face]->Geometry->Source : nullptr;
			bSuppress[Face] = NeighborFine[Face] || (Source &&
				(Source->FrozenVoxelProxy || Source->FrozenSurface || Source->FrozenMacro));
		}
		return true;
	}

	bool InputsMatch(const FSnapshotPtr& Latest, const FAtomPublication& Publication) const
	{
		if (!SameFineInput(Base, FineAfterPublication(Latest ? Latest->Root : nullptr, Base->Section, Publication))) return false;
		for (uint8 Face = 0; Face < 6; ++Face)
		{
			const FIntVector Section = Base->Section + VoxelCoord::Direction(Face);
			const auto Fine = FineAfterPublication(Latest ? Latest->Root : nullptr, Section, Publication);
			if (!SameFineInput(Fine, NeighborFine[Face])) return false;
			if (Fine) continue;
			const auto Coarse = FindCoarseOwner(Latest ? Latest->CoarseRoot : nullptr, Section, &Publication);
			if (bool(Coarse) != bool(NeighborCoarse[Face]) || (Coarse &&
				!SameCoarseGeometry(Coarse, NeighborCoarse[Face]))) return false;
		}
		return true;
	}

	bool KeepTriangle(const FVoxelTerrainRenderChunk& Chunk, const int32 Triangle) const
	{
		const auto& Mesh = Chunk.Source->Batches[Chunk.SourceBatch].Mesh;
		const FVector Origin = Chunk.LocalToWorld.GetOrigin() / CellSize;
		const FVector Min = FVector(Base->Section) * 16.0;
		for (uint8 Face = 0; Face < 6; ++Face)
		{
			if (!bSuppress[Face]) continue;
			const int32 Axis = Face / 2;
			const double Plane = Min[Axis] + ((Face & 1) ? 0.0 : 16.0);
			bool bOnPlane = true;
			for (int32 Corner = 0; Corner < 3; ++Corner)
				bOnPlane &= FMath::IsNearlyEqual(Origin[Axis] + Mesh.Vertices[Mesh.Triangles[Triangle * 3 + Corner]][Axis], Plane);
			if (bOnPlane) return false;
		}
		return true;
	}

	void VisitRequiredSources(FVoxelGenerationRequiredSourceScope::FVisit InVisit) const
	{
		const auto VisitLease = [&](const auto& InLease)
		{
			if (InLease && InLease->IsOwnedBy(Resources.Get()))
				FVoxelGenerationRequiredSourceScope::VisitLease(InLease.Get(), InVisit);
		};
		VisitLease(Lease); VisitLease(PathLease); VisitLease(ControlInputLease); VisitLease(PreparationInputLease);
		VisitLease(PublicationLease); VisitLease(PublicationCallbackLease); VisitLease(PendingWorkingLease);
		const auto VisitAtom = [&](const FAtomPtr& Atom)
		{
			if (!Atom) return;
			VisitLease(Atom->MetadataLease);
			if (Atom->FineGeometry) FVoxelTerrainRenderer::VisitFineSources(*Atom->FineGeometry, Resources.Get(), InVisit);
			for (const auto& Boundary : Atom->Boundaries) if (Boundary) VisitLease(Boundary->ResourceLease);
			for (const auto& RootChunk : Atom->Chunks)
				for (auto Chunk = RootChunk; Chunk; Chunk = Chunk->NextSourceChunk)
				{
					VisitLease(Chunk->ResidentLease);
					if (Chunk->Source) FVoxelTerrainRenderer::VisitMeshSources(*Chunk->Source, Resources.Get(), InVisit);
				}
		};
		VisitAtom(Base);
		for (int32 Face = 0; Face < 6; ++Face)
		{
			VisitAtom(NeighborFine[Face]);
			if (NeighborCoarse[Face]) FVoxelTerrainRenderer::VisitCoarseSources(*NeighborCoarse[Face], Resources.Get(), InVisit);
		}
		if (DistantSlice) VisitLease(DistantSlice->Lease);
		for (auto Piece = FirstPiece; Piece; Piece = Piece->Next)
		{
			VisitLease(Piece->Lease);
			if (Piece->Mesh) FVoxelTerrainRenderer::VisitMeshSources(*Piece->Mesh, Resources.Get(), InVisit);
		}
	}

	bool MeasureRequired(const TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& ExtraLease,
		FVoxelGenerationRequiredCapacity& OutRequired, FString& OutError) const
	{
		const FVoxelGenerationRequiredSourceScope Sources(&Resources.Get(), this,
			[](const void* Context, FVoxelGenerationRequiredSourceScope::FVisit Visit)
			{ static_cast<const FPreparedFine*>(Context)->VisitRequiredSources(Visit); });
		const FVoxelGenerationRequiredSourceScope Extra(&Resources.Get(), ExtraLease.Get());
		return FVoxelGenerationRequiredSourceScope::Collect(Resources.Get(), nullptr, OutRequired, OutError);
	}

	bool AcquireNative(const EVoxelTerrainResourcePool Pool, const uint64 Bytes,
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& OutLease, FString& OutError,
		const TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& RequiredLease = nullptr)
	{
		FVoxelGenerationRequiredCapacity Required;
		if (!MeasureRequired(RequiredLease, Required, OutError)) return false;
		const auto Limits = Resources->Snapshot().Limits;
		const uint64 PoolLimits[] = {Limits.InputBytes, Limits.WorkingBytes, Limits.StagedBytes,
			Limits.ResidentBytes - Limits.OverlapBytes, Limits.OverlapBytes};
		if (Bytes > PoolLimits[int32(Pool)] || Required.Bytes[int32(Pool)] > PoolLimits[int32(Pool)] - Bytes)
		{ OutError = TEXT("CapacityUnachievable: Native Fine retained sources"); return false; }
		const auto Admission = Resources->TryAcquire(Consumer, Pool, Bytes, OutLease, 0,
			Required.Bytes[0], Required.LeaseCount);
		if (Admission == EVoxelTerrainResourceAdmission::Granted) return true;
		OutError = Admission == EVoxelTerrainResourceAdmission::CapacityBlocked ? TEXT("CapacityBlocked: Native Fine interface") :
			Admission == EVoxelTerrainResourceAdmission::CapacityUnachievable ? TEXT("CapacityUnachievable: Native Fine interface") :
			TEXT("InvalidRequest: Native Fine interface");
		OutError += FString::Printf(TEXT(" pool=%d bytes=%llu retainedPool=%llu retainedInput=%llu retainedLeases=%llu"),
			int32(Pool), Bytes, Required.Bytes[int32(Pool)], Required.Bytes[0], uint64(Required.LeaseCount));
		return false;
	}

	static EVoxelFineBuildProgress RejectedAdmission(const FString& Error)
	{
		return Error.StartsWith(TEXT("CapacityBlocked")) ? EVoxelFineBuildProgress::CapacityBlocked : EVoxelFineBuildProgress::Failed;
	}

	bool PrepareAtom(FString& OutError)
	{
		const uint64 Bytes = VoxelBoundedAllocation::WorkerSharedBytes<FVoxelTerrainRenderAtom>() +
			VoxelBoundedAllocation::ArrayBytes<TSharedPtr<const FVoxelTerrainRenderChunk, ESPMode::ThreadSafe>>(Base->Chunks.Num()) +
			VoxelBoundedAllocation::ArrayBytes<TSharedPtr<const FVoxelFineBoundarySnapshot, ESPMode::ThreadSafe>>(Base->Boundaries.Num()) +
			VoxelBoundedAllocation::ArrayBytes<FVoxelTerrainAtomDrawRange>(RangeCount);
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> AtomLease;
		if (!AcquireNative(EVoxelTerrainResourcePool::Staged, Bytes, AtomLease, OutError)) return false;
		FVoxelGenerationRequiredCapacity Required;
		if (!MeasureRequired(AtomLease, Required, OutError)) return false;
		const auto PathAdmission = TryAcquirePublicationResident(Resources, Consumer,
			33 * VoxelBoundedAllocation::WorkerSharedBytes<FAtomTreeNode>(), PathLease, 33, bAllowOverlap,
			Required.Bytes[0], Required.LeaseCount);
		if (PathAdmission != EVoxelTerrainResourceAdmission::Granted)
		{
			OutError = PathAdmission == EVoxelTerrainResourceAdmission::CapacityBlocked ? TEXT("CapacityBlocked: Native Fine path") :
				PathAdmission == EVoxelTerrainResourceAdmission::CapacityUnachievable ? TEXT("CapacityUnachievable: Native Fine path") :
				TEXT("InvalidRequest: Native Fine path");
			return false;
		}
		Item = VoxelBoundedAllocation::MakeWorkerShared<FVoxelTerrainRenderAtom>(AtomLease);
		Item->MetadataLease = MoveTemp(AtomLease);
		Item->Section = Base->Section; Item->Evidence = Base->Evidence; Item->FineGeometry = Base->FineGeometry;
		Item->Chunks.Reserve(Base->Chunks.Num()); Item->Chunks.Append(Base->Chunks);
		Item->Boundaries.Reserve(Base->Boundaries.Num()); Item->Boundaries.Append(Base->Boundaries);
		Item->bUseDrawRanges = true;
		Item->DrawRanges.Reserve(RangeCount);
		ChunkCursor = TriangleCursor = 0; RunStart = INDEX_NONE;
		return true;
	}

	EVoxelFineBuildProgress AdvanceHeightfield(const FVoxelRegistrySnapshot& Registry, const FVoxelShapeRegistry& Shapes,
		const FVoxelGenerationRuntimeConfig& Config, const TAtomic<bool>& Cancel, FString& OutError)
	{
		if (Cancel.Load()) return EVoxelFineBuildProgress::Canceled;
		const auto& Neighbor = NeighborCoarse[FaceCursor];
		const auto& Source = Neighbor->Geometry->Source;
		const TConstArrayView<FVoxelDistantCell> Cells = Source->FrozenSurface ? MakeArrayView(Source->FrozenSurface->DistantCells) : MakeArrayView(Source->FrozenMacro->DistantCells);
		if (!bDistantSliceReady)
		{
			const FVector Min = FVector(Base->Section) * 16.0;
			const int32 Axis = FaceCursor / 2, U = Axis == 0 ? 1 : 0, V = Axis == 2 ? 1 : 2;
			const double Plane = Min[Axis] + ((FaceCursor & 1) ? -1.e-6 : 16.0 + 1.e-6);
			for (int32 Step = 0; Step < 32 && DistantCursor < Cells.Num(); ++Step)
			{
				const int32 Index = DistantCursor++; const auto& Cell = Cells[Index];
				if (Cell.Min[Axis] > Plane || Cell.Max[Axis] < Plane || Cell.Min[U] >= Min[U] + 16 || Cell.Max[U] <= Min[U] ||
					Cell.Min[V] >= Min[V] + 16 || Cell.Max[V] <= Min[V]) continue;
				const int32 MinU = FMath::Clamp(Cell.Min[U] - int32(Min[U]), 0, 16);
				const int32 MaxU = FMath::Clamp(Cell.Max[U] - int32(Min[U]), 0, 16);
				const int32 MinV = FMath::Clamp(Cell.Min[V] - int32(Min[V]), 0, 16);
				const int32 MaxV = FMath::Clamp(Cell.Max[V] - int32(Min[V]), 0, 16);
				for (int32 Y = MinV; Y < MaxV; ++Y) for (int32 X = MinU; X < MaxU; ++X)
				{
					const int32 Tile = X + Y * 16;
					if (DistantSlice)
					{
						if (DistantTileCursors[Tile] >= DistantSlice->TileOffsets[Tile + 1])
						{ OutError = TEXT("Frozen distant tile Count/Build mismatch"); return EVoxelFineBuildProgress::Failed; }
						DistantSlice->Indices[DistantTileCursors[Tile]++] = Index;
					}
					else
					{
						if (DistantCount == MAX_int32) { OutError = TEXT("Oversized: Native distant tile index"); return EVoxelFineBuildProgress::Failed; }
						++DistantTileCursors[Tile]; ++DistantCount;
					}
				}
			}
			if (DistantCursor < Cells.Num()) return EVoxelFineBuildProgress::Working;
			if (!DistantSlice)
			{
				TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> SliceLease;
				if (!AcquireNative(EVoxelTerrainResourcePool::Input,
					VoxelBoundedAllocation::WorkerSharedBytes<FNativeHeightfieldDistantSlice>() + VoxelBoundedAllocation::ArrayBytes<int32>(DistantCount), SliceLease, OutError))
					return RejectedAdmission(OutError);
				DistantSlice = VoxelBoundedAllocation::MakeWorkerShared<FNativeHeightfieldDistantSlice>(SliceLease);
				DistantSlice->Lease = MoveTemp(SliceLease); DistantSlice->Geometry = Neighbor->Geometry;
				DistantSlice->Indices.Reserve(DistantCount);
				DistantSlice->Indices.AddUninitialized(DistantCount);
				for (int32 Tile = 0; Tile < 256; ++Tile)
				{
					DistantSlice->TileOffsets[Tile + 1] = DistantSlice->TileOffsets[Tile] + DistantTileCursors[Tile];
					DistantTileCursors[Tile] = DistantSlice->TileOffsets[Tile];
				}
				DistantCursor = 0;
				return EVoxelFineBuildProgress::Working;
			}
			for (int32 Tile = 0; Tile < 256; ++Tile)
				if (DistantTileCursors[Tile] != DistantSlice->TileOffsets[Tile + 1])
				{ OutError = TEXT("Frozen distant tile Count/Build mismatch"); return EVoxelFineBuildProgress::Failed; }
			bDistantSliceReady = true;
		}
		if (Registry.Definitions.Num() > MAX_int32 / 12)
		{ OutError = TEXT("CapacityUnachievable: Native interface batch metadata"); return EVoxelFineBuildProgress::Failed; }
		const int32 BatchBound = 12 * Registry.Definitions.Num();
		const uint64 Scratch = VoxelBoundedAllocation::ArrayBytes<FVoxelRenderBatch>(BatchBound);
		if (Scratch > MeshPieceLimit) { OutError = TEXT("Oversized: Native heightfield interface metadata"); return EVoxelFineBuildProgress::Failed; }
		const TConstArrayView<int32> TileIndices(DistantSlice->Indices.IsEmpty() ? nullptr :
			DistantSlice->Indices.GetData() + DistantSlice->TileOffsets[TileCursor],
			DistantSlice->TileOffsets[TileCursor + 1] - DistantSlice->TileOffsets[TileCursor]);
		if (!PendingPlan)
		{
			if (!AcquireNative(EVoxelTerrainResourcePool::Working, Scratch, PendingWorkingLease, OutError)) return RejectedAdmission(OutError);
			FVoxelSectionMeshResult Counted;
			if (!NativeHeightfieldInterface::BuildTile(*Base, *Neighbor, FaceCursor, TileCursor, Config, Registry, Shapes,
				MaximumTextureStretchCells, TileIndices, Counted, OutError, true)) return EVoxelFineBuildProgress::Failed;
			PendingPlan.Emplace(MoveTemp(Counted));
		}
		const auto& Plan = PendingPlan.GetValue();
		if (Plan.Batches.ContainsByPredicate([](const auto& Batch) { return Batch.Mesh.PlannedIndices > 0; }))
		{
			const uint64 Bytes = Plan.PlannedBytes() + VoxelBoundedAllocation::WorkerSharedBytes<FVoxelSectionMeshResult>() +
				VoxelBoundedAllocation::WorkerSharedBytes<FNativeHeightfieldInputs>();
			if (Bytes > MeshPieceLimit - Scratch || Bytes > GeometryLimit || OutputBytesTotal > GeometryLimit - Bytes)
			{ OutError = TEXT("Oversized: Native heightfield interface Piece or Shard"); return EVoxelFineBuildProgress::Failed; }
			TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> OutputLease, PieceLease;
			if (!AcquireNative(EVoxelTerrainResourcePool::Staged, Bytes, OutputLease, OutError) ||
				!AcquireNative(EVoxelTerrainResourcePool::Input, VoxelBoundedAllocation::WorkerSharedBytes<FNativeInterfacePiece>(), PieceLease, OutError, OutputLease))
				return RejectedAdmission(OutError);
			auto Mesh = VoxelBoundedAllocation::MakeWorkerShared<FVoxelSectionMeshResult>(OutputLease);
			if (!NativeHeightfieldInterface::BuildTile(*Base, *Neighbor, FaceCursor, TileCursor, Config, Registry, Shapes,
				MaximumTextureStretchCells, TileIndices, *Mesh, OutError, false, &Plan)) return EVoxelFineBuildProgress::Failed;
			auto Inputs = VoxelBoundedAllocation::MakeWorkerShared<FNativeHeightfieldInputs>(OutputLease);
			Inputs->Section = Base->Section; Inputs->Face = FaceCursor;
			Inputs->FineOwner = Base->Evidence.Owner; Inputs->FineContent = Base->Evidence.Content;
			Inputs->CoarseOwner = Neighbor->Evidence.Owner; Inputs->CoarseContent = Neighbor->Evidence.Content;
			Inputs->FineGeometry = Base->FineGeometry; Inputs->CoarseGeometry = Neighbor->Geometry; Inputs->Lease = OutputLease;
			Inputs->DistantSlice = DistantSlice;
			Mesh->FrozenNativeInterfaceInputs = MoveTemp(Inputs); Mesh->ResourceLease = MoveTemp(OutputLease);
			auto Piece = VoxelBoundedAllocation::MakeWorkerShared<FNativeInterfacePiece>(PieceLease);
			Piece->Lease = MoveTemp(PieceLease); Piece->Mesh = MoveTemp(Mesh); Piece->Origin = FVector(Base->Section) * 16.0;
			if (LastPiece) LastPiece->Next = Piece; else FirstPiece = Piece;
			LastPiece = MoveTemp(Piece); OutputBytesTotal += Bytes;
		}
		PendingPlan.Reset();
		PendingWorkingLease.Reset();
		if (++TileCursor == 256)
		{
			TileCursor = 0; ++FaceCursor; DistantSlice.Reset(); DistantCursor = DistantCount = 0; bDistantSliceReady = false;
			FMemory::Memzero(DistantTileCursors);
		}
		return EVoxelFineBuildProgress::Working;
	}

	EVoxelFineBuildProgress Advance(const FVoxelRegistrySnapshot& Registry, const FVoxelShapeRegistry& Shapes,
		const FVoxelGenerationRuntimeConfig& Config,
		const TAtomic<bool>& Cancel, FString& OutError)
	{
		if (Cancel.Load()) return EVoxelFineBuildProgress::Canceled;
		if (Phase < 2)
		{
			if (ChunkCursor == Base->Chunks.Num())
			{
				if (Phase == 0) { if (!PrepareAtom(OutError)) return RejectedAdmission(OutError); Phase = 1; }
				else Phase = 2;
				return EVoxelFineBuildProgress::Working;
			}
			const auto Chunk = Base->Chunks[ChunkCursor];
			const int32 End = Chunk->SourceTriangleCount;
			for (int32 Step = 0; Step < 64 && TriangleCursor < End; ++Step, ++TriangleCursor)
			{
				const bool bKeep = KeepTriangle(*Chunk, TriangleCursor + Chunk->FirstSourceTriangle);
				if (bKeep && RunStart == INDEX_NONE) RunStart = TriangleCursor;
				if (RunStart != INDEX_NONE && (!bKeep || TriangleCursor + 1 == End))
				{
					const int32 Count = TriangleCursor - RunStart + (bKeep ? 1 : 0);
					if (Phase == 0) ++RangeCount;
					else Item->DrawRanges.Add({Chunk, RunStart, Count});
					RunStart = INDEX_NONE;
				}
			}
			if (TriangleCursor == End) { ++ChunkCursor; TriangleCursor = 0; }
			return EVoxelFineBuildProgress::Working;
		}
		if (FaceCursor == 6) return EVoxelFineBuildProgress::Complete;
		const uint8 Face = FaceCursor;
		const auto OtherFine = NeighborFine[Face];
		// 同精度界面由较小坐标一侧承载；两侧原面仍在同一个根事务中抑制。
		const bool bOwnFineInterface = OtherFine && (Base->Section.X != OtherFine->Section.X ? Base->Section.X < OtherFine->Section.X :
			Base->Section.Y != OtherFine->Section.Y ? Base->Section.Y < OtherFine->Section.Y : Base->Section.Z < OtherFine->Section.Z);
		if (!bSuppress[Face] || (OtherFine && !bOwnFineInterface))
		{ ++FaceCursor; TileCursor = 0; return EVoxelFineBuildProgress::Working; }
		if (OtherFine && !OtherFine->FineGeometry) { OutError = TEXT("DependencyNotReady:NativeNeighborBoundary"); return EVoxelFineBuildProgress::Failed; }
		if (!OtherFine && !NeighborCoarse[Face]->Geometry->Source->FrozenVoxelProxy)
			return AdvanceHeightfield(Registry, Shapes, Config, Cancel, OutError);
		if (Registry.Definitions.Num() > MAX_int32 / 12)
		{ OutError = TEXT("CapacityUnachievable: Native interface batch metadata"); return EVoxelFineBuildProgress::Failed; }
		const int32 NativeSide = OtherFine ? 16 : NeighborCoarse[Face]->Geometry->Source->FrozenVoxelProxy->GridSide;
		if (NativeSide <= 0 || NativeSide > 128)
		{ OutError = TEXT("InvalidRequest: Native interface grid side"); return EVoxelFineBuildProgress::Failed; }
		const int32 BatchBound = int32(FMath::Min(int64(6) * NativeSide * NativeSide * NativeSide, int64(12) * Registry.Definitions.Num()));
		const uint64 Scratch = VoxelBoundedAllocation::ArrayBytes<FVoxelRenderBatch>(BatchBound) +
			VoxelBoundedAllocation::MapBytes<uint32, int32>(BatchBound) +
			VoxelBoundedAllocation::ArrayBytes<FVoxelBoundaryTransitionPatch>(1);
		if (Scratch > MeshPieceLimit)
		{ OutError = TEXT("Oversized:Native interface count metadata"); return EVoxelFineBuildProgress::Failed; }
		if (!PendingWorkingLease && !AcquireNative(EVoxelTerrainResourcePool::Working, Scratch, PendingWorkingLease, OutError))
			return RejectedAdmission(OutError);
		if (!PendingPlan)
		{
			FVoxelBoundaryTransitionContext Context;
			FVoxelSectionSnapshot Snapshot;
			FVoxelBoundaryTransitionPatch Patch;
			const auto Proxy = OtherFine ? nullptr : NeighborCoarse[Face]->Geometry->Source->FrozenVoxelProxy;
			const uint8 OwnerFace = Proxy ? Face ^ 1 : Face;
			Context.Owner = Proxy ? Proxy->Key : FVoxelViewKey{Base->Section, 0};
			Context.OwnerGeometry = Proxy;
			Context.FineOwnerGeometry = Proxy ? nullptr : Base->FineGeometry;
			Context.MaximumTextureStretchCells = MaximumTextureStretchCells;
			Context.SampleStepCells = Proxy ? Proxy->Geometry.SampleStepCells : 1;
			Context.WorldEpoch = Base->Evidence.Content.WorldEpoch;
			Context.GeneratorIdentity = Base->Evidence.Content.GeneratorIdentity;
			Context.RegistryRevision = Base->Evidence.Content.RegistryRevision;
			Context.ShapeRevision = Base->Evidence.Content.ShapeRevision;
			Context.InputSerial = Consumer.RequestSerial;
			const auto Neighbor = Proxy ? Base : OtherFine;
			const FVoxelViewKey NeighborKey{Neighbor->Section, 0};
			if (!FVoxelBoundaryFaceSnapshot::CaptureFineShardAtom(NeighborKey, OwnerFace ^ 1,
				Neighbor->FineGeometry, Patch.Neighbor))
			{ OutError = TEXT("Invalid frozen Native boundary"); return EVoxelFineBuildProgress::Failed; }
			const auto NeighborBounds = NeighborKey.GetBounds();
			Patch.Face.Owner = Context.Owner; Patch.Face.Neighbor = NeighborKey;
			Patch.Face.Direction = static_cast<EVoxelVolumeFaceDirection>(OwnerFace);
			Patch.Face.Ratio = Context.Owner.GetSide() / 16;
			Patch.Face.Min = NeighborBounds.Min; Patch.Face.Max = NeighborBounds.Max;
			const int32 Axis = OwnerFace / 2, U = Axis == 0 ? 1 : 0, V = Axis == 2 ? 1 : 2;
			Patch.Face.Min[U] += (TileCursor % 4) * 4; Patch.Face.Min[V] += (TileCursor / 4) * 4;
			Patch.Face.Max[U] = Patch.Face.Min[U] + 4; Patch.Face.Max[V] = Patch.Face.Min[V] + 4;
			Context.Patches.Reserve(1); Context.Patches.Add(MoveTemp(Patch));
			Snapshot.GridSide = Context.GetGridSide();
			FVoxelSectionMeshResult Plan;
			if (!FVoxelSectionMesher::Build(Snapshot, Registry, Shapes, Plan, &Cancel, 1.0, &Context, true, true, {}, 0, true))
			{ OutError = TEXT("Failed to count frozen Native interface"); return EVoxelFineBuildProgress::Failed; }
			PendingContext.Emplace(MoveTemp(Context));
			PendingSnapshot.Emplace(MoveTemp(Snapshot));
			PendingPlan.Emplace(MoveTemp(Plan));
		}
		auto& Context = PendingContext.GetValue();
		const auto& Snapshot = PendingSnapshot.GetValue();
		const auto& Plan = PendingPlan.GetValue();
		if (!Plan.Batches.ContainsByPredicate([](const FVoxelRenderBatch& Batch) { return Batch.Mesh.PlannedIndices > 0; }))
		{
			ClearPendingPlan();
			if (++TileCursor == 16) { TileCursor = 0; ++FaceCursor; }
			return EVoxelFineBuildProgress::Working;
		}
		const uint64 ContextBytes = VoxelBoundedAllocation::WorkerSharedBytes<FVoxelBoundaryTransitionContext>() + Context.Patches.GetAllocatedSize();
		const uint64 OutputBytes = Plan.PlannedBytes() + ContextBytes + VoxelBoundedAllocation::WorkerSharedBytes<FVoxelSectionMeshResult>();
		if (OutputBytes > MeshPieceLimit - Scratch) { OutError = TEXT("Oversized:Native interface Piece"); return EVoxelFineBuildProgress::Failed; }
		if (OutputBytes > GeometryLimit || OutputBytesTotal > GeometryLimit - OutputBytes)
		{ OutError = TEXT("Oversized:Native interface geometry"); return EVoxelFineBuildProgress::Failed; }
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> OutputLease, PieceLease;
		if (!AcquireNative(EVoxelTerrainResourcePool::Staged, OutputBytes, OutputLease, OutError) ||
			!AcquireNative(EVoxelTerrainResourcePool::Input,
				VoxelBoundedAllocation::WorkerSharedBytes<FNativeInterfacePiece>(), PieceLease, OutError, OutputLease)) return RejectedAdmission(OutError);
		auto Mesh = VoxelBoundedAllocation::MakeWorkerShared<FVoxelSectionMeshResult>(OutputLease);
		if (!FVoxelSectionMesher::Build(Snapshot, Registry, Shapes, *Mesh, &Cancel, 1.0, &Context, true, true, {}, 0, false, &Plan))
		{ OutError = TEXT("Native interface Count/Build mismatch"); return EVoxelFineBuildProgress::Failed; }
		auto Frozen = VoxelBoundedAllocation::MakeWorkerShared<FVoxelBoundaryTransitionContext>(OutputLease, MoveTemp(Context));
		Frozen->ResourceLease = OutputLease; Mesh->ResourceLease = MoveTemp(OutputLease); Mesh->FrozenBoundaryInputs = Frozen;
		auto Piece = VoxelBoundedAllocation::MakeWorkerShared<FNativeInterfacePiece>(PieceLease);
		Piece->Lease = MoveTemp(PieceLease); Piece->Mesh = MoveTemp(Mesh);
		Piece->Origin = FVector(Frozen->Owner.GetBounds().Min); Piece->Scale = Frozen->GetSampleStep();
		if (LastPiece) LastPiece->Next = Piece; else FirstPiece = Piece;
		LastPiece = MoveTemp(Piece);
		OutputBytesTotal += OutputBytes;
		ClearPendingPlan();
		if (++TileCursor == 16) { TileCursor = 0; ++FaceCursor; }
		return EVoxelFineBuildProgress::Working;
	}

	void ClearPendingPlan()
	{
		PendingPlan.Reset();
		PendingSnapshot.Reset();
		PendingContext.Reset();
		PendingWorkingLease.Reset();
	}

	TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	FVoxelGenerationConsumerKey Consumer;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease, PathLease;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ControlInputLease;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> PreparationInputLease;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> PublicationLease, PublicationCallbackLease;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> PendingWorkingLease;
	TOptional<FVoxelSectionMeshResult> PendingPlan;
	TOptional<FVoxelSectionSnapshot> PendingSnapshot;
	TOptional<FVoxelBoundaryTransitionContext> PendingContext;
	FAtomPtr Base;
	FAtomPtr NeighborFine[6];
	FCoarsePtr NeighborCoarse[6];
	bool bSuppress[6] = {};
	bool bAllowOverlap = false;
	TSharedPtr<FVoxelTerrainRenderAtom, ESPMode::ThreadSafe> Item;
	TSharedPtr<FPreparedFine, ESPMode::ThreadSafe> Next;
	TSharedPtr<FNativeInterfacePiece, ESPMode::ThreadSafe> FirstPiece, LastPiece, Upload;
	TSharedPtr<FNativeHeightfieldDistantSlice, ESPMode::ThreadSafe> DistantSlice;
	int32 DistantCursor = 0, DistantCount = 0;
	int32 DistantTileCursors[256] = {};
	bool bDistantSliceReady = false;
	double CellSize = 1.0;
	double MaximumTextureStretchCells = 8.0;
	uint64 OutputBytesTotal = 0;
	uint64 MeshPieceLimit = 0;
	uint64 GeometryLimit = 0;
	int32 Phase = 0, ChunkCursor = 0, TriangleCursor = 0, RangeCount = 0, RunStart = INDEX_NONE;
	uint8 FaceCursor = 0;
	uint16 TileCursor = 0;
	int32 UploadBatch = 0, UploadTriangle = 0;
};
