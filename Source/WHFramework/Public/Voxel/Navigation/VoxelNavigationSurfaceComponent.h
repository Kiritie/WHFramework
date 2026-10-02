#pragma once

#include "Components/PrimitiveComponent.h"
#include "Voxel/Navigation/VoxelNavigationTypes.h"
#include "VoxelNavigationSurfaceComponent.generated.h"

/** 只向导航系统导出体素可行走表面，不参与渲染和物理碰撞。 */
UCLASS()
class WHFRAMEWORK_API UVoxelNavigationSurfaceComponent : public UPrimitiveComponent
{
	GENERATED_BODY()

public:
	UVoxelNavigationSurfaceComponent();
	void Apply(FVoxelNavigationSurface&& InSurface);
	virtual FBoxSphereBounds CalcBounds(const FTransform& InLocalToWorld) const override;
	virtual bool DoCustomNavigableGeometryExport(FNavigableGeometryExport& InExport) const override;
	const FVoxelNavigationSurface& GetSurface() const;

private:
	FVoxelNavigationSurface Surface;
};
