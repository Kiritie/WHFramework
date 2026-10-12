#include "Step/Tests/StepGraphTestTypes.h"

#include "Step/Base/StepAsset.h"
#include "Step/Base/StepRuntime.h"

bool UStepGraphTestStep::IsSkipAble_Implementation() const
{
	return bAllowSkip;
}

void UStepGraphTestStep::OnInitialize()
{
	++InitializeCount;
	if(UStepGraphTestModule* Module = Cast<UStepGraphTestModule>(GetStepModule()))
	{
		Module->InitializedGUIDs.Add(StepGUID);
		if(bTerminateOnInitialize)
		{
			Module->OnTermination(EPhase::Primary);
		}
	}
}

void UStepGraphTestStep::OnStateChanged(EStepState InState)
{
}

void UStepGraphTestStep::OnEnter(UStepBase* Last)
{
	++EnterCount;
	SetRuntimeState(EStepState::Entered, false);
	if(bTerminateOnEnter)
	{
		UStepModule* Module = GetStepModule();
		UStepAsset* Asset = GetStepAsset();
		Module->OnTermination(EPhase::Primary);
		bRetainedDuringControlCallback = Module->GetRuntime(Asset) == Runtime && Runtime && !Runtime->RootSteps.IsEmpty();
		return;
	}
	RefreshAutomaticTimers();
}

void UStepGraphTestStep::OnExecute()
{
	++ExecuteCount;
	SetRuntimeState(EStepState::Executing, false);
	if(bReloadOnExecute)
	{
		UStepModule* Module = GetStepModule();
		UStepRuntime* Before = Runtime;
		FStepModuleSaveData Save = Module->ToData().GetRef<FStepModuleSaveData>();
		Save.MakeSaved();
		Module->LoadData(FParameter(Save), EPhase::Primary);
		bReloadKeptRuntime = Runtime == Before && Module->GetRuntime(GetStepAsset()) == Before;
	}
	if(bTerminateOnExecute)
	{
		UStepModule* Module = GetStepModule();
		UStepAsset* Asset = GetStepAsset();
		Module->OnTermination(EPhase::Primary);
		bRetainedDuringControlCallback = Module->GetRuntime(Asset) == Runtime && Runtime && !Runtime->RootSteps.IsEmpty();
		return;
	}
	if(bPauseOnExecute)
	{
		GetStepModule()->OnPause();
	}
	AdvanceChildren();
	RefreshAutomaticTimers();
}

void UStepGraphTestStep::OnComplete(EStepExecuteResult Result)
{
	++CompleteCount;
	StepExecuteResult = Result;
	SetRuntimeState(EStepState::Completed, false);
	RefreshAutomaticTimers();
}

void UStepGraphTestStep::OnLeave()
{
	++LeaveCount;
	SetRuntimeState(EStepState::Leaved, false);
	if(bUnloadOnLeave)
	{
		UStepModule* Module = GetStepModule();
		UStepAsset* Asset = GetStepAsset();
		Module->UnloadData(EPhase::Primary);
		bRetainedDuringControlCallback = Module->GetRuntime(Asset) == Runtime && Runtime && !Runtime->RootSteps.IsEmpty();
		return;
	}
	if(bRemoveAssetOnLeave)
	{
		GetStepModule()->RemoveAsset(GetStepAsset());
	}
	if(bRestartOnLeave)
	{
		GetStepModule()->StartStep();
	}
}

bool UStepGraphTestModule::Prepare(UStepAsset* Asset)
{
	AddAsset(Asset);
	SwitchAsset(Asset);
	return GetRuntime(Asset) != nullptr;
}

void UStepGraphTestModule::BroadcastStepStarted(int32 RootIndex)
{
}

void UStepGraphTestModule::BroadcastStepEnded()
{
}
