#include "Voxel/Geometry/VoxelSectionMesher.h"
#include "Voxel/Geometry/DWVoxelFaceVisibility.h"
#include "Voxel/Geometry/DWVoxelBoundaryTransition.h"
#include "Voxel/Chunks/VoxelSectionKey.h"
#include "Voxel/Rendering/VoxelBoundaryRaster.h"
#include "Voxel/Rendering/VoxelCoveragePartition.h"

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
				for (const uint32 Packed : Patch.Neighbor.States)
					if (!FVoxelBlockState::Unpack(Packed).IsAir()) return false;
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
			for (const uint32 Packed : Patch.Neighbor.States)
			{
				if (!IsOpaqueCube(Packed)) return false;
			}
		}
	}
	return true;
}

uint64 FVoxelMeshBuffers::Bytes()const
{return uint64(Vertices.Num())*sizeof(FVector)+uint64(Normals.Num())*sizeof(FVector)+uint64(Triangles.Num())*4+
    uint64(UV0.Num()+UV1.Num()+UV2.Num())*sizeof(FVector2D)+uint64(Colors.Num())*sizeof(FLinearColor)+uint64(Tangents.Num())*sizeof(FProcMeshTangent);}
bool FVoxelMeshBuffers::Validate()const
{
    int32 N=Vertices.Num();if(N!=Normals.Num()||N!=UV0.Num()||N!=UV1.Num()||N!=UV2.Num()||N!=Colors.Num()||N!=Tangents.Num()||Triangles.Num()%3)return false;
    for(int32 I:Triangles)if(I<0||I>=N)return false;
    for(int32 I=0;I<N;++I)if(Vertices[I].ContainsNaN()||Normals[I].ContainsNaN()||!FMath::IsFinite(UV0[I].X)||!FMath::IsFinite(UV0[I].Y)||
        !FMath::IsFinite(UV1[I].X)||!FMath::IsFinite(UV1[I].Y)||!FMath::IsFinite(UV2[I].X))return false;
    return true;
}
uint64 FVoxelSectionMeshResult::Bytes()const{uint64 N=0;for(const auto&B:Batches)N+=B.Mesh.Bytes();return N;}
bool FVoxelSectionMeshResult::ValidateAndCache()
{
	LocalBounds = FBox(ForceInit);
	bValidated = false;
	for (const auto& Batch : Batches)
	{
		if (!Batch.Mesh.Validate()) return false;
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
    int32 N=O.Vertices.Num();FVector Normal=FVector::CrossProduct(P[1]-P[0],P[2]-P[0]).GetSafeNormal();FVector Tangent=(P[1]-P[0]).GetSafeNormal();
    for(int32 I=0;I<4;++I)
    {
        O.Vertices.Add(P[I]+Cell);O.Normals.Add(Normal);O.UV0.Add(UV[I]);O.UV1.Add(FVector2D(T.Layer,T.Frames));O.UV2.Add(FVector2D(T.FPS,0));
        const float Wind=bPlantWind&&G==EVoxelRenderGroup::Foliage?float(FMath::Clamp(P[I].Z,0.0,1.0)):0.f;
        O.Colors.Add(G==EVoxelRenderGroup::Opaque?T.Tint:FLinearColor(Wind,0,1,1));O.Tangents.Add(FProcMeshTangent(Tangent,false));
    }
	O.Triangles.Append({N, N + 2, N + 1, N, N + 3, N + 2});}
}
bool FVoxelSectionMesher::Build(const FVoxelSectionSnapshot&S,const FVoxelRegistrySnapshot&R,const FVoxelShapeRegistry&H,
    FVoxelSectionMeshResult&O,const TAtomic<bool>*Cancel, const double InTextureRepeatsPerCell,
    const FVoxelBoundaryTransitionContext* InTransition, const bool bBoundaryOnly, const bool bIncludeBoundary, const FIntVector& InLocalCellOffset, const int32 InTextureGridSide)
{
    const int32 Side = S.GridSide;
    if(S.Blocks.Num()!=Side * Side * Side)return false;for(uint32 P:S.Blocks)if(!R.IsValid(FVoxelBlockState::Unpack(P)))return false;
    if(InTransition && (!InTransition->Validate() ||
        (bIncludeBoundary && InTransition->GetGridSide() != Side))) return false;
    FVoxelSectionMeshResult T;
    T.Stamp.WorldEpoch = S.Stamp.Epoch;
    T.Stamp.Token = S.Stamp.Token;
    T.Stamp.Revision = S.Revision;
    T.Stamp.Section = S.Section;
    TMap<uint32,int32>Groups;int32 TotalVertices=0;
    auto Batch=[&](EVoxelRenderGroup G,uint16 Bank)->FVoxelMeshBuffers&
    {
        uint32 K=(uint32(G)<<16)|Bank;if(const int32*I=Groups.Find(K))return T.Batches[*I].Mesh;
        FVoxelRenderBatch B;B.Group=G;B.Bank=Bank;int32 I=T.Batches.Add(MoveTemp(B));Groups.Add(K,I);return T.Batches[I].Mesh;
    };
    auto OverBudget=[&](){return TotalVertices>262144;};
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
        const int32 Plane = Origin[Axis] + FMath::RoundToInt(Vertices[0][Axis] * Step);
        FVector LocalMin = Vertices[0], LocalMax = Vertices[0];
        for (int32 Index = 1; Index < 4; ++Index)
            for (int32 Coordinate = 0; Coordinate < 3; ++Coordinate)
            { LocalMin[Coordinate] = FMath::Min(LocalMin[Coordinate], Vertices[Index][Coordinate]);
              LocalMax[Coordinate] = FMath::Max(LocalMax[Coordinate], Vertices[Index][Coordinate]); }
        TArray<FVoxelCoverageRegion> Remaining;
        Remaining.Add({FIntVector(Origin[U] + FMath::RoundToInt(LocalMin[U] * Step),
            Origin[V] + FMath::RoundToInt(LocalMin[V] * Step), 0),
            FIntVector(Origin[U] + FMath::RoundToInt(LocalMax[U] * Step),
            Origin[V] + FMath::RoundToInt(LocalMax[V] * Step), 1)});
        const auto Cut = [&](const FIntVector& Min, const FIntVector& Max)
        {
            TArray<FVoxelCoverageRegion> Next;
            const FVoxelCoverageRegion Rect{FIntVector(Min[U], Min[V], 0), FIntVector(Max[U], Max[V], 1)};
            for (const auto& Piece : Remaining) FVoxelCoverageEvaluationTask::Subtract(Piece, Rect, Next);
            Remaining = MoveTemp(Next);
        };
        for (const auto& Suppression : InTransition->SuppressedFaces)
            if (static_cast<uint8>(Suppression.Direction) == Face && Suppression.Min[Axis] == Plane)
                Cut(Suppression.Min, Suppression.Max);
        for (const auto& Patch : InTransition->Patches)
        {
            const auto Bounds = Patch.Neighbor.Key.GetBounds();
            const int32 PatchPlane = (Face & 1) ? Bounds.Max[Axis] : Bounds.Min[Axis];
            if (static_cast<uint8>(Patch.Face.Direction) == Face && PatchPlane == Plane)
                Cut(Patch.Face.Min, Patch.Face.Max);
        }
        for (const auto& Piece : Remaining)
        {
            FVector Points[4]; FVector2D UV[4];
            const int32 X[4] = {Piece.Min.X, Piece.Max.X, Piece.Max.X, Piece.Min.X};
            const int32 Y[4] = {Piece.Min.Y, Piece.Min.Y, Piece.Max.Y, Piece.Max.Y};
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
        TArray<FFaceKey> Mask; Mask.SetNum(Side * Side);int32 A=F/2,U=(A+1)%3,V=(A+2)%3;
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
            const int32 NeighborStep = Patch.Neighbor.SampleStepCells > 0 ? Patch.Neighbor.SampleStepCells : Patch.Neighbor.Key.GetSampleStep();
            FVoxelBoundaryRaster OwnerRaster, NeighborRaster;
            OwnerRaster.Min = FIntPoint(OwnerBounds.Min[UAxis], OwnerBounds.Min[VAxis]);
            OwnerRaster.Max = FIntPoint(OwnerBounds.Max[UAxis], OwnerBounds.Max[VAxis]);
            OwnerRaster.StepCells = OwnerStep;
            NeighborRaster.Min = FIntPoint(NeighborBounds.Min[UAxis], NeighborBounds.Min[VAxis]);
            NeighborRaster.Max = FIntPoint(NeighborBounds.Max[UAxis], NeighborBounds.Max[VAxis]);
            NeighborRaster.StepCells = NeighborStep;
            const auto Filled = [&R](FVoxelBlockState State)
            {
                const auto* Definition = R.Find(State.TypeId);
                return !State.IsAir() && Definition && Definition->Shape == EVoxelShapeKind::FullCube;
            };
            for (int32 V = 0; V < Side; ++V)
                for (int32 U = 0; U < Side; ++U)
                {
                    FIntVector Local(0);
                    Local[Axis] = OwnerAxis;
                    Local[UAxis] = U; Local[VAxis] = V;
                    const auto State = FVoxelBlockState::Unpack(S.Blocks[Local.X + Side * (Local.Y + Side * Local.Z)]);
                    OwnerRaster.Cells.Add({State.TypeId, Filled(State)});
                }
            for (int32 V = NeighborRaster.Min.Y; V < NeighborRaster.Max.Y; V += NeighborStep)
                for (int32 U = NeighborRaster.Min.X; U < NeighborRaster.Max.X; U += NeighborStep)
                {
                    FIntVector World(0);
                    World[Axis] = (Face & 1) ? NeighborBounds.Max[Axis] - 1 : NeighborBounds.Min[Axis];
                    World[UAxis] = U; World[VAxis] = V;
                    FVoxelBlockState State;
                    if (!Patch.Neighbor.Sample(World, State)) return false;
                    NeighborRaster.Cells.Add({State.TypeId, Filled(State)});
                }
            TArray<FVoxelBoundaryFace> Faces;
            FString Error;
            const auto& Negative = (Face & 1) ? NeighborRaster : OwnerRaster;
            const auto& Positive = (Face & 1) ? OwnerRaster : NeighborRaster;
            if (!FVoxelBoundaryRasterBuilder::BuildVolume(Negative, Positive, Axis, Plane, 65536, Faces, Error)) return false;
            for (const auto& Boundary : Faces)
            {
                const bool bOwnerFilled = Boundary.Normal[Axis] == ((Face & 1) ? -1.0 : 1.0);
                const int32 U = FMath::FloorToInt(Boundary.Vertices[0][UAxis]);
                const int32 V = FMath::FloorToInt(Boundary.Vertices[0][VAxis]);
                FVoxelBlockState State;
                if (bOwnerFilled)
                {
                    FIntVector Local(0);
                    Local[Axis] = OwnerAxis;
                    Local[UAxis] = (U - OwnerBounds.Min[UAxis]) / OwnerStep;
                    Local[VAxis] = (V - OwnerBounds.Min[VAxis]) / OwnerStep;
                    State = FVoxelBlockState::Unpack(S.Blocks[Local.X + Side * (Local.Y + Side * Local.Z)]);
                }
                else
                {
                    FIntVector World(0);
                    World[Axis] = (Face & 1) ? NeighborBounds.Max[Axis] - 1 : NeighborBounds.Min[Axis];
                    World[UAxis] = U; World[VAxis] = V;
                    if (!Patch.Neighbor.Sample(World, State)) return false;
                }
                const auto* Definition = R.Find(State.TypeId);
                if (!Definition) return false;
                const uint8 OutputFace = Boundary.Normal[Axis] > 0 ? Axis * 2 : Axis * 2 + 1;
                const uint8 MaterialFace = FVoxelShapeRegistry::RotateFace(OutputFace, uint8((4 - (State.State & 3)) & 3));
                const auto Texture = Definition->Face(State.State, MaterialFace);
                FVector Vertices[4]; FVector2D UV[4];
                for (int32 I = 0; I < 4; ++I)
                {
                    Vertices[I] = (Boundary.Vertices[I] - FVector(OwnerBounds.Min)) / OwnerStep;
                    UV[I] = MakeFaceUV(OutputFace, Vertices[I], Side) * InTextureRepeatsPerCell;
                }
                if (Boundary.bReverseWinding) { Swap(Vertices[1], Vertices[3]); Swap(UV[1], UV[3]); }
                AppendQuad(Batch(Definition->RenderGroup, Texture.Bank), Vertices, UV, Texture, Definition->RenderGroup, FVector::ZeroVector);
                TotalVertices += 4; if (OverBudget()) return false;
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
            const auto&Tex=D->Face(B.State,Q.MaterialFace);AppendQuad(Batch(D->RenderGroup,Tex.Bank),Q.Vertices,Q.UV,Tex,D->RenderGroup,FVector(P),D->Shape==EVoxelShapeKind::CrossPlant);
            TotalVertices+=4;if(OverBudget())return false;
        }
    }
    T.Batches.Sort([](const FVoxelRenderBatch&A,const FVoxelRenderBatch&B){return A.Group!=B.Group?uint8(A.Group)<uint8(B.Group):A.Bank<B.Bank;});
    for(const auto&B:T.Batches)if(!B.Mesh.Validate())return false;if (!T.ValidateAndCache()) return false; O=MoveTemp(T);return true;
}
