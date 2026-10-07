#pragma once

#include "Voxel/Scene/VoxelSceneRegion.h"
#include "Voxel/VoxelModule.h"
#include "VoxelNavigationTestTypes.generated.h"

/** 导航生命周期测试使用的可实例化区域，沿用基础区域的完整回收行为。 */
UCLASS(Transient, NotBlueprintable)
class UVoxelNavigationTestSceneRegion : public UVoxelSceneRegion
{
	GENERATED_BODY()

public:
	void AddPendingCaptureActor(const FGuid& InId, const FParameter& InData)
	{
		PendingActors.Add(InId, InData);
		Super::OnSectionActivated(FIntVector::ZeroValue);
		Super::OnSectionDeactivated(FIntVector::ZeroValue);
	}
	bool bRejectProjectCapture = false;
	bool bAttemptWorldStopOnCapture = false;
	mutable bool bWorldStopRejected = false;
	virtual bool CaptureProjectData(FParameter& OutData) const override
	{
		if (bAttemptWorldStopOnCapture)
		{
			FString Error;
			bWorldStopRejected = Module && !Module->StopWorld(true, Error) && Error.Contains(TEXT("scene transaction"));
		}
		return !bRejectProjectCapture && Super::CaptureProjectData(OutData);
	}
};
