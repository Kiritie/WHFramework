#include "Voxel/Navigation/VoxelNavigationBounds.h"

#include "NavigationSystem.h"

AVoxelNavigationBounds::AVoxelNavigationBounds()
{
	PrimaryActorTick.bCanEverTick = false;
	SetReplicates(false);
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
}

void AVoxelNavigationBounds::SetBounds(const FBox& InBounds)
{
	if (NavigationBounds == InBounds)
	{
		return;
	}
	NavigationBounds = InBounds;
	SetActorLocation(InBounds.GetCenter());
	if (UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		Navigation->OnNavigationBoundsUpdated(this);
	}
}

FBox AVoxelNavigationBounds::GetComponentsBoundingBox(bool bNonColliding, bool bIncludeFromChildActors) const
{
	return NavigationBounds;
}
