#pragma once

#include "Voxel/Scene/VoxelSceneRegion.h"
#include "VoxelNavigationTestTypes.generated.h"

/** 导航生命周期测试使用的可实例化区域，沿用基础区域的完整回收行为。 */
UCLASS(Transient, NotBlueprintable)
class UVoxelNavigationTestSceneRegion : public UVoxelSceneRegion
{
	GENERATED_BODY()
};
