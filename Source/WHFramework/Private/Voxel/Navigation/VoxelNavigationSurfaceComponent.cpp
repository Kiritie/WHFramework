#include "Voxel/Navigation/VoxelNavigationSurfaceComponent.h"

#include "AI/NavigationSystemBase.h"
#include "AI/NavigationSystemHelpers.h"
#include "AI/Navigation/NavigationElement.h"
#include "AI/Navigation/NavigationRelevantData.h"

UVoxelNavigationSurfaceComponent::UVoxelNavigationSurfaceComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetGenerateOverlapEvents(false);
	SetCanEverAffectNavigation(true);
	SetCustomNavigableGeometry(EHasCustomNavigableGeometry::EvenIfNotCollidable);
	SetVisibility(false);
}

void UVoxelNavigationSurfaceComponent::Apply(TSharedPtr<const FVoxelNavigationSurface, ESPMode::ThreadSafe> InSurface)
{
	if (Surface && Surface != InSurface && Surface->Access) Surface->Access->Revoke();
	Surface = MoveTemp(InSurface);
	if (Surface && Surface->Access) Surface->Access->Publish();
	ExportedSurface.Reset();
	ExportedElement.Reset();
	UpdateBounds();
	FNavigationSystem::UpdateComponentData(*this);
}

FBoxSphereBounds UVoxelNavigationSurfaceComponent::CalcBounds(const FTransform& InLocalToWorld) const
{
	return FBoxSphereBounds(Surface && Surface->Bounds.IsValid ? Surface->Bounds :
		FBox(FVector::ZeroVector, FVector::ZeroVector)).TransformBy(InLocalToWorld);
}

bool UVoxelNavigationSurfaceComponent::DoCustomNavigableGeometryExport(FNavigableGeometryExport& InExport) const
{
	if (Surface && !Surface->Indices.IsEmpty())
	{
		InExport.ExportCustomMesh(Surface->Vertices.GetData(), Surface->Vertices.Num(), Surface->Indices.GetData(),
			Surface->Indices.Num(), GetComponentTransform());
	}
	return false;
}

const FVoxelNavigationSurface& UVoxelNavigationSurfaceComponent::GetSurface() const
{
	static const FVoxelNavigationSurface Empty;
	return Surface ? *Surface : Empty;
}

bool UVoxelNavigationSurfaceComponent::AcquireSurfaceRead(FVoxelNavigationSurfaceReadLease& OutLease) const
{
	check(IsInGameThread());
	return IsRegistered() && OutLease.Acquire(Surface);
}

void UVoxelNavigationSurfaceComponent::OnUnregister()
{
	if (Surface && Surface->Access) Surface->Access->Revoke();
	Super::OnUnregister();
}

void UVoxelNavigationSurfaceComponent::OnUpdateTransform(const EUpdateTransformFlags InFlags, const ETeleportType InTeleport)
{
	if (Surface && Surface->Access) Surface->Access->Revoke();
	Super::OnUpdateTransform(InFlags, InTeleport);
}

void UVoxelNavigationSurfaceComponent::GetNavigationData(FNavigationRelevantData& OutData) const
{
	check(IsInGameThread());
	Super::GetNavigationData(OutData);
	// Instant 的几何和元数据导出在同一 GT 调用中完成；旧 octree 输入不能借用新 Surface 版本。
	ExportedElement = OutData.SourceElement;
	ExportedSurface = Surface;
	ExportedTransform = GetComponentTransform();
}

ENavDataGatheringMode UVoxelNavigationSurfaceComponent::GetGeometryGatheringMode() const
{
	return ENavDataGatheringMode::Instant;
}

TSharedPtr<const FVoxelNavigationSurface, ESPMode::ThreadSafe> UVoxelNavigationSurfaceComponent::CaptureExportedSurface(
	const FNavigationElement& InElement) const
{
	check(IsInGameThread());
	return ExportedElement.Pin().Get() == &InElement && ExportedSurface == Surface &&
		ExportedTransform.Equals(GetComponentTransform(), 0.0) ? ExportedSurface : nullptr;
}
