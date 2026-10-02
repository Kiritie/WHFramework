#pragma once

#include "NavMesh/NavMeshBoundsVolume.h"
#include "VoxelNavigationBounds.generated.h"

/** 每个权威模拟流送源独立持有的局部导航边界。 */
UCLASS(NotBlueprintable, Transient)
class WHFRAMEWORK_API AVoxelNavigationBounds : public ANavMeshBoundsVolume
{
	GENERATED_BODY()

public:
	AVoxelNavigationBounds();
	void SetBounds(const FBox& InBounds);
	virtual FBox GetComponentsBoundingBox(bool bNonColliding = false, bool bIncludeFromChildActors = false) const override;

private:
	FBox NavigationBounds = FBox(ForceInit);
};
