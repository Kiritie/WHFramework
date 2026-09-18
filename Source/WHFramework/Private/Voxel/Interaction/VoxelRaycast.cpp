#include "Voxel/Interaction/VoxelRaycast.h"
#include <limits>
namespace
{
bool RayBox(const FVector&S,const FVector&D,const FBox&B,double&T,FVector&N,bool&Inside)
{
    double Near=-std::numeric_limits<double>::infinity(),Far=std::numeric_limits<double>::infinity();FVector NN(0,0,0),FN(0,0,0);
    for(int32 A=0;A<3;++A)
    {
        if(FMath::Abs(D[A])<1e-12){if(S[A]<B.Min[A]||S[A]>B.Max[A])return false;continue;}
        double X=(B.Min[A]-S[A])/D[A],Y=(B.Max[A]-S[A])/D[A];FVector NX(0,0,0),NY(0,0,0);NX[A]=-1;NY[A]=1;
        if(X>Y){Swap(X,Y);Swap(NX,NY);}if(X>Near){Near=X;NN=NX;}if(Y<Far){Far=Y;FN=NY;}if(Near>Far)return false;
    }
    if(Far<0)return false;Inside=Near<0;T=Inside?Far:Near;N=Inside?FN:NN;return FMath::IsFinite(T);
}
bool RayTriangle(const FVector&S,const FVector&D,const FVector&A,const FVector&B,const FVector&C,double&T,FVector&N)
{
    FVector E1=B-A,E2=C-A,P=FVector::CrossProduct(D,E2);double Det=FVector::DotProduct(E1,P);if(FMath::Abs(Det)<1e-12)return false;
    FVector Q=S-A;double U=FVector::DotProduct(Q,P)/Det;if(U<0||U>1)return false;
    FVector X=FVector::CrossProduct(Q,E1);double V=FVector::DotProduct(D,X)/Det;if(V<0||U+V>1)return false;
    T=FVector::DotProduct(E2,X)/Det;if(T<0)return false;N=FVector::CrossProduct(E1,E2).GetSafeNormal();if(FVector::DotProduct(N,D)>0)N=-N;return true;
}
}
FVoxelTraceResult FVoxelRaycast::Trace(const FVoxelWorldRuntime&W,const FVoxelRegistrySnapshot&R,const FVoxelShapeRegistry&H,
    const FVector&Start,const FVector&Direction,double MaxDistance,double BlockSize)
{
    FVoxelTraceResult O;FIntVector Cell;
    if(!VoxelCoord::FromWorld(Start,BlockSize,Cell)||!FMath::IsFinite(MaxDistance)||MaxDistance<=0||MaxDistance>100000||Direction.ContainsNaN()||Direction.IsNearlyZero())
    {O.Status=EVoxelTraceStatus::Invalid;return O;}
    FVector D=Direction.GetSafeNormal(),S=Start/BlockSize;const double Limit=MaxDistance/BlockSize,Inf=std::numeric_limits<double>::infinity();
    FIntVector Step(0,0,0);FVector Next(Inf,Inf,Inf),Delta(Inf,Inf,Inf);
    for(int32 A=0;A<3;++A)if(FMath::Abs(D[A])>=1e-12){Step[A]=D[A]>0?1:-1;Delta[A]=FMath::Abs(1/D[A]);Next[A]=(Cell[A]+(Step[A]>0?1:0)-S[A])/D[A];}
    double Enter=0;const int32 MaxSteps=FMath::CeilToInt(Limit*1.733)+8;
    for(int32 Iter=0;Iter<MaxSteps&&Enter<=Limit;++Iter)
    {
        double Exit=FMath::Min(Limit,FMath::Min3(Next.X,Next.Y,Next.Z));
        if(Exit>Enter+1e-10)
        {
            FVoxelBlockState State;if(!W.TryGetBlock(Cell,State)){O.Status=EVoxelTraceStatus::NeedsData;O.Index=Cell;return O;}
            if(!State.IsAir())
            {
                const auto*Def=R.Find(State.TypeId);const auto*Shape=Def?H.Find(Def->Shape,State.State):nullptr;
                if(!Shape){O.Status=EVoxelTraceStatus::Invalid;return O;}FVector LocalStart=S-FVector(Cell);
                double Best=Inf;FVector BestN(0,0,0);bool Inside=false;
                auto Accept=[&](double T,const FVector&N,bool IsInside){if(T>=Enter-1e-9&&T<=Exit+1e-9&&T<Best){Best=T;BestN=N;Inside=IsInside;}};
                for(const auto&B:Shape->SelectionBoxes){double T;FVector N;bool In;if(RayBox(LocalStart,D,B,T,N,In))Accept(T,N,In);}
                for(const auto&Q:Shape->SelectionQuads)
                {
                    double T;FVector N;if(RayTriangle(LocalStart,D,Q.Vertices[0],Q.Vertices[1],Q.Vertices[2],T,N))Accept(T,N,false);
                    if(RayTriangle(LocalStart,D,Q.Vertices[0],Q.Vertices[2],Q.Vertices[3],T,N))Accept(T,N,false);
                }
                if(Best<=Limit)
                {
                    O.Status=EVoxelTraceStatus::Hit;O.Index=Cell;O.State=State;O.Distance=Best*BlockSize;O.Point=Start+D*O.Distance;O.Normal=BestN;O.bStartedInside=Inside;
                    int32 A=0;if(FMath::Abs(BestN.Y)>FMath::Abs(BestN[A]))A=1;if(FMath::Abs(BestN.Z)>FMath::Abs(BestN[A]))A=2;
                    O.PlacementIndex=Cell;O.PlacementIndex[A]+=BestN[A]>=0?1:-1;return O;
                }
            }
        }
        if(Exit>=Limit)break;const double Boundary=FMath::Min3(Next.X,Next.Y,Next.Z);
        for(int32 A=0;A<3;++A)if(Next[A]<=Boundary+1e-10){Cell[A]+=Step[A];Next[A]+=Delta[A];}
        if(!VoxelCoord::IsValid(Cell)){O.Status=EVoxelTraceStatus::Invalid;return O;}Enter=Boundary;
    }
    return O;
}
