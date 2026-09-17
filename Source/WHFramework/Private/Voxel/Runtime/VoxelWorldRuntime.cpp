#include "Voxel/Runtime/VoxelWorldRuntime.h"
#include "Voxel/Save/VoxelBlockEntityCodec.h"
#include "Voxel/Save/VoxelDeltaCodec.h"
FVoxelWorldRuntime::FVoxelWorldRuntime(uint64 E,
                                       bool A,
                                       TSharedRef<const FVoxelRegistrySnapshot, ESPMode::ThreadSafe> R,
                                       TSharedRef<const FVoxelGenerationPipeline, ESPMode::ThreadSafe> G)
    : WorldEpoch(E)
    , bAuthority(A)
    , Registry(R)
    , Generator(G)
{
}
FVoxelSection* FVoxelWorldRuntime::Find(const FVoxelSectionKey& K)
{
	auto* P = Sections.Find(K);
	return P ? P->Get() : nullptr;
}
const FVoxelSection* FVoxelWorldRuntime::Find(const FVoxelSectionKey& K) const
{
	const auto* P = Sections.Find(K);
	return P ? P->Get() : nullptr;
}
FVoxelSection* FVoxelWorldRuntime::Allocate(const FVoxelSectionKey& K, double Now)
{
	check(IsInGameThread());
	if (auto* S = Find(K))
		return S;
	const auto& C = Generator->GetConfig().Settings;
	if (!VoxelCoord::IsValidSection(K, C.MinZ, C.MaxZ) || NextToken == MAX_uint64)
		return nullptr;
	auto S = MakeShared<FVoxelSection>();
	S->Stamp.WorldEpoch = WorldEpoch;
	S->Stamp.Key = K;
	S->Stamp.GenerationToken = NextToken++;
	S->Stamp.GeometryVersion = 1;
	S->Overlay.Key = K;
	S->LastWanted = Now;
	FVoxelSection* P = &S.Get();
	Sections.Add(K, MoveTemp(S));
	return P;
}
TArray<FVoxelSectionKey> FVoxelWorldRuntime::ResidentKeys() const
{
	TArray<FVoxelSectionKey> K;
	Sections.GetKeys(K);
	return K;
}
bool FVoxelWorldRuntime::TryGetBlock(const FIntVector& P, FVoxelBlockState& O) const
{
	if (!VoxelCoord::IsValid(P))
		return false;
	const auto& C = Generator->GetConfig().Settings;
	if (P.Z < C.MinZ || P.Z >= C.MaxZ)
	{
		O = {};
		return true;
	}
	const auto* S = Find(VoxelCoord::Section(P));
	if (!S || S->Status != EVoxelSectionStatus::DataReady)
		return false;
	O = S->Blocks.Get(VoxelCoord::Linear(VoxelCoord::Local(P)));
	return true;
}
bool FVoxelWorldRuntime::IsCurrent(const FVoxelTaskStamp& T, bool Geometry) const
{
	const auto* S = Find(T.Key);
	return S && S->Status != EVoxelSectionStatus::Unloading && S->Stamp.WorldEpoch == T.WorldEpoch && S->Stamp.GenerationToken == T.GenerationToken &&
	       S->Stamp.Revision == T.Revision && (!Geometry || S->Stamp.GeometryVersion == T.GeometryVersion);
}
void FVoxelWorldRuntime::Invalidate(const FVoxelSectionKey& K, bool Neighbors)
{
	auto Mark = [&](const FVoxelSectionKey& P)
	{
		if (auto* S = Find(P))
		{
			check(S->Stamp.GeometryVersion < MAX_uint64);
			++S->Stamp.GeometryVersion;
			S->bMeshDirty = S->bCollisionDirty = true;
		}
	};
	Mark(K);
	if (Neighbors)
		for (uint8 F = 0; F < 6; ++F)
			Mark(VoxelCoord::Neighbor(K, F));
}
void FVoxelWorldRuntime::InvalidateEdit(const FVoxelSectionPatch& P)
{
	Invalidate(P.Key, false);
	TSet<uint8> Faces;
	for (const auto& W : P.Blocks)
	{
		FIntVector L = VoxelCoord::Unlinear(W.Index);
		for (uint8 A = 0; A < 3; ++A)
		{
			if (L[A] == 0)
				Faces.Add(A * 2 + 1);
			if (L[A] == 15)
				Faces.Add(A * 2);
		}
	}
	for (uint8 F : Faces)
		Invalidate(VoxelCoord::Neighbor(P.Key, F), false);
}
bool FVoxelWorldRuntime::ValidateOverlay(const FVoxelSectionOverlay& O, const FVoxelSectionStorage& E) const
{
	if (O.Blocks.Num() > 4096 || O.Entities.Num() > 256 || !FVoxelDeltaCodec::MaxEncodedBytes(*Registry, O))
		return false;
	uint64 Bytes = 0;
	for (const auto& B : O.Blocks)
		if (B.Key >= 4096 || !Registry->IsValid(B.Value))
			return false;
	for (const auto& B : O.Entities)
	{
		if (B.Key >= 4096 || !FVoxelBlockEntityCodec::Validate(B.Value))
			return false;
		const auto* D = Registry->Find(E.Get(B.Key).TypeId);
		if (!D || D->EntityKind == 0 || D->EntityKind != B.Value.Kind)
			return false;
		if (D->EntityKind == 100 && (B.Value.Payload.IsEmpty() || B.Value.Payload[0] != D->EntityVariant))
			return false;
		Bytes += B.Value.Payload.Num();
		if (Bytes > 256 * 1024)
			return false;
	}
	for (uint16 I = 0; I < 4096; ++I)
	{
		const auto* D = Registry->Find(E.Get(I).TypeId);
		if (!D)
			return false;
		if (D->EntityKind && !O.Entities.Contains(I))
			return false;
	}
	return true;
}
bool FVoxelWorldRuntime::PublishLoaded(const FVoxelTaskStamp& T, FVoxelSectionStorage&& Base, const FVoxelSectionOverlay& O, bool Committed)
{
	check(IsInGameThread());
	auto* S = Find(T.Key);
	if (!S || S->Status != EVoxelSectionStatus::Allocated || !IsCurrent(T, false) || O.Key != T.Key)
		return false;
	auto Natural = MakeShared<FVoxelSectionStorage, ESPMode::ThreadSafe>(Base);
	for (const auto& P : O.Blocks)
	{
		if (P.Key >= 4096 || !Registry->IsValid(P.Value))
			return false;
		Base.Set(P.Key, P.Value);
	}
	if (!ValidateOverlay(O, Base))
		return false;
	S->BaseBlocks = Natural;
	S->Blocks = MoveTemp(Base);
	S->Overlay = O;
	S->Stamp.Revision = O.Revision;
	S->CommittedRevision = Committed ? O.Revision : 0;
	S->Status = EVoxelSectionStatus::DataReady;
	Invalidate(T.Key, true);
	return true;
}
bool FVoxelWorldRuntime::CaptureSnapshot(const FVoxelSectionKey& K, FVoxelSectionSnapshot& O) const
{
	const auto* S = Find(K);
	if (!S || S->Status != EVoxelSectionStatus::DataReady)
		return false;
	FVoxelSectionSnapshot T;
	T.Stamp = S->Stamp;
	S->Blocks.CopyToDense(T.Blocks);
	FIntVector Origin = VoxelCoord::Origin(K);
	for (uint8 F = 0; F < 6; ++F)
	{
		int32 A = F / 2, U = (A + 1) % 3, V = (A + 2) % 3;
		T.Halo[F].SetNumUninitialized(256);
		bool Known = true;
		for (int32 Y = 0; Y < 16; ++Y)
			for (int32 X = 0; X < 16; ++X)
			{
				FIntVector P = Origin;
				P[A] += F % 2 == 0 ? 16 : -1;
				P[U] += X;
				P[V] += Y;
				FVoxelBlockState B;
				if (!TryGetBlock(P, B))
				{
					Known = false;
					B = {};
				}
				T.Halo[F][X + 16 * Y] = B.Pack();
			}
		T.Known[F] = Known;
	}
	O = MoveTemp(T);
	return true;
}
bool FVoxelWorldRuntime::PrepareEdit(const TArray<FVoxelCellEdit>& C, const TArray<FVoxelEntityEdit>& E, FVoxelPreparedEdit& O, FString& Error) const
{
	check(IsInGameThread());
	if (!bAuthority || C.Num() + E.Num() > 8192 || C.IsEmpty() && E.IsEmpty())
	{
		Error = TEXT("Invalid edit batch");
		return false;
	}
	FVoxelPreparedEdit T;
	T.TransactionId = FGuid::NewGuid();
	TMap<FVoxelSectionKey, int32> Map;
	TSet<FIntVector> SeenCells, SeenEntities;
	auto Target = [&](const FIntVector& P) -> FVoxelPreparedSection*
	{
		const auto& Cfg = Generator->GetConfig().Settings;
		if (!VoxelCoord::IsValid(P) || P.Z < Cfg.MinZ || P.Z >= Cfg.MaxZ)
			return nullptr;
		const auto K = VoxelCoord::Section(P);
		if (int32* I = Map.Find(K))
			return &T.Sections[*I];
		const auto* S = Find(K);
		if (!S || S->Status != EVoxelSectionStatus::DataReady || !S->BaseBlocks || S->Stamp.Revision == MAX_uint64 || Map.Num() >= 32)
			return nullptr;
		FVoxelPreparedSection N;
		N.Before = S->Stamp;
		N.Blocks = S->Blocks;
		N.Overlay = S->Overlay;
		N.Patch.Key = K;
		N.Patch.FromRevision = S->Stamp.Revision;
		N.Patch.ToRevision = S->Stamp.Revision + 1;
		int32 I = T.Sections.Add(MoveTemp(N));
		Map.Add(K, I);
		return &T.Sections[I];
	};
	for (const auto& W : C)
	{
		if (SeenCells.Contains(W.Position) || !Registry->IsValid(W.Value))
		{
			Error = TEXT("Duplicate or invalid block write");
			return false;
		}
		SeenCells.Add(W.Position);
		auto* N = Target(W.Position);
		if (!N)
		{
			Error = TEXT("Section not ready or edit out of range");
			return false;
		}
		uint16 I = VoxelCoord::Linear(VoxelCoord::Local(W.Position));
		auto Old = N->Blocks.Get(I);
		if (Old != W.Expected)
		{
			Error = TEXT("Stale block value");
			return false;
		}
		if (Old == W.Value)
			continue;
		N->Blocks.Set(I, W.Value);
		N->Patch.Blocks.Add({I, W.Value});
		const FVoxelSection* SourceSection = Find(VoxelCoord::Section(W.Position));
		if (!SourceSection || !SourceSection->BaseBlocks)
		{
			Error = TEXT("Natural section baseline is unavailable");
			return false;
		}
		if (W.Value == SourceSection->BaseBlocks->Get(I))
			N->Overlay.Blocks.Remove(I);
		else
			N->Overlay.Blocks.Add(I, W.Value);
		if (Old.TypeId != W.Value.TypeId)
		{
			if (N->Overlay.Entities.Remove(I))
			{
				FVoxelEntityWrite D;
				D.Index = I;
				D.bRemove = true;
				N->Patch.Entities.Add(D);
			}
			const auto* Def = Registry->Find(W.Value.TypeId);
			if (Def->EntityKind)
			{
				FVoxelBlockEntityState B;
				if (!FVoxelBlockEntityCodec::MakeDefault(Def->EntityKind, B, Def->EntityVariant))
				{
					Error = TEXT("Unsupported BlockEntity kind");
					return false;
				}
				N->Patch.Entities.RemoveAll(
				    [I](const FVoxelEntityWrite& X)
				    {
					    return X.Index == I;
				    });
				N->Overlay.Entities.Add(I, B);
				FVoxelEntityWrite U;
				U.Index = I;
				U.Value = B;
				N->Patch.Entities.Add(MoveTemp(U));
			}
		}
	}
	for (const auto& W : E)
	{
		if (SeenEntities.Contains(W.Position))
		{
			Error = TEXT("Duplicate entity write");
			return false;
		}
		SeenEntities.Add(W.Position);
		auto* N = Target(W.Position);
		if (!N)
		{
			Error = TEXT("Entity section not ready");
			return false;
		}
		uint16 I = VoxelCoord::Linear(VoxelCoord::Local(W.Position));
		const auto* D = Registry->Find(N->Blocks.Get(I).TypeId);
		if (W.bRemove)
		{
			if (D->EntityKind != 0)
			{
				Error = TEXT("Cannot remove required entity without removing block");
				return false;
			}
			N->Overlay.Entities.Remove(I);
		}
		else
		{
			if (D->EntityKind != W.Value.Kind || !FVoxelBlockEntityCodec::Validate(W.Value))
			{
				Error = TEXT("Invalid entity payload");
				return false;
			}
			N->Overlay.Entities.Add(I, W.Value);
		}
		N->Patch.Entities.RemoveAll(
		    [I](const FVoxelEntityWrite& X)
		    {
			    return X.Index == I;
		    });
		FVoxelEntityWrite V;
		V.Index = I;
		V.bRemove = W.bRemove;
		V.Value = W.Value;
		N->Patch.Entities.Add(MoveTemp(V));
	}
	T.Sections.RemoveAll(
	    [](const FVoxelPreparedSection& S)
	    {
		    return S.Patch.Blocks.IsEmpty() && S.Patch.Entities.IsEmpty();
	    });
	uint64 WireBytes = 17;
	for (auto& N : T.Sections)
	{
		N.Overlay.Revision = N.Patch.ToRevision;
		if (!ValidateOverlay(N.Overlay, N.Blocks))
		{
			Error = TEXT("Invalid or oversized resulting section");
			return false;
		}
		WireBytes += 4 + FVoxelDeltaCodec::MaxEncodedBytes(*Registry, N.Overlay);
		if (WireBytes > FVoxelDeltaCodec::MaxAtomicBatchWireBytes)
		{
			Error = TEXT("Atomic edit exceeds the full-snapshot byte budget");
			return false;
		}
	}
	O = MoveTemp(T);
	Error.Reset();
	return true;
}
bool FVoxelWorldRuntime::CommitEdit(FVoxelPreparedEdit&& T, FVoxelEditBatch& O)
{
	check(IsInGameThread());
	if (!bAuthority)
		return false;
	for (const auto& N : T.Sections)
		if (!IsCurrent(N.Before, false))
			return false;
	FVoxelEditBatch B;
	B.TransactionId = T.TransactionId;
	for (auto& N : T.Sections)
	{
		auto* S = Find(N.Before.Key);
		S->Blocks = MoveTemp(N.Blocks);
		S->Overlay = MoveTemp(N.Overlay);
		S->Stamp.Revision = N.Patch.ToRevision;
		B.Sections.Add(MoveTemp(N.Patch));
	}
	for (const auto& P : B.Sections)
		InvalidateEdit(P);
	O = MoveTemp(B);
	return true;
}
bool FVoxelWorldRuntime::ApplyRemoteSnapshots(const TArray<FVoxelSectionOverlay>& O, TArray<FVoxelSectionStorage>&& Bases)
{
	check(IsInGameThread());
	if (bAuthority || O.Num() != Bases.Num() || O.Num() > 32)
		return false;
	TSet<FVoxelSectionKey> Seen;
	TArray<TSharedPtr<const FVoxelSectionStorage, ESPMode::ThreadSafe>> Natural;
	Natural.Reserve(Bases.Num());
	for (int32 I = 0; I < O.Num(); ++I)
	{
		auto* S = Find(O[I].Key);
		if (!S || Seen.Contains(O[I].Key) || O[I].Revision < S->Stamp.Revision)
			return false;
		Seen.Add(O[I].Key);
		Natural.Add(MakeShared<FVoxelSectionStorage, ESPMode::ThreadSafe>(Bases[I]));
		for (const auto& P : O[I].Blocks)
		{
			if (P.Key >= 4096 || !Registry->IsValid(P.Value))
				return false;
			Bases[I].Set(P.Key, P.Value);
		}
		if (!ValidateOverlay(O[I], Bases[I]))
			return false;
	}
	for (int32 I = 0; I < O.Num(); ++I)
	{
		auto* S = Find(O[I].Key);
		S->BaseBlocks = Natural[I];
		S->Blocks = MoveTemp(Bases[I]);
		S->Overlay = O[I];
		S->Stamp.Revision = O[I].Revision;
		S->Status = EVoxelSectionStatus::DataReady;
	}
	for (const auto& P : O)
		Invalidate(P.Key, true);
	return true;
}
bool FVoxelWorldRuntime::Remove(const FVoxelSectionKey& K, bool Discard)
{
	check(IsInGameThread());
	auto* S = Find(K);
	if (!S)
		return true;
	if (S->PinCount || (!Discard && bAuthority && S->IsSaveDirty()))
		return false;
	S->Status = EVoxelSectionStatus::Unloading;
	Sections.Remove(K);
	for (uint8 F = 0; F < 6; ++F)
		Invalidate(VoxelCoord::Neighbor(K, F), false);
	return true;
}
void FVoxelWorldRuntime::MarkCommitted(const FVoxelSectionKey& K, uint64 R)
{
	if (auto* S = Find(K))
		if (R <= S->Stamp.Revision)
			S->CommittedRevision = FMath::Max(S->CommittedRevision, R);
}
void FVoxelWorldRuntime::MarkMeshApplied(const FVoxelTaskStamp& T)
{
	if (IsCurrent(T))
		Find(T.Key)->bMeshDirty = false;
}
void FVoxelWorldRuntime::MarkCollisionApplied(const FVoxelTaskStamp& T)
{
	if (IsCurrent(T))
	{
		Find(T.Key)->bCollisionDirty = false;
		Find(T.Key)->bHasCollision = true;
	}
}
