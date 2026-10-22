#include "Voxel/Geometry/VoxelSectionMesher.h"
#include "Voxel/Geometry/DWVoxelFaceVisibility.h"
#include "Voxel/Geometry/DWVoxelBoundaryTransition.h"
#include "Voxel/Chunks/VoxelSectionKey.h"
#include "Voxel/Rendering/VoxelBoundaryRaster.h"
#include "Voxel/Rendering/VoxelCoveragePartition.h"
#include "Voxel/Rendering/VoxelProxyBuilder.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"
#include "Voxel/Generation/VoxelGenerationAllocation.h"

namespace
{
    thread_local bool CountSectionMesh = false;
    thread_local bool SectionCapacityValid = true;

	int32 TransitionClipCapacity(const FVoxelBoundaryTransitionContext* Context, const FIntVector& Offset, const int32 Side)
	{
		if (!Context) return 0;
		const int32 Step = Context->GetSampleStep();
		const FIntVector WindowMin = Context->Owner.GetBounds().Min + Offset * Step;
		const FIntVector WindowMax = WindowMin + FIntVector(Side * Step);
		int64 MaximumCuts = 0;
		for (uint8 Face = 0; Face < 6; ++Face)
		{
			const int32 Axis = Face / 2, U = (Axis + 1) % 3, V = (Axis + 2) % 3;
			int64 Cuts = 0;
			const auto Count = [&](const uint8 Direction, const int32 Plane, const FIntVector& Min, const FIntVector& Max)
			{
				if (Direction == Face && Plane >= WindowMin[Axis] && Plane <= WindowMax[Axis] &&
					Min[U] < WindowMax[U] && Max[U] > WindowMin[U] && Min[V] < WindowMax[V] && Max[V] > WindowMin[V]) ++Cuts;
			};
			for (const auto& Suppression : Context->SuppressedFaces)
				Count(uint8(Suppression.Direction), Suppression.Min[Axis], Suppression.Min, Suppression.Max);
			for (const auto& Patch : Context->Patches)
			{
				const auto Bounds = Patch.Neighbor.Key.GetBounds();
				Count(uint8(Patch.Face.Direction), (Face & 1) ? Bounds.Max[Axis] : Bounds.Min[Axis], Patch.Face.Min, Patch.Face.Max);
			}
			MaximumCuts = FMath::Max(MaximumCuts, Cuts);
		}
		// 所有矩形端点构成至多 (2N+1)^2 个互不相交区间，覆盖任意重叠裁切顺序。
		if (MaximumCuts > 23169) return -1;
		return int32((2 * MaximumCuts + 1) * (2 * MaximumCuts + 1));
	}
}

bool FVoxelSectionMesher::IsKnownEmpty(const FVoxelSectionSnapshot& InSnapshot,
	const FVoxelRegistrySnapshot& InRegistry, const FVoxelBoundaryTransitionContext* InTransition)
{
	if (InSnapshot.Blocks.Num() != InSnapshot.GridSide * InSnapshot.GridSide * InSnapshot.GridSide) return false;
	if (InTransition && !InTransition->Validate()) return false;
	auto IsOpaqueCube = [&InRegistry](const uint32 Packed)
	{
		const FVoxelBlockState State = FVoxelBlockState::Unpack(Packed);
		const FVoxelRuntimeDefinition* Definition = InRegistry.Find(State.TypeId);
		return !State.IsAir() && Definition && Definition->Shape == EVoxelShapeKind::FullCube && Definition->bOccludes;
	};
	if (FVoxelBlockState::Unpack(InSnapshot.Blocks[0]).IsAir())
	{
		for (const uint32 Packed : InSnapshot.Blocks)
		{
			if (!FVoxelBlockState::Unpack(Packed).IsAir()) return false;
		}
		if (InTransition)
			for (const auto& Patch : InTransition->Patches)
				for (int32 Index = 0; Index < Patch.Neighbor.GetStateCount(); ++Index)
				{
					uint32 Packed = 0;
					if (!Patch.Neighbor.GetPackedState(Index, Packed) || !FVoxelBlockState::Unpack(Packed).IsAir()) return false;
				}
		return true;
	}
	for (const uint32 Packed : InSnapshot.Blocks)
	{
		if (!IsOpaqueCube(Packed)) return false;
	}
	for (int32 Face = 0; Face < 6; ++Face)
	{
		if (!InSnapshot.Known[Face] || InSnapshot.Halo[Face].Num() != InSnapshot.GridSide * InSnapshot.GridSide) return false;
		for (const uint32 Packed : InSnapshot.Halo[Face])
		{
			if (!IsOpaqueCube(Packed)) return false;
		}
	}
	if (InTransition)
	{
		for (const FVoxelBoundaryTransitionPatch& Patch : InTransition->Patches)
		{
			for (int32 Index = 0; Index < Patch.Neighbor.GetStateCount(); ++Index)
			{
				uint32 Packed = 0;
				if (!Patch.Neighbor.GetPackedState(Index, Packed) || !IsOpaqueCube(Packed)) return false;
			}
		}
	}
	return true;
}

