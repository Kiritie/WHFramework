#include "Voxel/Runtime/VoxelStreaming.h"
bool FVoxelStreaming::Validate(const FVoxelStreamingSource&S)
{
    return S.Id.IsValid()&&VoxelCoord::IsValid(S.Center)&&!S.Direction.ContainsNaN()&&S.RenderRadius>=0&&S.RenderRadius<=16&&
    S.CollisionRadius>=0&&S.CollisionRadius<=8&&S.SimulationRadius>=0&&S.SimulationRadius<=8&&S.PreloadRadius>=0&&S.PreloadRadius<=18&&S.VerticalRadius>=0&&S.VerticalRadius<=24;
}
TMap<FVoxelSectionKey,FVoxelSectionDemand> FVoxelStreaming::Compute(const TArray<FVoxelStreamingSource>&Sources,const FVoxelGenerationSettings&S)
{
    TMap<FVoxelSectionKey,FVoxelSectionDemand>O;
    for(const auto&A:Sources)
    {
        if(!Validate(A))continue;auto C=VoxelCoord::Section(A.Center);
        const int32 R=FMath::Max(FMath::Max(A.bRender?A.RenderRadius:0,A.bCollision?A.CollisionRadius:0),FMath::Max(A.bSimulation?A.SimulationRadius:0,A.PreloadRadius));
        for(int32 Z=-A.VerticalRadius;Z<=A.VerticalRadius;++Z)for(int32 Y=-R;Y<=R;++Y)for(int32 X=-R;X<=R;++X)
        {
            int32 D2=X*X+Y*Y;if(D2>R*R)continue;FVoxelSectionKey K{C.X+X,C.Y+Y,C.Z+Z};if(!VoxelCoord::IsValidSection(K,S.MinZ,S.MaxZ))continue;
            FVoxelSectionDemand D;D.bMesh=A.bRender&&D2<=A.RenderRadius*A.RenderRadius;
            D.bCollision=A.bCollision&&D2<=A.CollisionRadius*A.CollisionRadius&&FMath::Abs(Z)<=2;
            D.bSimulation=A.bSimulation&&D2<=A.SimulationRadius*A.SimulationRadius&&FMath::Abs(Z)<=2;
            FVector Offset(X,Y,Z);D.Priority=D2+4*Z*Z-2*FVector::DotProduct(Offset.GetSafeNormal(),A.Direction.GetSafeNormal());
            if(D.bCollision)D.Priority-=100000;
            if(auto*E=O.Find(K)){E->bMesh|=D.bMesh;E->bCollision|=D.bCollision;E->bSimulation|=D.bSimulation;E->Priority=FMath::Min(E->Priority,D.Priority);}
            else O.Add(K,D);
        }
    }return O;
}
TArray<FVoxelSectionKey> FVoxelStreaming::ByPriority(const TMap<FVoxelSectionKey,FVoxelSectionDemand>&D)
{
    TArray<FVoxelSectionKey>K;D.GetKeys(K);K.Sort([&](const FVoxelSectionKey&A,const FVoxelSectionKey&B){double X=D.FindChecked(A).Priority,Y=D.FindChecked(B).Priority;return X==Y?A<B:X<Y;});return K;
}
