#pragma once

#include "NavMesh/NavMeshBoundsVolume.h"
#include "VoxelNavigationBounds.generated.h"

/** 实际导航消费者共同持有的固定局部边界，不随流送源移动。 */
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
