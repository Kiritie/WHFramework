#include "Voxel/Geometry/VoxelSectionMesher.h"
#include "Voxel/Chunks/VoxelSectionKey.h"
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
bool Covered(const FVoxelResolvedShape&S,uint8 F,const FVoxelShapeQuad*Q)
{
    if(!Q)return (S.OcclusionMask&(1u<<F))!=0;
    int32 A=F/2,U=(A+1)%3,V=(A+2)%3;double U0=1,U1=0,V0=1,V1=0;
    for(const auto&P:Q->Vertices){U0=FMath::Min(U0,P[U]);U1=FMath::Max(U1,P[U]);V0=FMath::Min(V0,P[V]);V1=FMath::Max(V1,P[V]);}
    int32 MinU=FMath::Clamp(FMath::FloorToInt(U0*16+1e-6),0,15),MaxU=FMath::Clamp(FMath::CeilToInt(U1*16-1e-6)-1,0,15);
    int32 MinV=FMath::Clamp(FMath::FloorToInt(V0*16+1e-6),0,15),MaxV=FMath::Clamp(FMath::CeilToInt(V1*16-1e-6)-1,0,15);
    for(int32 Y=MinV;Y<=MaxV;++Y)for(int32 X=MinU;X<=MaxU;++X)if(!S.Covers(F,X,Y))return false;return true;
}
bool Visible(const FVoxelSectionSnapshot&S,const FVoxelRegistrySnapshot&R,const FVoxelShapeRegistry&H,
    FIntVector P,FVoxelBlockState A,uint8 Face,const FVoxelShapeQuad*Quad)
{
    FVoxelBlockState B;if(!S.TrySample(P+VoxelCoord::Direction(Face),B)||B.IsAir())return true;
    const auto*BD=R.Find(B.TypeId);const auto*AD=R.Find(A.TypeId);if(!BD||!AD)return true;
    const bool SameTransparent=A.TypeId==B.TypeId&&(AD->RenderGroup==EVoxelRenderGroup::Water||AD->RenderGroup==EVoxelRenderGroup::Translucent);
    if(!BD->bOccludes&&!SameTransparent)return true;const auto*Shape=H.Find(BD->Shape,B.State);return !Shape||!Covered(*Shape,Face^1,Quad);
}
void AppendQuad(FVoxelMeshBuffers&O,const FVector*P,const FVector2D*UV,const FVoxelRuntimeFaceRef&T,EVoxelRenderGroup G,const FVector&Cell,bool bPlantWind=false)
{
    int32 N=O.Vertices.Num();FVector Normal=FVector::CrossProduct(P[1]-P[0],P[2]-P[0]).GetSafeNormal();FVector Tangent=(P[1]-P[0]).GetSafeNormal();
    for(int32 I=0;I<4;++I)
    {
        O.Vertices.Add(P[I]+Cell);O.Normals.Add(Normal);O.UV0.Add(UV[I]);O.UV1.Add(FVector2D(T.Layer,T.Frames));O.UV2.Add(FVector2D(T.FPS,0));
        const float Wind=bPlantWind&&G==EVoxelRenderGroup::Foliage?float(FMath::Clamp(P[I].Z,0.0,1.0)):0.f;
        O.Colors.Add(FLinearColor(Wind,0,1,1));O.Tangents.Add(FProcMeshTangent(Tangent,false));
    }
    O.Triangles.Append({N,N+1,N+2,N,N+2,N+3});
}
}
bool FVoxelSectionMesher::Build(const FVoxelSectionSnapshot&S,const FVoxelRegistrySnapshot&R,const FVoxelShapeRegistry&H,
    FVoxelSectionMeshResult&O,const std::atomic_bool*Cancel,uint8 SkipBoundaryMask)
{
    if(S.Blocks.Num()!=4096)return false;for(uint32 P:S.Blocks)if(!R.IsValid(FVoxelBlockState::Unpack(P)))return false;
    FVoxelSectionMeshResult T;T.Stamp=S.Stamp;TMap<uint32,int32>Groups;int32 TotalVertices=0;
    auto Batch=[&](EVoxelRenderGroup G,uint16 Bank)->FVoxelMeshBuffers&
    {
        uint32 K=(uint32(G)<<16)|Bank;if(const int32*I=Groups.Find(K))return T.Batches[*I].Mesh;
        FVoxelRenderBatch B;B.Group=G;B.Bank=Bank;int32 I=T.Batches.Add(MoveTemp(B));Groups.Add(K,I);return T.Batches[I].Mesh;
    };
    auto OverBudget=[&](){return TotalVertices>262144;};
    for(uint8 F=0;F<6;++F)for(int32 Slice=0;Slice<16;++Slice)
    {
        if((SkipBoundaryMask&(1u<<F))&&Slice==((F&1)?0:15))continue;
        if(Cancel&&Cancel->load(std::memory_order_relaxed))return false;
        FFaceKey Mask[256];int32 A=F/2,U=(A+1)%3,V=(A+2)%3;
        for(int32 Y=0;Y<16;++Y)for(int32 X=0;X<16;++X)
        {
            FIntVector P(0,0,0);P[A]=Slice;P[U]=X;P[V]=Y;FVoxelBlockState B=FVoxelBlockState::Unpack(S.Blocks[VoxelCoord::Linear(P)]);
            if(B.IsAir())continue;const auto*D=R.Find(B.TypeId);if(D->Shape!=EVoxelShapeKind::FullCube||!Visible(S,R,H,P,B,F,nullptr))continue;
            FFaceKey&K=Mask[X+16*Y];K.Visible=true;K.Packed=B.Pack();K.Group=D->RenderGroup;
            uint8 MF=FVoxelShapeRegistry::RotateFace(F,uint8((4-(B.State&3))&3));K.Texture=D->Face(B.State,MF);
        }
        for(int32 Y=0;Y<16;++Y)for(int32 X=0;X<16;)
        {
            const auto K=Mask[X+16*Y];if(!K.Visible){++X;continue;}int32 W=1,Ht=1;
            while(X+W<16&&Mask[X+W+16*Y]==K)++W;
            bool Stop=false;while(Y+Ht<16&&!Stop){for(int32 DX=0;DX<W;++DX)if(!(Mask[X+DX+16*(Y+Ht)]==K)){Stop=true;break;}if(!Stop)++Ht;}
            FVector P[4];FVector2D UV[4]={{0,0},{double(W),0},{double(W),double(Ht)},{0,double(Ht)}};
            const int32 XX[4]={X,X+W,X+W,X},YY[4]={Y,Y,Y+Ht,Y+Ht};
            for(int32 I=0;I<4;++I){P[I]=FVector::ZeroVector;P[I][A]=Slice+(F%2==0?1:0);P[I][U]=XX[I];P[I][V]=YY[I];}
            if(F&1){Swap(P[1],P[3]);Swap(UV[1],UV[3]);}
            AppendQuad(Batch(K.Group,K.Texture.Bank),P,UV,K.Texture,K.Group,FVector::ZeroVector);
            TotalVertices+=4;if(OverBudget())return false;
            for(int32 DY=0;DY<Ht;++DY)for(int32 DX=0;DX<W;++DX)Mask[X+DX+16*(Y+DY)].Visible=false;X+=W;
        }
    }
    for(uint16 I=0;I<4096;++I)
    {
        if((I&63)==0&&Cancel&&Cancel->load(std::memory_order_relaxed))return false;
        auto B=FVoxelBlockState::Unpack(S.Blocks[I]);if(B.IsAir())continue;const auto*D=R.Find(B.TypeId);
        if(D->Shape==EVoxelShapeKind::FullCube)continue;const auto*Shape=H.Find(D->Shape,B.State);if(!Shape)return false;
        FIntVector P=VoxelCoord::Unlinear(I);for(const auto&Q:Shape->Quads)
        {
            if(Q.bBoundary&&(SkipBoundaryMask&(1u<<Q.Face))&&P[Q.Face/2]==((Q.Face&1)?0:15))continue;
            if(Q.bBoundary&&!Visible(S,R,H,P,B,Q.Face,&Q))continue;
            const auto&Tex=D->Face(B.State,Q.MaterialFace);AppendQuad(Batch(D->RenderGroup,Tex.Bank),Q.Vertices,Q.UV,Tex,D->RenderGroup,FVector(P),D->Shape==EVoxelShapeKind::CrossPlant);
            TotalVertices+=4;if(OverBudget())return false;
        }
    }
    T.Batches.Sort([](const FVoxelRenderBatch&A,const FVoxelRenderBatch&B){return A.Group!=B.Group?uint8(A.Group)<uint8(B.Group):A.Bank<B.Bank;});
    for(const auto&B:T.Batches)if(!B.Mesh.Validate())return false;O=MoveTemp(T);return true;
}
