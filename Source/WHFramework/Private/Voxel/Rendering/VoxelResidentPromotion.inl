// 包含于 Renderer 的私有命名空间；只读根保活，游标及其最后引用均由 Worker 销毁。
template<typename NodeType, int32 Depth>
struct TVoxelPromotionWalk
{
	struct FFrame { const NodeType* Node = nullptr; int32 Child = -1; };
	TArray<FFrame, TInlineAllocator<Depth>> Frames;

	void Begin(const NodeType* Root)
	{
		Frames.Reset();
		if (Root) Frames.Add({Root, -1});
	}

	void Rebase(const NodeType* Root)
	{
		if (!Root) { Frames.Reset(); return; }
		if (Frames.IsEmpty()) return;
		Frames[0].Node = Root;
		for (int32 Index = 1; Index < Frames.Num(); ++Index)
		{
			const auto& Parent = Frames[Index - 1];
			const NodeType* Child = Parent.Node->Children[Parent.Child - 1].Get();
			if (!Child) { Frames.SetNum(Index, EAllowShrinking::No); return; }
			Frames[Index].Node = Child;
		}
	}

	bool IsComplete() const { return Frames.IsEmpty(); }

	template<typename Predicate>
	const NodeType* Next(Predicate&& Accept)
	{
		if (Frames.IsEmpty()) return nullptr;
		auto& Frame = Frames.Last();
		if (Frame.Child == -1)
		{
			if (!Accept(*Frame.Node)) { Frames.Pop(EAllowShrinking::No); return nullptr; }
			Frame.Child = 0; return Frame.Node;
		}
		if (Frame.Child == UE_ARRAY_COUNT(Frame.Node->Children)) { Frames.Pop(EAllowShrinking::No); return nullptr; }
		const NodeType* Child = Frame.Node->Children[Frame.Child++].Get();
		if (Child) { check(Frames.Num() < Depth); Frames.Add({Child, -1}); }
		return nullptr;
	}

	const NodeType* Next() { return Next([](const NodeType&) { return true; }); }
};

struct FResidentPromotion
{
	explicit FResidentPromotion(TSharedPtr<const FPresentedSnapshot, ESPMode::ThreadSafe> InSnapshot,
		FSnapshotPtr InFrame, const double InCellSize)
		: Snapshot(MoveTemp(InSnapshot)), Frame(MoveTemp(InFrame)), CellSize(InCellSize)
	{
		Fine.Begin(Snapshot->Root.Get()); Coarse.Begin(Snapshot->CoarseRoot.Get());
	}

	void Rebase(TSharedPtr<const FPresentedSnapshot, ESPMode::ThreadSafe> InSnapshot, FSnapshotPtr InFrame)
	{
		// 保留 radix 游标，重新绑定当前根；不会因每帧发布而反复从低坐标开始。
		if (Atom && FindFineAtom(InSnapshot->Root, Atom->Section).Get() != Atom) Atom = nullptr;
		if (Item && FindCoarseItem(InSnapshot->CoarseRoot, Item->Geometry->Stamp.GeometryIdentity.Key, Item->SourceId).Get() != Item)
		{ Item = nullptr; SourceChunk = nullptr; DrawPiece = nullptr; Draw.Begin(nullptr); }
		Fine.Rebase(InSnapshot->Root.Get()); Coarse.Rebase(InSnapshot->CoarseRoot.Get());
		Snapshot = MoveTemp(InSnapshot); Frame = MoveTemp(InFrame);
	}

	static bool Promote(const TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& Lease)
	{
		// 普通池尚被旧 GPU 帧占满时保留原池，下一轮再尝试；不会丢弃或伪造资源释放。
		if (!Lease) return true;
		// 已收呈现回执的正文释放必须推进 PublishedRetirementVersion，唤醒同 owner 的后续事务。
		Lease->MarkPublished();
		return Lease->Transfer(EVoxelTerrainResourcePool::Resident);
	}

