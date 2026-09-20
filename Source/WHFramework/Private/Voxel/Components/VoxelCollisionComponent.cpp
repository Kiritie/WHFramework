#include "Voxel/Components/VoxelCollisionComponent.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/BoxElem.h"
UVoxelCollisionComponent::UVoxelCollisionComponent()
{
    SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);SetCollisionObjectType(ECC_WorldStatic);
    SetCollisionResponseToAllChannels(ECR_Block);SetGenerateOverlapEvents(false);SetCanEverAffectNavigation(false);
    SetMobility(EComponentMobility::Movable);SetHiddenInGame(true);
}
UBodySetup* UVoxelCollisionComponent::GetBodySetup(){return Setup;}
FPrimitiveSceneProxy* UVoxelCollisionComponent::CreateSceneProxy(){return nullptr;}
FBoxSphereBounds UVoxelCollisionComponent::CalcBounds(const FTransform&T)const{return FBoxSphereBounds(LocalBounds).TransformBy(T);}
bool UVoxelCollisionComponent::Apply(const TArray<FBox>&B,double Scale)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_CollisionApplyGT);
    check(IsInGameThread());
	const double StartTime = FPlatformTime::Seconds();
	if(!FMath::IsFinite(Scale)||Scale<=0||B.Num()>16384)return false;
    for(const auto&X:B)if(!X.IsValid||X.Min.ContainsNaN()||X.Max.ContainsNaN()||X.Min.X>=X.Max.X||X.Min.Y>=X.Max.Y||X.Min.Z>=X.Max.Z)return false;
    auto*New=NewObject<UBodySetup>(this);New->CollisionTraceFlag=CTF_UseSimpleAsComplex;
    New->AggGeom.BoxElems.Reserve(B.Num());FBox CollisionBounds(ForceInit);
    for(const auto&X:B)
    {
        FKBoxElem E;E.Center=X.GetCenter()*Scale;FVector Size=X.GetSize()*Scale;E.X=Size.X;E.Y=Size.Y;E.Z=Size.Z;
        New->AggGeom.BoxElems.Add(E);CollisionBounds+=FBox(X.Min*Scale,X.Max*Scale);
    }
    DestroyPhysicsState();Setup=New;LocalBounds=B.IsEmpty()?FBox(FVector::ZeroVector,FVector::ZeroVector):CollisionBounds;
    SetCollisionEnabled(B.IsEmpty()?ECollisionEnabled::NoCollision:ECollisionEnabled::QueryAndPhysics);
	RecreatePhysicsState();
	UpdateBounds();
	const bool bSuccess = B.IsEmpty() || IsPhysicsStateCreated();
#if !UE_BUILD_SHIPPING
	const double Milliseconds =
		(FPlatformTime::Seconds() - StartTime) * 1000.0;
	if (Milliseconds >= 5.0)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Voxel collision GT apply %.2fms; boxes=%d"),
			Milliseconds,
			B.Num());
	}
#endif
	return bSuccess;
}
