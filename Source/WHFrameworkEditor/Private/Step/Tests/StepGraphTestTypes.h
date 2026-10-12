#pragma once

#include "Step/Base/StepBase.h"
#include "Step/StepModule.h"
#include "StepGraphTestTypes.generated.h"

UCLASS()
class UStepGraphTestStep : public UStepBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere)
	bool bAllowSkip = true;

	UPROPERTY(EditAnywhere)
	bool bRemoveAssetOnLeave = false;

	UPROPERTY(EditAnywhere)
	bool bTerminateOnInitialize = false;

	UPROPERTY(EditAnywhere)
	bool bTerminateOnEnter = false;

	UPROPERTY(EditAnywhere)
	bool bTerminateOnExecute = false;

	UPROPERTY(EditAnywhere)
	bool bUnloadOnLeave = false;

	UPROPERTY(EditAnywhere)
	bool bRestartOnLeave = false;

	UPROPERTY(EditAnywhere)
	bool bReloadOnExecute = false;

	UPROPERTY(EditAnywhere)
	bool bPauseOnExecute = false;

	UPROPERTY(Transient)
	bool bRetainedDuringControlCallback = false;

	UPROPERTY(Transient)
	bool bReloadKeptRuntime = false;

	UPROPERTY(Transient)
	int32 InitializeCount = 0;

	UPROPERTY(Transient)
	int32 EnterCount = 0;

	UPROPERTY(Transient)
	int32 ExecuteCount = 0;

	UPROPERTY(Transient)
	int32 CompleteCount = 0;

	UPROPERTY(Transient)
	int32 LeaveCount = 0;

	virtual bool IsSkipAble_Implementation() const override;
	virtual void OnInitialize() override;
	virtual void OnStateChanged(EStepState InState) override;
	virtual void OnEnter(UStepBase* Last) override;
	virtual void OnExecute() override;
	virtual void OnComplete(EStepExecuteResult Result) override;
	virtual void OnLeave() override;
};

UCLASS()
class UStepGraphTestModule : public UStepModule
{
	GENERATED_BODY()

public:
	bool Prepare(UStepAsset* Asset);
	TArray<FString> InitializedGUIDs;

protected:
	virtual void BroadcastStepStarted(int32 RootIndex) override;
	virtual void BroadcastStepEnded() override;
};