	static bool PromoteChunk(const FVoxelTerrainRenderChunk* Chunk)
	{
		if (!Chunk || Chunk->bResidentResourcesReady.Load()) return true;
		bool Ready = Promote(Chunk->ResidentLease) & (!Chunk->Source || Promote(Chunk->Source->ResourceLease));
		if (Chunk->Source && Chunk->Source->FrozenNativeInterfaceInputs)
		{
			const auto Inputs = StaticCastSharedPtr<const FNativeHeightfieldInputs>(Chunk->Source->FrozenNativeInterfaceInputs);
			if (Inputs->DistantSlice) Ready &= Promote(Inputs->DistantSlice->Lease);
		}
		Chunk->bResidentResourcesReady.Store(Ready);
		return Ready;
	}

	void Advance()
	{
		if (Phase == 0)
		{
			Promote(Snapshot->Lease);
			if (Frame) Promote(Frame->Lease);
			if (Frame && Frame->Issued.IsValidIndex(PublicationIndex))
			{
				Promote(Frame->Issued[PublicationIndex++].Resources->Lease);
				return;
			}
			PublicationIndex = SourceIndex = 0; Phase = 1; return;
		}
		if (Phase == 1)
		{
			if (!Frame || !Frame->Issued.IsValidIndex(PublicationIndex))
			{ Fine.Begin(Snapshot->Root.Get()); Coarse.Begin(Snapshot->CoarseRoot.Get()); Phase = 4; return; }
			const auto& Publication = *Frame->Issued[PublicationIndex].Resources;
			if (Publication.Atoms.IsValidIndex(SourceIndex))
			{
				const FVector Min = FVector(Publication.Atoms[SourceIndex++]->Section) * 16.0;
				LocalBounds = FBox(Min, Min + FVector(16.0)).ExpandBy(16.0);
			}
			else if (Publication.CoarseChanges.IsValidIndex(SourceIndex - Publication.Atoms.Num()))
				LocalBounds = Publication.CoarseChanges[SourceIndex++ - Publication.Atoms.Num()].Item->OwnershipBounds.ExpandBy(16.0);
			else { ++PublicationIndex; SourceIndex = 0; return; }
			Fine.Begin(Snapshot->Root.Get()); Coarse.Begin(Snapshot->CoarseRoot.Get()); Phase = 2; return;
		}
		if (Phase == 2 || Phase == 4)
		{
			if (Atom) { AdvanceAtom(); return; }
			const auto* Node = Fine.Next([this](const FAtomTreeNode& Candidate)
			{ return Phase == 4 || FBox(Candidate.Bounds.Min / CellSize, Candidate.Bounds.Max / CellSize).Intersect(LocalBounds); });
			if (!Node) { if (Fine.IsComplete()) ++Phase; return; }
			Promote(Node->PathLease);
			Atom = Node->Atom.Get(); Stage = Index = 0; bItemReady = true;
			return;
		}
		if (Item) { AdvanceCoarse(); return; }
		if (const auto* Node = Coarse.Next([this](const FCoarseTreeNode& Candidate)
			{ return Phase == 5 || Candidate.CellBounds.Intersect(LocalBounds); }))
		{
			Promote(Node->Lease);
			Item = Node->Item.Get(); Stage = Index = DrawRoot = 0; bItemReady = true;
			return;
		}
		if (!Coarse.IsComplete()) return;
		Phase = Phase == 3 ? 1 : 0;
	}

	void AdvanceAtom()
	{
		if (Atom->bResidentResourcesReady.Load()) { Atom = nullptr; return; }
		const auto* Geometry = Atom->FineGeometry.Get();
		switch (Stage)
		{
		case 0: bItemReady &= Promote(Atom->MetadataLease); ++Stage; return;
		case 1:
			if (!Geometry || Geometry->bResidentResourcesReady.Load()) { Stage = 4; return; }
			bGeometryReady = Promote(Geometry->MetadataLease); ++Stage; return;
		case 2: if (Geometry && Geometry->Boundary) bGeometryReady &= Promote(Geometry->Boundary->ResourceLease); ++Stage; return;
		case 3:
			if (Geometry && Geometry->Pieces.IsValidIndex(Index))
			{
				const auto& Piece = Geometry->Pieces[Index++];
				if (Piece.Mesh) bGeometryReady &= Promote(Piece.Mesh->ResourceLease);
				return;
			}
			if (Geometry) Geometry->bResidentResourcesReady.Store(bGeometryReady);
			bItemReady &= bGeometryReady;
			Index = 0; ++Stage; return;
		case 4:
			if (Atom->Chunks.IsValidIndex(Index)) { bItemReady &= PromoteChunk(Atom->Chunks[Index++].Get()); return; }
			Index = 0; ++Stage; return;
		case 5:
			if (Atom->DrawRanges.IsValidIndex(Index)) { bItemReady &= PromoteChunk(Atom->DrawRanges[Index++].Chunk.Get()); return; }
			Atom->bResidentResourcesReady.Store(bItemReady);
			Atom = nullptr; return;
		default: checkNoEntry();
		}
	}