uint64 FVoxelMeshBuffers::Bytes()const
{return uint64(Vertices.Num())*sizeof(FVector)+Normals.Bytes()+uint64(Triangles.Num())*4+
    uint64(UV0.Num())*sizeof(FVector2D)+UV1.Bytes()+UV2.Bytes()+Colors.Bytes()+Tangents.Bytes();}
bool FVoxelMeshBuffers::CompactAttributes(const uint64 InWorkingBytes, const TAtomic<bool>* InCancel)
{
    return Normals.Compact(InWorkingBytes, InCancel) && UV1.Compact(InWorkingBytes, InCancel) &&
        UV2.Compact(InWorkingBytes, InCancel) && Colors.Compact(InWorkingBytes, InCancel) &&
        Tangents.Compact(InWorkingBytes, InCancel);
}

bool FVoxelSectionMeshResult::CompactAttributes(const uint64 InWorkingBytes, const TAtomic<bool>* InCancel)
{
    check(bValidated);
    bool bComplete = true;
    for (auto& Batch : Batches)
    {
        if (!Batch.Mesh.CompactAttributes(InWorkingBytes, InCancel)) { bComplete = false; break; }
    }
    MeshDataBytes = 0;
    for (const auto& Batch : Batches) MeshDataBytes += Batch.Mesh.Bytes();
    return bComplete;
}

bool FVoxelSectionMeshResult::TryCompactAttributes(const TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe>& InResources,
    const FVoxelGenerationConsumerKey& InConsumer, FString& OutError, const TAtomic<bool>* InCancel)
{
    TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_MeshAttributeStorage);
    OutError.Reset();
    if (!bValidated || !ResourceLease || !ResourceLease->IsOwnedBy(InResources.Get()))
    {
        OutError = TEXT("Invalid mesh attribute storage owner or validation");
        return false;
    }
    if (InCancel && InCancel->Load()) { OutError = TEXT("Canceled"); return false; }
    uint64 ScratchBytes = 0;
    for (const auto& Batch : Batches)
    {
        const int32 Count = Batch.Mesh.Vertices.Num();
        if (Count < 64) continue;
        ScratchBytes = FMath::Max(ScratchBytes, VoxelBoundedAllocation::ArrayBytes<uint8>(Count) +
            VoxelBoundedAllocation::ArrayBytes<FProcMeshTangent>(FMath::Min(256, Count)));
    }
    if (!ScratchBytes) return true;
    FVoxelGenerationRequiredSourceScope OutputSource(&InResources.Get(), ResourceLease.Get());
    FVoxelGenerationRequiredCapacity Required;
    if (!FVoxelGenerationRequiredSourceScope::Collect(InResources.Get(), nullptr, Required, OutError)) return false;
    TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ScratchLease;
    const auto Admission = InResources->TryAcquire(InConsumer, EVoxelTerrainResourcePool::Working, ScratchBytes,
        ScratchLease, 0, Required.Bytes[int32(EVoxelTerrainResourcePool::Input)], Required.LeaseCount);
    if (Admission == EVoxelTerrainResourceAdmission::CapacityBlocked) return true;
    if (Admission != EVoxelTerrainResourceAdmission::Granted)
    {
        OutError = TEXT("Invalid mesh attribute storage scratch admission");
        return false;
    }
    const uint64 Before = AllocatedBytes();
    if (!CompactAttributes(ScratchLease->Bytes(), InCancel)) { OutError = TEXT("Canceled"); return false; }
    const uint64 After = AllocatedBytes();
    check(After <= Before);
#if !UE_BUILD_SHIPPING
    if (Before - After >= 1024 * 1024)
        UE_LOG(LogTemp, Display, TEXT("Voxel mesh attribute storage: vertices=%llu before=%llu after=%llu scratch=%llu"),
            VertexCount, Before, After, ScratchBytes);
#endif
    return true;
}

