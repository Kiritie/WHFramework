#include "Voxel/Geometry/VoxelSectionMesher.h"
#include "Voxel/Geometry/DWVoxelFaceVisibility.h"
#include "Voxel/Geometry/DWVoxelBoundaryTransition.h"
#include "Voxel/Chunks/VoxelSectionKey.h"

bool FVoxelSectionMesher::IsKnownEmpty(const FVoxelSectionSnapshot& InSnapshot,
	const FVoxelRegistrySnapshot& InRegistry, const FVoxelBoundaryTransitionContext* InTransition)
{
	if (InSnapshot.Blocks.Num() != VoxelBlock::Volume) return false;
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
		return true;
	}
	for (const uint32 Packed : InSnapshot.Blocks)
	{
		if (!IsOpaqueCube(Packed)) return false;
	}
	for (int32 Face = 0; Face < 6; ++Face)
	{
		if (!InSnapshot.Known[Face] || InSnapshot.Halo[Face].Num() != VoxelBlock::Size * VoxelBlock::Size) return false;
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
    const FVoxelBoundaryTransitionContext* InTransition)
{
    if(S.Blocks.Num()!=4096)return false;for(uint32 P:S.Blocks)if(!R.IsValid(FVoxelBlockState::Unpack(P)))return false;
    if(InTransition && !InTransition->Validate())return false;
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
    for(uint8 F=0;F<6;++F)for(int32 Slice=0;Slice<16;++Slice)
    {
        if(Cancel&&Cancel->Load())return false;
        FFaceKey Mask[256];int32 A=F/2,U=(A+1)%3,V=(A+2)%3;
        for(int32 Y=0;Y<16;++Y)for(int32 X=0;X<16;++X)
        {
            FIntVector P(0,0,0);P[A]=Slice;P[U]=X;P[V]=Y;FVoxelBlockState B=FVoxelBlockState::Unpack(S.Blocks[VoxelCoord::Linear(P)]);
            if(B.IsAir())continue;const auto*D=R.Find(B.TypeId);if(D->Shape!=EVoxelShapeKind::FullCube||
                (InTransition && InTransition->CoversCell(F,P))||!Visible(S,R,H,P,B,F,nullptr))continue;
            FFaceKey&K=Mask[X+16*Y];K.Visible=true;K.Packed=B.Pack();K.Group=D->RenderGroup;
            uint8 MF=FVoxelShapeRegistry::RotateFace(F,uint8((4-(B.State&3))&3));K.Texture=D->Face(B.State,MF);
        }
        for(int32 Y=0;Y<16;++Y)for(int32 X=0;X<16;)
        {
            const auto K=Mask[X+16*Y];if(!K.Visible){++X;continue;}int32 W=1,Ht=1;
            while(X+W<16&&Mask[X+W+16*Y]==K)++W;
            bool Stop=false;while(Y+Ht<16&&!Stop){for(int32 DX=0;DX<W;++DX)if(!(Mask[X+DX+16*(Y+Ht)]==K)){Stop=true;break;}if(!Stop)++Ht;}
        	FVector P[4];
        	FVector2D UV[4];
            const int32 XX[4]={X,X+W,X+W,X},YY[4]={Y,Y,Y+Ht,Y+Ht};
            for(int32 I=0;I<4;++I)
            {
	            P[I]=FVector::ZeroVector;P[I][A]=Slice+(F%2==0?1:0);P[I][U]=XX[I];P[I][V]=YY[I];
            	UV[I] = MakeFaceUV(F, P[I], 16.0);
            }
            for (FVector2D& Coordinate : UV)
            {
                Coordinate *= InTextureRepeatsPerCell;
            }
            if(F&1){Swap(P[1],P[3]);Swap(UV[1],UV[3]);}
            AppendQuad(Batch(K.Group,K.Texture.Bank),P,UV,K.Texture,K.Group,FVector::ZeroVector);
            TotalVertices+=4;if(OverBudget())return false;
            for(int32 DY=0;DY<Ht;++DY)for(int32 DX=0;DX<W;++DX)Mask[X+DX+16*(Y+DY)].Visible=false;X+=W;
        }
    }
    if(InTransition)
    {
        const FVoxelGenerationBounds OwnerBounds = InTransition->Owner.GetBounds();
        const int32 OwnerStep = InTransition->Owner.GetStep();
        for(const FVoxelBoundaryTransitionPatch& Patch:InTransition->Patches)
        {
            if(Cancel&&Cancel->Load())return false;
            const uint8 Face=static_cast<uint8>(Patch.Face.Direction);
            const int32 Axis=Face/2,UAxis=(Axis+1)%3,VAxis=(Axis+2)%3;
            const int32 Ratio=Patch.Face.Ratio,FineStep=OwnerStep/Ratio;
            const int32 UFirst=(Patch.Face.Min[UAxis]-OwnerBounds.Min[UAxis])/OwnerStep;
            const int32 ULast=(Patch.Face.Max[UAxis]-OwnerBounds.Min[UAxis])/OwnerStep;
            const int32 VFirst=(Patch.Face.Min[VAxis]-OwnerBounds.Min[VAxis])/OwnerStep;
            const int32 VLast=(Patch.Face.Max[VAxis]-OwnerBounds.Min[VAxis])/OwnerStep;
            for(int32 V=VFirst;V<VLast;++V)for(int32 U=UFirst;U<ULast;++U)
            {
                FIntVector Cell=FIntVector::ZeroValue;
                Cell[Axis]=(Face&1)?0:15;Cell[UAxis]=U;Cell[VAxis]=V;
                const FVoxelBlockState Current=FVoxelBlockState::Unpack(S.Blocks[VoxelCoord::Linear(Cell)]);
                if(Current.IsAir())continue;
                const FVoxelRuntimeDefinition* Definition=R.Find(Current.TypeId);
                if(!Definition||Definition->Shape!=EVoxelShapeKind::FullCube)continue;
                const uint8 MaterialFace=FVoxelShapeRegistry::RotateFace(Face,uint8((4-(Current.State&3))&3));
                const FVoxelRuntimeFaceRef Texture=Definition->Face(Current.State,MaterialFace);
                for(int32 SV=0;SV<Ratio;++SV)for(int32 SU=0;SU<Ratio;++SU)
                {
                    FIntVector Sample=FIntVector::ZeroValue;
                    Sample[Axis]=(Face&1)?OwnerBounds.Min[Axis]-1:OwnerBounds.Max[Axis];
                    Sample[UAxis]=OwnerBounds.Min[UAxis]+U*OwnerStep+SU*FineStep+FineStep/2;
                    Sample[VAxis]=OwnerBounds.Min[VAxis]+V*OwnerStep+SV*FineStep+FineStep/2;
                    FVoxelBlockState Neighbor;
                    if(!Patch.Neighbor.Sample(Sample,Neighbor))return false;
                    if(!VoxelFaceVisibility::ShouldRender(R,H,Current,Neighbor,Face,nullptr))continue;
                    FVector Vertices[4];FVector2D UV[4];
                    const double U0=double(U)+double(SU)/Ratio,U1=double(U)+double(SU+1)/Ratio;
                    const double V0=double(V)+double(SV)/Ratio,V1=double(V)+double(SV+1)/Ratio;
                    const double Us[4]={U0,U1,U1,U0},Vs[4]={V0,V0,V1,V1};
                    for(int32 I=0;I<4;++I)
                    {
                        Vertices[I]=FVector::ZeroVector;
                        Vertices[I][Axis]=(Face&1)?0.0:16.0;
                        Vertices[I][UAxis]=Us[I];Vertices[I][VAxis]=Vs[I];
                        UV[I]=MakeFaceUV(Face,Vertices[I],16.0)*InTextureRepeatsPerCell;
                    }
                    if(Face&1){Swap(Vertices[1],Vertices[3]);Swap(UV[1],UV[3]);}
                    AppendQuad(Batch(Definition->RenderGroup,Texture.Bank),Vertices,UV,Texture,
                        Definition->RenderGroup,FVector::ZeroVector);
                    TotalVertices+=4;if(OverBudget())return false;
                }
            }
        }
    }
    for(uint16 I=0;I<4096;++I)
    {
        if((I&63)==0&&Cancel&&Cancel->Load())return false;
        auto B=FVoxelBlockState::Unpack(S.Blocks[I]);if(B.IsAir())continue;const auto*D=R.Find(B.TypeId);
        if(D->Shape==EVoxelShapeKind::FullCube)continue;const auto*Shape=H.Find(D->Shape,B.State);if(!Shape)return false;
        FIntVector P=VoxelCoord::Unlinear(I);for(const auto&Q:Shape->Quads)
        {
            if(Q.bBoundary&&!Visible(S,R,H,P,B,Q.Face,&Q))continue;
            const auto&Tex=D->Face(B.State,Q.MaterialFace);AppendQuad(Batch(D->RenderGroup,Tex.Bank),Q.Vertices,Q.UV,Tex,D->RenderGroup,FVector(P),D->Shape==EVoxelShapeKind::CrossPlant);
            TotalVertices+=4;if(OverBudget())return false;
        }
    }
    T.Batches.Sort([](const FVoxelRenderBatch&A,const FVoxelRenderBatch&B){return A.Group!=B.Group?uint8(A.Group)<uint8(B.Group):A.Bank<B.Bank;});
    for(const auto&B:T.Batches)if(!B.Mesh.Validate())return false;O=MoveTemp(T);return true;
}
