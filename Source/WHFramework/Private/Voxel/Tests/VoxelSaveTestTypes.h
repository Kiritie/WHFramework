#pragma once

#include "Main/Base/ModuleBase.h"
#include "SaveGame/SaveGameModule.h"
#include "Voxel/VoxelModuleTypes.h"
#include "VoxelSaveTestTypes.generated.h"

/** 保存协调器回归沿用实际反射 payload 和原始模块回调。 */
UCLASS(Transient, NotBlueprintable)
class UVoxelSaveTestModule : public UModuleBase
{
	GENERATED_BODY()

public:
	UVoxelSaveTestModule()
	{
		ModuleName = TEXT("VoxelCaptureTest");
		SaveScope = ESaveScope::World;
		SaveDataVersion = 3;
	}

	UPROPERTY(Transient)
	TObjectPtr<USaveGameModule> Coordinator;

	int32 BeforeCount = 0;
	int32 CaptureCount = 0;
	int32 AfterCount = 0;
	int32 Seed = 71;
	bool bTerminateBefore = false;
	bool bRefreshDuringBefore = false;
	bool bTerminateCapture = false;
	bool bTerminateAfter = false;
	bool bAfterSucceeded = false;

	virtual void OnBeforeSaveData() override
	{
		++BeforeCount;
		if (bRefreshDuringBefore) Coordinator->OnRefresh(0.01f, false);
		if (bTerminateBefore) Coordinator->OnTermination(EPhase::All);
	}

	virtual void OnAfterSaveData(const bool bSuccess) override
	{
		++AfterCount;
		bAfterSucceeded = bSuccess;
		if (bTerminateAfter) Coordinator->OnTermination(EPhase::All);
	}

protected:
	virtual FParameter ToData() override
	{
		++CaptureCount;
		FVoxelModuleSaveData Data;
		Data.Seed = Seed;
		Data.ManifestBytes.SetNumZeroed(3 * 64 * 1024 + 19);
		if (bTerminateCapture) Coordinator->OnTermination(EPhase::All);
		return FParameter(MoveTemp(Data));
	}
};

/** 不同模块身份验证真实批量保存中的逐项准备和已开始回调前缀。 */
UCLASS(Transient, NotBlueprintable)
class UVoxelSaveOtherTestModule : public UVoxelSaveTestModule
{
	GENERATED_BODY()

public:
	UVoxelSaveOtherTestModule()
	{
		ModuleName = TEXT("VoxelCaptureSecond");
		ModuleIndex = 1;
	}
};