bool FVoxelMeshBuffers::Validate()const
{
    int32 N=Vertices.Num();if(N!=Normals.Num()||N!=UV0.Num()||N!=UV1.Num()||N!=UV2.Num()||N!=Colors.Num()||N!=Tangents.Num()||Triangles.Num()%3)return false;
    for(int32 I:Triangles)if(I<0||I>=N)return false;
    for(int32 I=0;I<N;++I)if(Vertices[I].ContainsNaN()||Normals[I].ContainsNaN()||!FMath::IsFinite(UV0[I].X)||!FMath::IsFinite(UV0[I].Y)||
        !FMath::IsFinite(UV1[I].X)||!FMath::IsFinite(UV1[I].Y)||!FMath::IsFinite(UV2[I].X))return false;
    return true;
}
uint64 FVoxelMeshBuffers::AllocatedBytes() const
{
    return Vertices.GetAllocatedSize() + Normals.GetAllocatedSize() + Triangles.GetAllocatedSize() +
        UV0.GetAllocatedSize() + UV1.GetAllocatedSize() + UV2.GetAllocatedSize() +
        Colors.GetAllocatedSize() + Tangents.GetAllocatedSize();
}
uint64 FVoxelMeshBuffers::PlannedBytes() const
{
    using namespace VoxelBoundedAllocation;
    return 2 * ArrayBytes<FVector>(PlannedVertices) + 3 * ArrayBytes<FVector2D>(PlannedVertices) +
        ArrayBytes<FLinearColor>(PlannedVertices) + ArrayBytes<FProcMeshTangent>(PlannedVertices) +
        ArrayBytes<int32>(PlannedIndices);
}
void FVoxelMeshBuffers::ReservePlan(const FVoxelMeshBuffers& InPlan)
{
    PlannedVertices = InPlan.PlannedVertices;
    PlannedIndices = InPlan.PlannedIndices;
    Vertices.Reserve(PlannedVertices); Normals.Reserve(PlannedVertices);
    UV0.Reserve(PlannedVertices); UV1.Reserve(PlannedVertices); UV2.Reserve(PlannedVertices);
    Colors.Reserve(PlannedVertices); Tangents.Reserve(PlannedVertices); Triangles.Reserve(PlannedIndices);
}
uint64 FVoxelSectionMeshResult::Bytes()const{if(bValidated)return MeshDataBytes;uint64 N=0;for(const auto&B:Batches)N+=B.Mesh.Bytes();return N;}
uint64 FVoxelSectionMeshResult::AllocatedBytes() const
{
    uint64 Bytes = Batches.GetAllocatedSize();
    for (const auto& Batch : Batches) Bytes += Batch.Mesh.AllocatedBytes();
    return Bytes;
}
uint64 FVoxelSectionMeshResult::PlannedBytes() const
{
    uint64 Bytes = VoxelBoundedAllocation::ArrayBytes<FVoxelRenderBatch>(Batches.Num());
    for (const auto& Batch : Batches) Bytes += Batch.Mesh.PlannedBytes();
    return Bytes;
}
bool FVoxelSectionMeshResult::ValidateAndCache()
{
	LocalBounds = FBox(ForceInit);
	bValidated = false;
	VertexCount = TriangleCount = MeshDataBytes = 0;
	for (const auto& Batch : Batches)
	{
		if (!Batch.Mesh.Validate()) return false;
		VertexCount += Batch.Mesh.Vertices.Num();
		TriangleCount += Batch.Mesh.Triangles.Num() / 3;
		MeshDataBytes += Batch.Mesh.Bytes();
		for (const auto& Vertex : Batch.Mesh.Vertices) LocalBounds += Vertex;
	}
	bValidated = true;
	return true;
}
namespace
{
struct FFaceKey
{
    bool Visible=false;uint32 Packed=0;FVoxelRuntimeFaceRef Texture;EVoxelRenderGroup Group=EVoxelRenderGroup::Opaque;
    bool operator==(const FFaceKey&B)const{return Visible==B.Visible&&Packed==B.Packed&&Texture==B.Texture&&Group==B.Group;}
};
FVector2D MakeFaceUV(
uint8 Face,
const FVector& Position,
double VerticalExtent)
{
	if(Face < 2)
	{
		return FVector2D(
			Position.Y,
			VerticalExtent - Position.Z
		);
	}

	if(Face < 4)
	{
		return FVector2D(
			Position.X,
			VerticalExtent - Position.Z
		);
	}

	return FVector2D(
		Position.X,
		Position.Y
	);
}
bool Visible(const FVoxelSectionSnapshot&S,const FVoxelRegistrySnapshot&R,const FVoxelShapeRegistry&H,
    FIntVector P,FVoxelBlockState A,uint8 Face,const FVoxelShapeQuad*Quad)
{
    FVoxelBlockState B;
    if (!S.TrySample(P + VoxelCoord::Direction(Face), B))
    {
        return false;
    }
    return VoxelFaceVisibility::ShouldRender(R, H, A, B, Face, Quad);
}
void AppendQuad(FVoxelMeshBuffers&O,const FVector*P,const FVector2D*UV,const FVoxelRuntimeFaceRef&T,EVoxelRenderGroup G,const FVector&Cell,bool bPlantWind=false)
{
    if (!SectionCapacityValid) return;
    if (CountSectionMesh) { O.PlannedVertices += 4; O.PlannedIndices += 6; return; }
    if (O.PlannedVertices > 0)
    {
        if (O.Vertices.Num() + 4 > O.PlannedVertices || O.Triangles.Num() + 6 > O.PlannedIndices)
        { SectionCapacityValid = false; return; }
    }
    int32 N=O.Vertices.Num();FVector Normal=FVector::CrossProduct(P[1]-P[0],P[2]-P[0]).GetSafeNormal();FVector Tangent=(P[1]-P[0]).GetSafeNormal();
    for(int32 I=0;I<4;++I)
    {
        O.Vertices.Add(P[I]+Cell);O.Normals.Add(Normal);O.UV0.Add(UV[I]);O.UV1.Add(FVector2D(T.Layer,T.Frames));O.UV2.Add(FVector2D(T.FPS,0));
        const float Wind=bPlantWind&&G==EVoxelRenderGroup::Foliage?float(FMath::Clamp(P[I].Z,0.0,1.0)):0.f;
        O.Colors.Add(G==EVoxelRenderGroup::Opaque?T.Tint:FLinearColor(Wind,0,1,1));O.Tangents.Add(FProcMeshTangent(Tangent,false));
    }
	O.Triangles.Append({N, N + 2, N + 1, N, N + 3, N + 2});}
}
uint64 FVoxelSectionMesher::FaceMaskBytes(const int32 InSide)
{
    return VoxelBoundedAllocation::ArrayBytes<FFaceKey>(int64(InSide) * InSide);
}
uint64 FVoxelSectionMesher::TransitionClipScratchBytes(const FVoxelBoundaryTransitionContext* InTransition,
	const FIntVector& InLocalCellOffset, const int32 InSide)
{
	const int32 Capacity = TransitionClipCapacity(InTransition, InLocalCellOffset, InSide);
	return Capacity < 0 ? MAX_uint64 : 2 * VoxelBoundedAllocation::ArrayBytes<FBox2D>(Capacity);
}
bool FVoxelSectionMesher::Build(const FVoxelSectionSnapshot&S,const FVoxelRegistrySnapshot&R,const FVoxelShapeRegistry&H,
    FVoxelSectionMeshResult&O,const TAtomic<bool>*Cancel, const double InTextureRepeatsPerCell,
    const FVoxelBoundaryTransitionContext* InTransition, const bool bBoundaryOnly, const bool bIncludeBoundary, const FIntVector& InLocalCellOffset, const int32 InTextureGridSide,
    const bool bCountOnly, const FVoxelSectionMeshResult* InCapacityPlan)
{
    TGuardValue<bool> CountGuard(CountSectionMesh, bCountOnly);
    TGuardValue<bool> CapacityGuard(SectionCapacityValid, true);
    const int32 Side = S.GridSide;
    const bool bFrozenProxyBoundary = bBoundaryOnly && InTransition && InTransition->OwnerGeometry &&
        InTransition->OwnerGeometry->GridSide == Side;
    const bool bFrozenFineBoundary = bBoundaryOnly && InTransition && InTransition->FineOwnerSource.Boundary && Side == 16;
    if (!bFrozenProxyBoundary && !bFrozenFineBoundary && S.Blocks.Num() != Side * Side * Side) return false;
    for(uint32 P:S.Blocks)if(!R.IsValid(FVoxelBlockState::Unpack(P)))return false;
    if(InTransition && (!InTransition->Validate() ||
        (bIncludeBoundary && InTransition->GetGridSide() != Side))) return false;
	TArray<FBox2D> ClipRemaining, ClipNext;
	if (InTransition && !bBoundaryOnly)
	{
		const int32 Capacity = TransitionClipCapacity(InTransition, InLocalCellOffset, Side);
		if (Capacity < 0) return false;
		ClipRemaining.Reserve(Capacity); ClipNext.Reserve(Capacity);
	}
    FVoxelSectionMeshResult T;
    T.Stamp.WorldEpoch = S.Stamp.Epoch;
    T.Stamp.Token = S.Stamp.Token;
    T.Stamp.Revision = S.Revision;
    T.Stamp.Section = S.Section;
    TMap<uint32,int32>Groups;int32 TotalVertices=0;
    FVoxelMeshBuffers RejectedBatch;
    if (bCountOnly)
    {
        const int32 MaximumBatches = int32(FMath::Min(int64(6) * Side * Side * Side, int64(12) * R.Definitions.Num()));
        T.Batches.Reserve(MaximumBatches);
        Groups.Reserve(MaximumBatches);
    }
    if (InCapacityPlan)
    {
        T.Batches.Reserve(InCapacityPlan->Batches.Num());
        Groups.Reserve(InCapacityPlan->Batches.Num());
        for (const auto& Planned : InCapacityPlan->Batches)
        {
            auto& Batch = T.Batches.AddDefaulted_GetRef();
            Batch.Group = Planned.Group; Batch.Bank = Planned.Bank; Batch.MaterialVariant = Planned.MaterialVariant;
            Batch.Mesh.ReservePlan(Planned.Mesh);
            Groups.Add((uint32(Batch.Group) << 16) | Batch.Bank, T.Batches.Num() - 1);
        }
    }
    auto Batch=[&](EVoxelRenderGroup G,uint16 Bank)->FVoxelMeshBuffers&
    {
        uint32 K=(uint32(G)<<16)|Bank;if(const int32*I=Groups.Find(K))return T.Batches[*I].Mesh;
        if (InCapacityPlan) { SectionCapacityValid = false; return RejectedBatch; }
        FVoxelRenderBatch B;B.Group=G;B.Bank=Bank;int32 I=T.Batches.Add(MoveTemp(B));Groups.Add(K,I);return T.Batches[I].Mesh;
    };
    // Count 不分配顶点；由调用方依据完整计划拆分 Piece，不能先用旧固定顶点阈值截断计数。
    auto OverBudget=[&](){return !bCountOnly && TotalVertices>262144;};
    // Clip only the physical interface rectangle. A finer neighbor may cover part
    // of a coarse sample face; discarding the whole cell would open a hole.
    const auto AppendVisibleQuad = [&](const uint8 Face, const FVector* Vertices,
        const FVector2D* Coordinates, const FFaceKey& Material)
    {
        if (!InTransition)
        {
            AppendQuad(Batch(Material.Group, Material.Texture.Bank), Vertices, Coordinates,
                Material.Texture, Material.Group, FVector::ZeroVector);
            TotalVertices += 4;
            return;
        }
        const int32 Axis = Face / 2, U = (Axis + 1) % 3, V = (Axis + 2) % 3;
        const int32 Step = InTransition->GetSampleStep();
        const FIntVector Origin = InTransition->Owner.GetBounds().Min + InLocalCellOffset * Step;
        const double Plane = Origin[Axis] + Vertices[0][Axis] * Step;
        FVector LocalMin = Vertices[0], LocalMax = Vertices[0];
        for (int32 Index = 1; Index < 4; ++Index)
            for (int32 Coordinate = 0; Coordinate < 3; ++Coordinate)
            { LocalMin[Coordinate] = FMath::Min(LocalMin[Coordinate], Vertices[Index][Coordinate]);
              LocalMax[Coordinate] = FMath::Max(LocalMax[Coordinate], Vertices[Index][Coordinate]); }
		auto& Remaining = ClipRemaining;
		Remaining.Reset();
        Remaining.Add(FBox2D(FVector2D(Origin[U] + LocalMin[U] * Step, Origin[V] + LocalMin[V] * Step),
            FVector2D(Origin[U] + LocalMax[U] * Step, Origin[V] + LocalMax[V] * Step)));
        const auto Cut = [&](const FIntVector& Min, const FIntVector& Max)
        {
			auto& Next = ClipNext;
			Next.Reset();
            for (const auto& Piece : Remaining)
            {
                const FVector2D Low(FMath::Max(Piece.Min.X, double(Min[U])), FMath::Max(Piece.Min.Y, double(Min[V])));
                const FVector2D High(FMath::Min(Piece.Max.X, double(Max[U])), FMath::Min(Piece.Max.Y, double(Max[V])));
                if (Low.X >= High.X || Low.Y >= High.Y) { Next.Add(Piece); continue; }
                if (Piece.Min.X < Low.X) Next.Add(FBox2D(Piece.Min, FVector2D(Low.X, Piece.Max.Y)));
                if (High.X < Piece.Max.X) Next.Add(FBox2D(FVector2D(High.X, Piece.Min.Y), Piece.Max));
                if (Piece.Min.Y < Low.Y) Next.Add(FBox2D(FVector2D(Low.X, Piece.Min.Y), FVector2D(High.X, Low.Y)));
                if (High.Y < Piece.Max.Y) Next.Add(FBox2D(FVector2D(Low.X, High.Y), FVector2D(High.X, Piece.Max.Y)));
            }
			Swap(Remaining, Next);
        };
        for (const auto& Suppression : InTransition->SuppressedFaces)
            if (static_cast<uint8>(Suppression.Direction) == Face && FMath::IsNearlyEqual(double(Suppression.Min[Axis]), Plane))
                Cut(Suppression.Min, Suppression.Max);
        for (const auto& Patch : InTransition->Patches)
        {
            const auto Bounds = Patch.Neighbor.Key.GetBounds();
            const int32 PatchPlane = (Face & 1) ? Bounds.Max[Axis] : Bounds.Min[Axis];
            if (static_cast<uint8>(Patch.Face.Direction) == Face && FMath::IsNearlyEqual(double(PatchPlane), Plane))
                Cut(Patch.Face.Min, Patch.Face.Max);
        }
        for (const auto& Piece : Remaining)
        {
            FVector Points[4]; FVector2D UV[4];
            const double X[4] = {Piece.Min.X, Piece.Max.X, Piece.Max.X, Piece.Min.X};
            const double Y[4] = {Piece.Min.Y, Piece.Min.Y, Piece.Max.Y, Piece.Max.Y};
            for (int32 Index = 0; Index < 4; ++Index)
            {
                Points[Index] = FVector::ZeroVector;
                Points[Index][Axis] = Vertices[0][Axis];
                Points[Index][U] = double(X[Index] - Origin[U]) / Step;
                Points[Index][V] = double(Y[Index] - Origin[V]) / Step;
                UV[Index] = MakeFaceUV(Face, Points[Index] + FVector(InLocalCellOffset), double(InTextureGridSide > 0 ? InTextureGridSide : Side)) * InTextureRepeatsPerCell;
            }
            if (Face & 1) { Swap(Points[1], Points[3]); Swap(UV[1], UV[3]); }
            AppendQuad(Batch(Material.Group, Material.Texture.Bank), Points, UV,
                Material.Texture, Material.Group, FVector::ZeroVector);
            TotalVertices += 4;
        }
    };

    if (!bBoundaryOnly) for(uint8 F=0;F<6;++F)for(int32 Slice=0;Slice<Side;++Slice)
    {
        if(Cancel&&Cancel->Load())return false;
        TArray<FFaceKey> Mask; Mask.Reserve(Side * Side); Mask.SetNum(Side * Side);int32 A=F/2,U=(A+1)%3,V=(A+2)%3;
        for(int32 Y=0;Y<Side;++Y)for(int32 X=0;X<Side;++X)
        {
            FIntVector P(0,0,0);P[A]=Slice;P[U]=X;P[V]=Y;FVoxelBlockState B=FVoxelBlockState::Unpack(S.Blocks[P.X + Side * (P.Y + Side * P.Z)]);
            if(B.IsAir())continue;const auto*D=R.Find(B.TypeId);if(D->Shape!=EVoxelShapeKind::FullCube||
                (InTransition && InTransition->CoversCell(F,P + InLocalCellOffset))||!Visible(S,R,H,P,B,F,nullptr))continue;
            FFaceKey&K=Mask[X+Side*Y];K.Visible=true;K.Packed=B.Pack();K.Group=D->RenderGroup;
            uint8 MF=FVoxelShapeRegistry::RotateFace(F,uint8((4-(B.State&3))&3));K.Texture=D->Face(B.State,MF);
        }
        for(int32 Y=0;Y<Side;++Y)for(int32 X=0;X<Side;)
        {
            const auto K=Mask[X+Side*Y];if(!K.Visible){++X;continue;}int32 W=1,Ht=1;
            while(X+W<Side&&Mask[X+W+Side*Y]==K)++W;
            bool Stop=false;while(Y+Ht<Side&&!Stop){for(int32 DX=0;DX<W;++DX)if(!(Mask[X+DX+Side*(Y+Ht)]==K)){Stop=true;break;}if(!Stop)++Ht;}
        	FVector P[4];
        	FVector2D UV[4];
            const int32 XX[4]={X,X+W,X+W,X},YY[4]={Y,Y,Y+Ht,Y+Ht};
            for(int32 I=0;I<4;++I)
            {
	            P[I]=FVector::ZeroVector;P[I][A]=Slice+(F%2==0?1:0);P[I][U]=XX[I];P[I][V]=YY[I];
	            UV[I] = MakeFaceUV(F, P[I] + FVector(InLocalCellOffset), double(InTextureGridSide > 0 ? InTextureGridSide : Side));
            }
            for (FVector2D& Coordinate : UV)
            {
                Coordinate *= InTextureRepeatsPerCell;
            }
            if(F&1){Swap(P[1],P[3]);Swap(UV[1],UV[3]);}
            AppendVisibleQuad(F, P, UV, K);
            if(OverBudget())return false;
            for(int32 DY=0;DY<Ht;++DY)for(int32 DX=0;DX<W;++DX)Mask[X+DX+Side*(Y+DY)].Visible=false;X+=W;
        }
    }
    if (InTransition && bIncludeBoundary)
    {
        const auto OwnerBounds = InTransition->Owner.GetBounds();
        const int32 OwnerStep = InTransition->GetSampleStep();
        for (const auto& Patch : InTransition->Patches)
        {
            if (Cancel && Cancel->Load()) return false;
            const uint8 Face = static_cast<uint8>(Patch.Face.Direction);
            const int32 Axis = Face / 2;
            // BoundaryRaster uses the same world axes for both sides of the interface.
            const int32 UAxis = Axis == 0 ? 1 : 0;
            const int32 VAxis = Axis == 2 ? 1 : 2;
            const auto NeighborBounds = Patch.Neighbor.Key.GetBounds();
            const int32 Plane = (Face & 1) ? NeighborBounds.Max[Axis] : NeighborBounds.Min[Axis];
            const int32 OwnerAxis = FMath::Clamp(((Face & 1) ? Plane : Plane - 1) - OwnerBounds.Min[Axis], 0, InTransition->Owner.GetSide() - 1) / OwnerStep;
            const int32 NeighborStep = Patch.Neighbor.SampleStepCells;
            if (OwnerStep <= 0 || NeighborStep <= 0) return false;
            const int32 FirstU = FMath::Max(OwnerBounds.Min[UAxis], Patch.Face.Min[UAxis]);
            const int32 FirstV = FMath::Max(OwnerBounds.Min[VAxis], Patch.Face.Min[VAxis]);
            const int32 LastU = FMath::Min(OwnerBounds.Max[UAxis], Patch.Face.Max[UAxis]);
            const int32 LastV = FMath::Min(OwnerBounds.Max[VAxis], Patch.Face.Max[VAxis]);
            const int32 TileStep = FMath::Min(OwnerStep, NeighborStep);
            for (int32 V = FirstV; V < LastV; V += TileStep)
                for (int32 U = FirstU; U < LastU; U += TileStep)
                {
                    if (Cancel && Cancel->Load()) return false;
                    FIntVector Local(0), NeighborCell(0);
                    Local[Axis] = OwnerAxis;
                    Local[UAxis] = (U - OwnerBounds.Min[UAxis]) / OwnerStep;
                    Local[VAxis] = (V - OwnerBounds.Min[VAxis]) / OwnerStep;
                    const int32 OwnerIndex = Local.X + Side * (Local.Y + Side * Local.Z);
                    FVoxelBlockState OwnerState;
                    if (bFrozenFineBoundary)
                    {
                        if (!InTransition->FineOwnerSource.Boundary->SampleAtomFace(InTransition->Owner.Coordinate,
                            Face, false, OwnerBounds.Min + Local, OwnerState)) return false;
                    }
                    else OwnerState = bFrozenProxyBoundary ? InTransition->OwnerGeometry->Cells[OwnerIndex]
                        : FVoxelBlockState::Unpack(S.Blocks[OwnerIndex]);
                    NeighborCell[Axis] = (Face & 1) ? NeighborBounds.Max[Axis] - 1 : NeighborBounds.Min[Axis];
                    NeighborCell[UAxis] = U; NeighborCell[VAxis] = V;
                    FVoxelBlockState NeighborState;
                    if (!Patch.Neighbor.Sample(NeighborCell, NeighborState)) return false;
                    const FVector OwnerCellMin = FVector(OwnerBounds.Min + Local * OwnerStep);
                    FVector NeighborCellMin(NeighborBounds.Min);
                    for (int32 Coordinate = 0; Coordinate < 3; ++Coordinate)
                        NeighborCellMin[Coordinate] += FMath::FloorToDouble(
                            double(NeighborCell[Coordinate] - NeighborBounds.Min[Coordinate]) / NeighborStep) * NeighborStep;
                    const auto EmitSide = [&](const FVoxelBlockState State, const FVoxelBlockState Other,
                        const FVector& CellMin, const FVector& OtherMin, const int32 StateStep,
                        const int32 OtherStep, const uint8 OutputFace) -> bool
                    {
                        if (State.IsAir()) return true;
                        const auto* Definition = R.Find(State.TypeId);
                        const auto* OtherDefinition = Other.IsAir() ? nullptr : R.Find(Other.TypeId);
                        const auto* Shape = Definition ? H.Find(Definition->Shape, State.State) : nullptr;
                        const auto* OtherShape = OtherDefinition ? H.Find(OtherDefinition->Shape, Other.State) : nullptr;
                        if (!Definition || !Shape || (!Other.IsAir() && (!OtherDefinition || !OtherShape))) return false;
                        if (!Shape->Quads.ContainsByPredicate([OutputFace](const FVoxelShapeQuad& Quad)
                            { return Quad.bBoundary && Quad.Face == OutputFace; })) return true;
                        const bool bOtherOccludes = OtherDefinition && (OtherDefinition->bOccludes ||
                            (State.TypeId == Other.TypeId && (Definition->RenderGroup == EVoxelRenderGroup::Water ||
                                Definition->RenderGroup == EVoxelRenderGroup::Translucent)));
                        if (OtherDefinition && OtherDefinition->Shape == EVoxelShapeKind::FullCube &&
                            !VoxelFaceVisibility::ShouldRender(R, H, State, Other, OutputFace, nullptr)) return true;
                        // 形状的规范分辨率是 1/16 cell。两侧使用实际 sample 的形状截面，不能把状态合并成 filled。
                        bool Mask[256] = {};
                        const auto Contains = [](const FVoxelResolvedShape& Resolved, const FVector& Point)
                        {
                            for (const FBox& Box : Resolved.SelectionBoxes) if (Box.IsInside(Point)) return true;
                            return false;
                        };
                        const double ExtentU = FMath::Min(U + TileStep, LastU) - U;
                        const double ExtentV = FMath::Min(V + TileStep, LastV) - V;
                        // 单个凸盒同时包含栅格首尾采样点时，整个 16x16 mask 必为可见。
                        // 对侧不遮挡才使用此等价路径；部分形状及遮挡仍走原规范栅格。
                        bool bUniformVisible = false;
                        if (!bOtherOccludes && Shape->SelectionBoxes.Num() == 1)
                        {
                            FVector FirstPoint(0.0), LastPoint(0.0);
                            FirstPoint[Axis] = LastPoint[Axis] = Plane + ((OutputFace & 1) ? 1.e-6 : -1.e-6);
                            FirstPoint[UAxis] = U + .5 * ExtentU / 16;
                            FirstPoint[VAxis] = V + .5 * ExtentV / 16;
                            LastPoint[UAxis] = U + 15.5 * ExtentU / 16;
                            LastPoint[VAxis] = V + 15.5 * ExtentV / 16;
                            const FBox& Box = Shape->SelectionBoxes[0];
                            bUniformVisible = Box.IsInside((FirstPoint - CellMin) / StateStep) &&
                                Box.IsInside((LastPoint - CellMin) / StateStep);
                        }
                        if (!bUniformVisible) for (int32 Y = 0; Y < 16; ++Y)
                            for (int32 X = 0; X < 16; ++X)
                            {
                                FVector Point(0.0);
                                Point[Axis] = Plane + ((OutputFace & 1) ? 1.e-6 : -1.e-6);
                                Point[UAxis] = U + (X + .5) * ExtentU / 16;
                                Point[VAxis] = V + (Y + .5) * ExtentV / 16;
                                if (!Contains(*Shape, (Point - CellMin) / StateStep)) continue;
                                Point[Axis] = Plane + ((OutputFace & 1) ? -1.e-6 : 1.e-6);
                                Mask[X + Y * 16] = !bOtherOccludes || !Contains(*OtherShape, (Point - OtherMin) / OtherStep);
                            }
                        const uint8 MaterialFace = FVoxelShapeRegistry::RotateFace(OutputFace, uint8((4 - (State.State & 3)) & 3));
                        const auto Texture = Definition->Face(State.State, MaterialFace);
                        const auto EmitRectangle = [&](const int32 X, const int32 Y, const int32 Width, const int32 Height)
                        {
                            FVector Points[4]; FVector2D UV[4];
                            const double Us[4] = {double(X), double(X + Width), double(X + Width), double(X)};
                            const double Vs[4] = {double(Y), double(Y), double(Y + Height), double(Y + Height)};
                            for (int32 Vertex = 0; Vertex < 4; ++Vertex)
                            {
                                FVector WorldPoint(0.0);
                                WorldPoint[Axis] = Plane;
                                WorldPoint[UAxis] = U + Us[Vertex] * ExtentU / 16;
                                WorldPoint[VAxis] = V + Vs[Vertex] * ExtentV / 16;
                                Points[Vertex] = (WorldPoint - FVector(OwnerBounds.Min)) / OwnerStep;
                                const double Period = FMath::Clamp(double(StateStep), 1.0,
                                    InTransition->MaximumTextureStretchCells);
                                UV[Vertex] = MakeFaceUV(OutputFace, Points[Vertex], Side) * (OwnerStep / Period);
                            }
                            // U/V 为 BoundaryRaster 轴序，与普通 voxel face 的循环轴序不同。
                            const bool bReverse = (Axis != 1) != ((OutputFace & 1) == 0);
                            if (bReverse) { Swap(Points[1], Points[3]); Swap(UV[1], UV[3]); }
                            AppendQuad(Batch(Definition->RenderGroup, Texture.Bank), Points, UV,
                                Texture, Definition->RenderGroup, FVector::ZeroVector);
                            TotalVertices += 4;
                            if (OverBudget()) return false;
                            return true;
                        };
                        if (bUniformVisible) return EmitRectangle(0, 0, 16, 16);
                        for (int32 Y = 0; Y < 16; ++Y)
                            for (int32 X = 0; X < 16;)
                            {
                                if (!Mask[X + Y * 16]) { ++X; continue; }
                                int32 Width = 1, Height = 1;
                                while (X + Width < 16 && Mask[X + Width + Y * 16]) ++Width;
                                bool bStop = false;
                                while (Y + Height < 16 && !bStop)
                                {
                                    for (int32 Delta = 0; Delta < Width; ++Delta)
                                        if (!Mask[X + Delta + (Y + Height) * 16]) { bStop = true; break; }
                                    if (!bStop) ++Height;
                                }
                                if (!EmitRectangle(X, Y, Width, Height)) return false;
                                for (int32 DY = 0; DY < Height; ++DY)
                                    for (int32 DX = 0; DX < Width; ++DX) Mask[X + DX + (Y + DY) * 16] = false;
                                X += Width;
                            }
                        return true;
                    };
                    if (!EmitSide(OwnerState, NeighborState, OwnerCellMin, NeighborCellMin, OwnerStep, NeighborStep, Face) ||
                        !EmitSide(NeighborState, OwnerState, NeighborCellMin, OwnerCellMin, NeighborStep, OwnerStep, Face ^ 1)) return false;
                }
        }
    }
    if (!bBoundaryOnly) for(int32 I=0;I<Side * Side * Side;++I)
    {
        if((I&63)==0&&Cancel&&Cancel->Load())return false;
        auto B=FVoxelBlockState::Unpack(S.Blocks[I]);if(B.IsAir())continue;const auto*D=R.Find(B.TypeId);
        if(D->Shape==EVoxelShapeKind::FullCube)continue;const auto*Shape=H.Find(D->Shape,B.State);if(!Shape)return false;
        FIntVector P=FIntVector(I % Side, (I / Side) % Side, I / (Side * Side));for(const auto&Q:Shape->Quads)
        {
            if(Q.bBoundary&&!Visible(S,R,H,P,B,Q.Face,&Q))continue;
            const auto& Tex = D->Face(B.State,Q.MaterialFace);
            if (Q.bBoundary && InTransition)
            {
                FVector Points[4]; FVector2D UV[4];
                for (int32 Vertex = 0; Vertex < 4; ++Vertex)
                {
                    Points[Vertex] = Q.Vertices[Vertex] + FVector(P);
                    UV[Vertex] = MakeFaceUV(Q.Face, Points[Vertex] + FVector(InLocalCellOffset),
                        double(InTextureGridSide > 0 ? InTextureGridSide : Side)) * InTextureRepeatsPerCell;
                }
                FFaceKey Material;
                Material.Visible = true; Material.Packed = B.Pack(); Material.Group = D->RenderGroup; Material.Texture = Tex;
                AppendVisibleQuad(Q.Face, Points, UV, Material);
            }
            else
            {
                AppendQuad(Batch(D->RenderGroup,Tex.Bank),Q.Vertices,Q.UV,Tex,D->RenderGroup,FVector(P),D->Shape==EVoxelShapeKind::CrossPlant);
                TotalVertices += 4;
            }
            if(OverBudget())return false;
        }
    }
    if (!SectionCapacityValid) return false;
    T.Batches.Sort([](const FVoxelRenderBatch&A,const FVoxelRenderBatch&B){return A.Group!=B.Group?uint8(A.Group)<uint8(B.Group):A.Bank<B.Bank;});
    for(const auto&B:T.Batches)if(!B.Mesh.Validate())return false;if (!T.ValidateAndCache()) return false; O=MoveTemp(T);return true;
}
