#include "Voxel/Navigation/VoxelNavigationSurfaceComponent.h"

#include "AI/NavigationSystemBase.h"
#include "AI/NavigationSystemHelpers.h"

UVoxelNavigationSurfaceComponent::UVoxelNavigationSurfaceComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetGenerateOverlapEvents(false);
	SetCanEverAffectNavigation(true);
	SetCustomNavigableGeometry(EHasCustomNavigableGeometry::EvenIfNotCollidable);
	SetVisibility(false);
}

void UVoxelNavigationSurfaceComponent::Apply(FVoxelNavigationSurface&& InSurface)
{
	Surface = MoveTemp(InSurface);
	UpdateBounds();
	FNavigationSystem::UpdateComponentData(*this);
}

FBoxSphereBounds UVoxelNavigationSurfaceComponent::CalcBounds(const FTransform& InLocalToWorld) const
{
	return FBoxSphereBounds(Surface.Bounds.IsValid ? Surface.Bounds : FBox(FVector::ZeroVector, FVector::ZeroVector)).TransformBy(InLocalToWorld);
}

bool UVoxelNavigationSurfaceComponent::DoCustomNavigableGeometryExport(FNavigableGeometryExport& InExport) const
{
	if (!Surface.Indices.IsEmpty())
	{
		InExport.ExportCustomMesh(Surface.Vertices.GetData(), Surface.Vertices.Num(), Surface.Indices.GetData(), Surface.Indices.Num(), GetComponentTransform());
	}
	return false;
}

const FVoxelNavigationSurface& UVoxelNavigationSurfaceComponent::GetSurface() const
{
	return Surface;
}
