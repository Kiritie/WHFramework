#include "Voxel/Rendering/VoxelLodMesher.h"
#include "Voxel/Generation/Kernel/VoxelGenMath.h"
namespace
{
const VoxelView::Grid* At(const FVoxelLodMeshInput& I,const FVector& P)
{
    VoxelGen::I3 C{FMath::FloorToInt(P.X),FMath::FloorToInt(P.Y),FMath::FloorToInt(P.Z)};
    if(I.Current->key.Bounds().Contains(C))return I.Current.get();
    for(const auto& G:I.Neighbors)if(G&&G->key.Bounds().Contains(C))return G.get();return nullptr;
}
FVoxelBlockState Sample(const VoxelView::Grid* Grid,const FVector& P,const FVoxelGenerationPipeline& G)
{
    const FIntVector Cell(FMath::FloorToInt(P.X),FMath::FloorToInt(P.Y),FMath::FloorToInt(P.Z));
    FVoxelBlockState Out;
    if(Grid){G.ToRuntime(Grid->Sample({Cell.X,Cell.Y,Cell.Z}),Out);return Out;}
    // Missing displayed-neighbor data is not evidence of air. Natural generation is authoritative for untouched boundaries.
    G.SampleBaseBlock(Cell,Out);return Out;
}
bool Covered(const VoxelView::Grid* Grid,FVoxelBlockState S,const FVector& P,uint8 Face,
             const FVoxelRegistrySnapshot& R,const FVoxelShapeRegistry& H)
{
    if(!Grid||S.IsAir())return false;
    if(Grid->key.level)return true; // The proxy representation is explicitly a full coarse cube, not a gameplay shape.
    const auto* D=R.Find(S.TypeId);const auto* Q=D?H.Find(D->Shape,S.State):nullptr;
    if(!Q)return false;
    const int A=Face/2,U=(A+1)%3,V=(A+2)%3;
    const int X=FMath::Clamp(FMath::FloorToInt((P[U]-FMath::FloorToDouble(P[U]))*16),0,15);
    const int Y=FMath::Clamp(FMath::FloorToInt((P[V]-FMath::FloorToDouble(P[V]))*16),0,15);
    return Q->Covers(Face,X,Y);
}
bool Partial(const VoxelView::Grid& G,const FVoxelGenerationPipeline& Bind,const FVoxelRegistrySnapshot& R)
{
    if(G.key.level)return false;
    for(auto C:G.cells){FVoxelBlockState S;if(!Bind.ToRuntime(C,S)||S.IsAir())continue;const auto* D=R.Find(S.TypeId);
        if(D&&D->Shape!=EVoxelShapeKind::FullCube&&D->Shape!=EVoxelShapeKind::Fluid&&D->Shape!=EVoxelShapeKind::CrossPlant&&
           (D->bOccludes||D->RenderGroup==EVoxelRenderGroup::Translucent))return true;}
    return false;
}
void Quad(FVoxelMeshBuffers& B,uint8 Face,const FVector& Origin,int U0,int V0,int Width,int Height,double Pitch,double Plane,const FVoxelRuntimeFaceRef& T)
{
    const int A=Face/2,U=(A+1)%3,V=(A+2)%3;FVector P[4];const int X[4]={U0,U0+Width,U0+Width,U0},Y[4]={V0,V0,V0+Height,V0+Height};
    for(int K=0;K<4;++K){P[K]=FVector::ZeroVector;P[K][A]=Plane;P[K][U]=X[K]*Pitch;P[K][V]=Y[K]*Pitch;}
    if(Face&1)Swap(P[1],P[3]);const int N=B.Vertices.Num();
    FVector Normal=FVector::ZeroVector;Normal[A]=(Face&1)?-1:1;FVector Tangent=(P[1]-P[0]).GetSafeNormal();
    for(int K=0;K<4;++K)
    {
        B.Vertices.Add(P[K]);B.Normals.Add(Normal);B.Tangents.Add(FProcMeshTangent(Tangent,false));
        const FVector W=P[K]+Origin;
        B.UV0.Add(Face<2?FVector2D(W.Y,-W.Z):Face<4?FVector2D(W.X,-W.Z):FVector2D(W.X,W.Y));
        B.UV1.Add(FVector2D(T.Layer,T.Frames));B.UV2.Add(FVector2D(T.FPS,0));B.Colors.Add(FLinearColor(0,0,1,1));
    }
    B.Triangles.Append({N,N+2,N+1,N,N+3,N+2});
}
}
bool FVoxelLodMesher::Build(const FVoxelLodMeshInput& I,const FVoxelGenerationPipeline& G,const FVoxelRegistrySnapshot& R,const FVoxelShapeRegistry& H,FVoxelLodBuiltMesh& Out,FString& E,const std::atomic_bool* C)
{
    if(!I.Current||I.Current->key.level>4||I.Neighbors.size()>24){E=TEXT("Invalid bounded LOD mesh snapshot");return false;}
    const auto Key=I.Current->key;const auto Bounds=Key.Bounds();const int Step=Key.Step();
    FVoxelSectionSnapshot Snap;Snap.Blocks.SetNumUninitialized(4096);
    for(int J=0;J<4096;++J){FVoxelBlockState B;if(!G.ToRuntime(I.Current->cells[J],B)||!R.IsValid(B)){E=TEXT("Invalid LOD symbol/state");return false;}if(Key.level)B.State&=3;Snap.Blocks[J]=B.Pack();}
    for(int F=0;F<6;++F){Snap.Halo[F].Init(0,256);Snap.Known[F]=false;}
    FVoxelRegistrySnapshot Coarse;
    const FVoxelRegistrySnapshot* RenderRegistry=&R;
    if(Key.level){Coarse.Definitions=R.Definitions;for(auto& D:Coarse.Definitions)D.Shape=EVoxelShapeKind::FullCube;RenderRegistry=&Coarse;}
    FVoxelLodBuiltMesh Result;Result.Key=Key;Result.CoverageSerial=I.CoverageSerial;
    // Interior only. Six boundaries are computed against the ACTUALLY displayed neighbor resolution below.
    if(!FVoxelSectionMesher::Build(Snap,*RenderRegistry,H,Result.Mesh,C,63)){E=TEXT("Interior LOD mesh failed");return false;}
    TMap<uint32,int32> Batches;
    for(int J=0;J<Result.Mesh.Batches.Num();++J)
    {
        auto& B=Result.Mesh.Batches[J];Batches.Add((uint32(B.Group)<<16)|B.Bank,J);
        for(FVector& P:B.Mesh.Vertices)P*=Step;
        // Macro tree crowns are rigid at all LODs. CrossPlant roots retain their local wind mask in L0.
        if(Key.level)for(auto& Color:B.Mesh.Colors)Color.R=0;
    }
    auto Batch=[&](const FVoxelRuntimeDefinition& D,const FVoxelRuntimeFaceRef& T)->FVoxelMeshBuffers&
    {
        uint32 K=(uint32(D.RenderGroup)<<16)|T.Bank;if(const auto* J=Batches.Find(K))return Result.Mesh.Batches[*J].Mesh;
        FVoxelRenderBatch B;B.Group=D.RenderGroup;B.Bank=T.Bank;int J=Result.Mesh.Batches.Add(MoveTemp(B));Batches.Add(K,J);return Result.Mesh.Batches[J].Mesh;
    };
    int MinStep=Step;bool Micro=Partial(*I.Current,G,R);
    for(const auto& N:I.Neighbors)if(N){if(FMath::Abs(int(N->key.level)-int(Key.level))>1){E=TEXT("LOD coverage is not balanced");return false;}MinStep=FMath::Min(MinStep,N->key.Step());Micro|=Partial(*N,G,R);}
    const double Pitch=Micro?1.0/16.0:double(MinStep);
    const int Size=FMath::RoundToInt(Key.Side()/Pitch);
    if(Size>512){E=TEXT("Boundary subdivision budget exceeded");return false;}
    const FVector Origin(Bounds.min.x,Bounds.min.y,Bounds.min.z);
    for(uint8 F=0;F<6;++F)
    {
        if(VoxelGen::Canceled(C))return false;
        const int A=F/2,U=(A+1)%3,V=(A+2)%3;const double Plane=(F&1)?0:Key.Side();
        TArray<uint32> Mask;Mask.Init(0,Size*Size);
        for(int Y=0;Y<Size;++Y)
        {
            if((Y&15)==0&&VoxelGen::Canceled(C))return false;
            for(int X=0;X<Size;++X)
            {
                FVector P=Origin;P[A]+=Plane;P[U]+=(X+.5)*Pitch;P[V]+=(Y+.5)*Pitch;
                FVector Inside=P,Outside=P;const double Sign=(F&1)?-1:1;Inside[A]-=Sign/1024;Outside[A]+=Sign/1024;
                auto AState=Sample(I.Current.get(),Inside,G);if(AState.IsAir()||!Covered(I.Current.get(),AState,Inside,F,R,H))continue;
                const auto* AD=R.Find(AState.TypeId);const auto* N=At(I,Outside);auto BState=Sample(N,Outside,G);const auto* BD=R.Find(BState.TypeId);
                const bool Same=AState.TypeId==BState.TypeId&&(AD->RenderGroup==EVoxelRenderGroup::Water||AD->RenderGroup==EVoxelRenderGroup::Translucent);
                const bool BoundaryCovered=N?Covered(N,BState,Outside,F^1,R,H):(BD&&(BD->Shape==EVoxelShapeKind::FullCube||BD->Shape==EVoxelShapeKind::Fluid));
                const bool Hidden=BD&&!BState.IsAir()&&(BD->bOccludes||Same)&&BoundaryCovered;
                if(!Hidden)Mask[X+Size*Y]=AState.Pack();
            }
        }
        for(int Y=0;Y<Size;++Y)for(int X=0;X<Size;)
        {
            const uint32 Value=Mask[X+Y*Size];if(!Value){++X;continue;}int W=1,Ht=1;
            while(X+W<Size&&Mask[X+W+Y*Size]==Value)++W;
            bool Stop=false;while(Y+Ht<Size&&!Stop){for(int DX=0;DX<W;++DX)if(Mask[X+DX+(Y+Ht)*Size]!=Value){Stop=true;break;}if(!Stop)++Ht;}
            const auto S=FVoxelBlockState::Unpack(Value);const auto* D=R.Find(S.TypeId);
            const uint8 MaterialFace=FVoxelShapeRegistry::RotateFace(F,uint8((4-(S.State&3))&3));const auto& Tex=D->Face(S.State,MaterialFace);
            Quad(Batch(*D,Tex),F,Origin,X,Y,W,Ht,Pitch,Plane,Tex);
            for(int DY=0;DY<Ht;++DY)for(int DX=0;DX<W;++DX)Mask[X+DX+(Y+DY)*Size]=0;
            X+=W;
            if(Result.Bytes()>48ull*1024*1024){E=TEXT("LOD mesh result exceeds 48 MiB");return false;}
        }
    }
    for(const auto& B:Result.Mesh.Batches)if(!B.Mesh.Validate()){E=TEXT("Invalid final LOD mesh");return false;}
    Out=MoveTemp(Result);E.Reset();return true;
}
