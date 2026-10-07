#pragma once

#include "Components/PrimitiveComponent.h"
#include "Voxel/Navigation/VoxelNavigationTypes.h"
#include "VoxelNavigationSurfaceComponent.generated.h"

struct FNavigationElement;

/** 只向导航系统导出体素可行走表面，不参与渲染和物理碰撞。 */
UCLASS()
class WHFRAMEWORK_API UVoxelNavigationSurfaceComponent : public UPrimitiveComponent
{
	GENERATED_BODY()

public:
	UVoxelNavigationSurfaceComponent();
	void Apply(TSharedPtr<const FVoxelNavigationSurface, ESPMode::ThreadSafe> InSurface);
	virtual FBoxSphereBounds CalcBounds(const FTransform& InLocalToWorld) const override;
	virtual bool DoCustomNavigableGeometryExport(FNavigableGeometryExport& InExport) const override;
	virtual void GetNavigationData(FNavigationRelevantData& OutData) const override;
	virtual ENavDataGatheringMode GetGeometryGatheringMode() const override;
	TSharedPtr<const FVoxelNavigationSurface, ESPMode::ThreadSafe> CaptureExportedSurface(const FNavigationElement& InElement) const;
	const FVoxelNavigationSurface& GetSurface() const;
	bool AcquireSurfaceRead(
		FVoxelNavigationSurfaceReadLease& OutLease) const;

protected:
	virtual void OnUnregister() override;
	virtual void OnUpdateTransform(
		EUpdateTransformFlags InFlags,
		ETeleportType InTeleport) override;

private:
	TSharedPtr<const FVoxelNavigationSurface, ESPMode::ThreadSafe> Surface;
	mutable TWeakPtr<const FNavigationElement> ExportedElement;
	mutable TSharedPtr<const FVoxelNavigationSurface, ESPMode::ThreadSafe> ExportedSurface;
	mutable FTransform ExportedTransform = FTransform::Identity;
};
