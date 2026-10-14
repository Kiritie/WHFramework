#include "Voxel/Generation/Hydrology/VoxelHydrologyRoutingGrid.h"
namespace
{
    int64 RoutingFloor(int64 N, int64 D)
    {
        const int64 Q = N / D;
        return Q - (N % D < 0 ? 1 : 0);
    }
    int64 RoutingCeil(int64 N, int64 D) { return -RoutingFloor(-N,D); }
    int64 RoutingClamp(int64 X, int64 L, int64 H) { return X < L ? L : X > H ? H : X; }
}
bool FVoxelHydrologyRoutingGrid::Initialize(int32 InCoarseCellSize, int32 InMaximumInfluenceRadius,
    const FIntPoint& InWorldMin, const FIntPoint& InWorldMax, const FIntPoint& InOrigin, FString& Error)
{
    if (bInitialized || InCoarseCellSize <= 0 || InMaximumInfluenceRadius < 0 ||
        InWorldMin.X > InWorldMax.X || InWorldMin.Y > InWorldMax.Y ||
        InOrigin.X % InCoarseCellSize != 0 || InOrigin.Y % InCoarseCellSize != 0)
    { Error = TEXT("Invalid routing lattice"); return false; }
    const int64 M = RoutingCeil(2 * int64(InMaximumInfluenceRadius) + 1, InCoarseCellSize);
    const int64 S = M * InCoarseCellSize;
    if (S > MAX_int32) { Error = TEXT("Oversized: routing lattice spacing"); return false; }
    CoarseCellSize = InCoarseCellSize; Spacing = int32(S); Multiplier = int32(M);
    Origin = InOrigin; WorldMin = InWorldMin; WorldMax = InWorldMax;
    IndexMin[0] = RoutingCeil(int64(WorldMin.X)-Origin.X,Spacing);
    IndexMin[1] = RoutingCeil(int64(WorldMin.Y)-Origin.Y,Spacing);
    IndexMax[0] = RoutingFloor(int64(WorldMax.X)-Origin.X,Spacing);
    IndexMax[1] = RoutingFloor(int64(WorldMax.Y)-Origin.Y,Spacing);
    if (IndexMin[0] > IndexMax[0] || IndexMin[1] > IndexMax[1])
    { Error = TEXT("Physical world contains no routing lattice node"); return false; }
    CoarseMin = FIntPoint(int32(RoutingFloor(WorldMin.X,CoarseCellSize)),int32(RoutingFloor(WorldMin.Y,CoarseCellSize)));
    CoarseMax = FIntPoint(int32(RoutingFloor(WorldMax.X,CoarseCellSize)),int32(RoutingFloor(WorldMax.Y,CoarseCellSize)));
    bInitialized = true; Error.Reset(); return true;
}
bool FVoxelHydrologyRoutingGrid::ToWorld(int64 X, int64 Y, FIntPoint& Out) const
{
    if (!bInitialized || X < IndexMin[0] || X > IndexMax[0] || Y < IndexMin[1] || Y > IndexMax[1]) return false;
    const int64 WX = int64(Origin.X) + X*Spacing, WY = int64(Origin.Y) + Y*Spacing;
    if (WX < WorldMin.X || WX > WorldMax.X || WY < WorldMin.Y || WY > WorldMax.Y) return false;
    Out = FIntPoint(int32(WX),int32(WY)); return true;
}
bool FVoxelHydrologyRoutingGrid::IsGridNode(const FIntPoint& P) const
{
    return bInitialized && P.X >= WorldMin.X && P.X <= WorldMax.X && P.Y >= WorldMin.Y && P.Y <= WorldMax.Y &&
        (int64(P.X)-Origin.X)%Spacing == 0 && (int64(P.Y)-Origin.Y)%Spacing == 0;
}
bool FVoxelHydrologyRoutingGrid::TryNeighbor(const FIntPoint& P, int32 Face, FIntPoint& Out) const
{
    if (!IsGridNode(P) || Face < 0 || Face >= 4) return false;
    const int64 X = int64(P.X)+(Face==0?-int64(Spacing):Face==1?int64(Spacing):0);
    const int64 Y = int64(P.Y)+(Face==2?-int64(Spacing):Face==3?int64(Spacing):0);
    Out = FIntPoint(int32(RoutingClamp(X,WorldMin.X,WorldMax.X)),int32(RoutingClamp(Y,WorldMin.Y,WorldMax.Y)));
    return Out != P;
}
int32 FVoxelHydrologyRoutingGrid::GetReverseCandidates(const FIntPoint& P, FIntPoint Out[4]) const
{
    if (!bInitialized || P.X < WorldMin.X || P.X > WorldMax.X || P.Y < WorldMin.Y || P.Y > WorldMax.Y) return 0;
    int32 Count = 0;
    if (IsGridNode(P))
    {
        for (int32 Face=0;Face<4;++Face)
        {
            FIntPoint Candidate;
            if (TryNeighbor(P,Face,Candidate) && IsGridNode(Candidate)) Out[Count++] = Candidate;
        }
        return Count;
    }
    const int64 DX=int64(P.X)-Origin.X, DY=int64(P.Y)-Origin.Y;
    const int64 X=RoutingFloor(DX,Spacing), Y=RoutingFloor(DY,Spacing);
    if (DY%Spacing==0)
    {
        if (ToWorld(X,Y,Out[Count])) ++Count;
        if (ToWorld(X+1,Y,Out[Count])) ++Count;
    }
    if (DX%Spacing==0)
    {
        if (ToWorld(X,Y,Out[Count])) ++Count;
        if (ToWorld(X,Y+1,Out[Count])) ++Count;
    }
    return Count;
}
bool FVoxelHydrologyRoutingGrid::TryMapCoarseCell(const FIntPoint& C, FIntPoint& Out) const
{
    if (!bInitialized || C.X < CoarseMin.X || C.X > CoarseMax.X || C.Y < CoarseMin.Y || C.Y > CoarseMax.Y) return false;
    const int64 X=RoutingFloor(int64(C.X)-Origin.X/CoarseCellSize,Multiplier);
    const int64 Y=RoutingFloor(int64(C.Y)-Origin.Y/CoarseCellSize,Multiplier);
    return ToWorld(RoutingClamp(X,IndexMin[0],IndexMax[0]),RoutingClamp(Y,IndexMin[1],IndexMax[1]),Out);
}
bool FVoxelHydrologyRoutingGrid::TryGetCoarseBounds(const FIntPoint& P, FIntPoint& OutMin, FIntPoint& OutMax) const
{
    if (!IsGridNode(P)) return false;
    const int64 X=(int64(P.X)-Origin.X)/Spacing, Y=(int64(P.Y)-Origin.Y)/Spacing;
    const int64 CX=int64(Origin.X)/CoarseCellSize+X*Multiplier, CY=int64(Origin.Y)/CoarseCellSize+Y*Multiplier;
    OutMin=FIntPoint(int32(X==IndexMin[0]?CoarseMin.X:CX),int32(Y==IndexMin[1]?CoarseMin.Y:CY));
    OutMax=FIntPoint(int32(X==IndexMax[0]?CoarseMax.X:CX+Multiplier-1),int32(Y==IndexMax[1]?CoarseMax.Y:CY+Multiplier-1));
    return true;
}