	void AdvanceCoarse()
	{
		if (Item->bResidentResourcesReady.Load()) { Item = nullptr; return; }
		const auto& Source = Item->Geometry->Source;
		switch (Stage)
		{
		case 0: bItemReady &= Promote(Item->MetadataLease); ++Stage; return;
		case 1: bItemReady &= Promote(Item->Geometry->MetadataLease); ++Stage; return;
		case 2:
			bItemReady &= Promote(Source->ResourceLease);
			if (Source->FrozenVoxelProxy) bItemReady &= Promote(Source->FrozenVoxelProxy->ResourceLease);
			if (Source->FrozenSurface) bItemReady &= Promote(Source->FrozenSurface->ResourceLease);
			if (Source->FrozenMacro) bItemReady &= Promote(Source->FrozenMacro->ResourceLease);
			if (Source->FrozenWater) bItemReady &= Promote(Source->FrozenWater->ResourceLease);
			++Stage; return;
		case 3: if (Item->Patch) bItemReady &= Promote(Item->Patch->MetadataLease); ++Stage; return;
		case 4:
			if (SourceChunk) { bItemReady &= PromoteChunk(SourceChunk); SourceChunk = SourceChunk->NextSourceChunk.Get(); return; }
			if (Item->SourceChunks.IsValidIndex(Index)) { SourceChunk = Item->SourceChunks[Index++].Get(); return; }
			++Stage; return;
		case 5:
			if (DrawPiece)
			{
			bItemReady &= Promote(DrawPiece->Lease); bItemReady &= PromoteChunk(DrawPiece->Chunk.Get());
				if (DrawPiece->Piece)
				{
					bItemReady &= Promote(DrawPiece->Piece->MetadataLease);
					if (DrawPiece->Piece->Mesh) bItemReady &= Promote(DrawPiece->Piece->Mesh->ResourceLease);
				}
				DrawPiece = DrawPiece->Next.Get(); return;
			}
			if (const auto* Node = Draw.Next())
			{
				bItemReady &= Promote(Node->Lease);
				if (Node->Patch) bItemReady &= Promote(Node->Patch->MetadataLease);
				DrawPiece = Node->FirstPiece.Get(); return;
			}
			if (!Draw.IsComplete()) return;
			if (Item->DrawRoots.IsValidIndex(DrawRoot)) { Draw.Begin(Item->DrawRoots[DrawRoot++].Get()); return; }
			Item->bResidentResourcesReady.Store(bItemReady);
			Item = nullptr; return;
		default: checkNoEntry();
		}
	}

	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InputLease;
	TSharedPtr<const FPresentedSnapshot, ESPMode::ThreadSafe> Snapshot;
	FSnapshotPtr Frame;
	TVoxelPromotionWalk<FAtomTreeNode, 33> Fine;
	TVoxelPromotionWalk<FCoarseTreeNode, 79> Coarse;
	TVoxelPromotionWalk<FVoxelTerrainCoarseDrawNode, 64> Draw;
	const FVoxelTerrainRenderAtom* Atom = nullptr;
	const FVoxelTerrainRenderCoarse* Item = nullptr;
	const FVoxelTerrainRenderChunk* SourceChunk = nullptr;
	const FVoxelTerrainCoarseDrawPiece* DrawPiece = nullptr;
	int32 Phase = 0, Stage = 0, Index = 0, DrawRoot = 0;
	int32 PublicationIndex = 0, SourceIndex = 0;
	double CellSize = 1.0;
	FBox LocalBounds = FBox(ForceInit);
	bool bItemReady = true, bGeometryReady = true;
};
