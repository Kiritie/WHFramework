#include "Voxel/Navigation/VoxelNavigationBounds.h"

#include "Components/BrushComponent.h"
#include "NavigationSystem.h"

AVoxelNavigationBounds::AVoxelNavigationBounds()
{
	PrimaryActorTick.bCanEverTick = false;
	// 导航 Invoker 读取 Actor 位置，必须与随模拟来源移动的边界中心一致。
	GetBrushComponent()->SetMobility(EComponentMobility::Movable);
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