bool FVoxelHydrologyRoutingGrid::TryGetFineBucketBounds(const FIntPoint& P, FIntPoint& OutMin, FIntPoint& OutMax) const
{
    FIntPoint Low, High;
    if (!TryGetCoarseBounds(P,Low,High)) return false;
    OutMin=FIntPoint(int32(FMath::Max<int64>(WorldMin.X,int64(Low.X)*CoarseCellSize)),
        int32(FMath::Max<int64>(WorldMin.Y,int64(Low.Y)*CoarseCellSize)));
    OutMax=FIntPoint(int32(FMath::Min<int64>(WorldMax.X,(int64(High.X)+1)*CoarseCellSize-1)),
        int32(FMath::Min<int64>(WorldMax.Y,(int64(High.Y)+1)*CoarseCellSize-1)));
    return OutMin.X<=OutMax.X && OutMin.Y<=OutMax.Y;
}
bool FVoxelHydrologyRoutingGrid::TryGetBucketOwner(const FIntPoint& P, FIntPoint& Out) const
{
    if (!bInitialized || P.X<WorldMin.X || P.X>WorldMax.X || P.Y<WorldMin.Y || P.Y>WorldMax.Y) return false;
    return TryMapCoarseCell(FIntPoint(int32(RoutingFloor(P.X,CoarseCellSize)),
        int32(RoutingFloor(P.Y,CoarseCellSize))),Out);
}
