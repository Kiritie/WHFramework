#include "Step/Base/StepBase.h"

#include "Camera/CameraModule.h"
#include "Camera/Anchor/CameraShotAnchor.h"
#include "Camera/Manager/CameraManagerBase.h"
#include "Event/EventModuleStatics.h"
#include "Event/Events/Step/Event_StepCompleted.h"
#include "Event/Events/Step/Event_StepEntered.h"
#include "Event/Events/Step/Event_StepExecuted.h"
#include "Event/Events/Step/Event_StepLeaved.h"
#include "Event/Events/Step/Event_StepStateChanged.h"
#include "Step/Base/StepAsset.h"
#include "Step/Base/StepRuntime.h"
#include "Step/StepModule.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"

UStepBase::UStepBase()
{
	StepGUID = FGuid::NewGuid().ToString();
	StepDisplayName = FText::FromString(TEXT("Step Base"));
	StepDescription = FText::GetEmpty();

	StepIndex = 0;
	StepHierarchy = 0;
	StepState = EStepState::None;

	OperationTarget = nullptr;
	bApplyCameraAction = false;
	bRestoreDefaultCameraOnLeave = false;

	StepExecuteCondition = EStepExecuteResult::None;
	StepExecuteResult = EStepExecuteResult::None;

	StepExecuteType = EStepExecuteType::Automatic;
	AutoExecuteStepTime = 0.f;

	StepLeaveType = EStepLeaveType::Automatic;
	AutoLeaveStepTime = 0.f;

	StepCompleteType = EStepCompleteType::Procedure;
	AutoCompleteStepTime = 0.f;
	StepGuideType = EStepGuideType::None;
	StepGuideIntervalTime = 0.f;

	CurrentSubStepIndex = -1;

	CurrentStepTaskIndex = -1;

	bMergeSubStep = false;

	SubSteps = TArray<UStepBase*>();

	RootStep = nullptr;
	ParentStep = nullptr;

	StepState = EStepState::None;
}

#if WITH_EDITOR
void UStepBase::OnGenerate()
{
	
}

void UStepBase::OnUnGenerate()
{
	
}
#endif

void UStepBase::InitializeRuntime(UStepRuntime* InRuntime)
{
	Runtime = InRuntime;
	bRuntimeInitialized = false;
	RootStep = nullptr;
	ParentStep = nullptr;
	SubSteps.Reset();
	StepState = EStepState::None;
	StepExecuteResult = EStepExecuteResult::None;
	CurrentSubStepIndex = INDEX_NONE;
	CurrentStepTaskIndex = INDEX_NONE;
	StepTaskItems.Reset();
	CameraOverrideHandle.Reset();
	ClearRuntimeTimers();
}

void UStepBase::ShutdownRuntime()
{
	ClearRuntimeTimers();
	ReleaseCameraAction(false);
	if(bRuntimeInitialized)
	{
		bRuntimeInitialized = false;
		K2_OnSuspend();
	}
	StepTaskItems.Reset();
	Runtime = nullptr;
}

bool UStepBase::CanContinueRuntimeCallback() const
{
	const UStepModule* Module = GetStepModule();
	return Runtime && Module && !Module->IsShuttingDown();
}

void UStepBase::ClearRuntimeTimers()
{
	AutoExecuteRemaining = -1.f;
	AutoCompleteRemaining = -1.f;
	AutoLeaveRemaining = -1.f;
	GuideRemaining = -1.f;
}

void UStepBase::SetRuntimeState(EStepState InState, bool bNotify)
{
	if(StepState == InState)
	{
		return;
	}
	StepState = InState;
	if(bNotify)
	{
		OnStateChanged(InState);
	}
}

void UStepBase::OnStateChanged(EStepState InStepState)
{
	OnStepStateChanged.Broadcast(InStepState);
	if(!CanContinueRuntimeCallback())
	{
		return;
	}
	K2_OnStateChanged(InStepState);
	if(CanContinueRuntimeCallback())
	{
		UEventModuleStatics::BroadcastEvent<FEventStepStateChanged>(this, {this});
	}
}

void UStepBase::OnInitialize()
{
	K2_OnInitialize();
}

void UStepBase::OnRestore()
{
	ClearRuntimeTimers();
	ReleaseCameraAction(false);
	StepExecuteResult = EStepExecuteResult::None;
	CurrentSubStepIndex = INDEX_NONE;
	CurrentStepTaskIndex = INDEX_NONE;
	for(FStepTaskItem& Item : StepTaskItems)
	{
		Item.TaskState = EStepTaskState::None;
		Item.RemainTime = 0.f;
	}
	SetRuntimeState(EStepState::None);
	if(CanContinueRuntimeCallback())
	{
		K2_OnRestore();
	}
}

void UStepBase::OnEnter(UStepBase* InLastStep)
{
	ClearRuntimeTimers();
	StepExecuteResult = EStepExecuteResult::None;
	if(ParentStep)
	{
		ParentStep->CurrentSubStepIndex = StepIndex;
	}
	SetRuntimeState(EStepState::Entered);
	if(!CanContinueRuntimeCallback() || StepState != EStepState::Entered)
	{
		return;
	}
	K2_OnEnter(InLastStep);
	if(!CanContinueRuntimeCallback() || StepState != EStepState::Entered)
	{
		return;
	}
	ApplyCameraAction();
	if(StepGuideType != EStepGuideType::None)
	{
		GuideRemaining = FMath::Max(0.f, StepGuideIntervalTime);
	}
	UEventModuleStatics::BroadcastEvent<FEventStepEntered>(this, {this});
	if(CanContinueRuntimeCallback() && StepState == EStepState::Entered)
	{
		RefreshAutomaticTimers();
	}
}

void UStepBase::OnExecute()
{
	AutoExecuteRemaining = -1.f;
	SetRuntimeState(EStepState::Executing);
	if(!CanContinueRuntimeCallback() || StepState != EStepState::Executing)
	{
		return;
	}
	K2_OnExecute();
	if(!CanContinueRuntimeCallback() || StepState != EStepState::Executing)
	{
		return;
	}
	UEventModuleStatics::BroadcastEvent<FEventStepExecuted>(this, {this});
	if(!CanContinueRuntimeCallback() || StepState != EStepState::Executing)
	{
		return;
	}
	if(GetStepCompleteType() == EStepCompleteType::Skip)
	{
		Complete(EStepExecuteResult::Skipped);
		return;
	}
	if(bMergeSubStep)
	{
		for(UStepBase* Child : SubSteps)
		{
			if(!CanContinueRuntimeCallback() || GetStepModule()->GetStepModuleState() != EStepModuleState::Running || StepState != EStepState::Executing)
			{
				return;
			}
			const bool bMatches = Child->CheckStepCondition(nullptr);
			if(!CanContinueRuntimeCallback() || GetStepModule()->GetStepModuleState() != EStepModuleState::Running)
			{
				return;
			}
			if(bMatches)
			{
				Child->Enter();
			}
			else
			{
				GetStepModule()->SkipUnmatchedStep(Child);
			}
		}
	}
	else
	{
		AdvanceChildren();
	}
	if(CanContinueRuntimeCallback())
	{
		RefreshAutomaticTimers();
	}
}

void UStepBase::AdvanceChildren()
{
	if(!CanContinueRuntimeCallback() || GetStepModule()->GetStepModuleState() != EStepModuleState::Running || bMergeSubStep || StepState != EStepState::Executing)
	{
		return;
	}
	UStepBase* Previous = GetCurrentSubStep();
	if(Previous && Previous->StepState != EStepState::Leaved)
	{
		return;
	}
	while(SubSteps.IsValidIndex(CurrentSubStepIndex + 1))
	{
		if(!CanContinueRuntimeCallback() || GetStepModule()->GetStepModuleState() != EStepModuleState::Running || StepState != EStepState::Executing)
		{
			return;
		}
		UStepBase* Child = SubSteps[CurrentSubStepIndex + 1];
		const bool bMatches = Child->CheckStepCondition(Previous);
		if(!CanContinueRuntimeCallback() || GetStepModule()->GetStepModuleState() != EStepModuleState::Running || StepState != EStepState::Executing)
		{
			return;
		}
		++CurrentSubStepIndex;
		if(bMatches)
		{
			Child->Enter();
			return;
		}
		GetStepModule()->SkipUnmatchedStep(Child);
		Previous = Child;
	}
}

void UStepBase::OnRefresh()
{
	K2_OnRefresh();
	if(!CanContinueRuntimeCallback() || GetStepModule()->GetStepModuleState() != EStepModuleState::Running || StepState != EStepState::Executing)
	{
		return;
	}
	if(!SubSteps.IsEmpty())
	{
		if(bMergeSubStep)
		{
			bool bAllLeaved = true;
			for(UStepBase* Child : SubSteps)
			{
				if(!CanContinueRuntimeCallback() || GetStepModule()->GetStepModuleState() != EStepModuleState::Running)
				{
					return;
				}
				if(Child->StepState == EStepState::None)
				{
					const bool bMatches = Child->CheckStepCondition(nullptr);
					if(!CanContinueRuntimeCallback() || GetStepModule()->GetStepModuleState() != EStepModuleState::Running)
					{
						return;
					}
					if(bMatches)
					{
						Child->Enter();
					}
					else
					{
						GetStepModule()->SkipUnmatchedStep(Child);
					}
				}
				else if(Child->StepState != EStepState::Leaved)
				{
					Child->TickRuntime(RefreshDeltaSeconds);
				}
				bAllLeaved &= Child->StepState == EStepState::Leaved;
			}
			if(!bAllLeaved)
			{
				return;
			}
		}
		else
		{
			UStepBase* Child = GetCurrentSubStep();
			if(Child && Child->StepState != EStepState::Leaved)
			{
				Child->TickRuntime(RefreshDeltaSeconds);
			}
			AdvanceChildren();
			if(CurrentSubStepIndex < SubSteps.Num() - 1 || (GetCurrentSubStep() && GetCurrentSubStep()->StepState != EStepState::Leaved))
			{
				return;
			}
		}
	}
	if(!CanContinueRuntimeCallback() || GetStepModule()->GetStepModuleState() != EStepModuleState::Running || StepState != EStepState::Executing)
	{
		return;
	}
	if(HasStepTask())
	{
		RefreshStepTasks(RefreshDeltaSeconds);
		if(!CanContinueRuntimeCallback() || GetStepModule()->GetStepModuleState() != EStepModuleState::Running || CurrentStepTaskIndex < StepTaskItems.Num())
		{
			return;
		}
	}
	if(!SubSteps.IsEmpty() || HasStepTask())
	{
		Complete(IsAllSubExecuteSucceed() ? EStepExecuteResult::Succeed : EStepExecuteResult::Failed);
	}
}

void UStepBase::RefreshStepTasks(float DeltaSeconds)
{
	if(CurrentStepTaskIndex == INDEX_NONE)
	{
		CurrentStepTaskIndex = 0;
	}
	if(!StepTaskItems.IsValidIndex(CurrentStepTaskIndex))
	{
		return;
	}
	FStepTaskItem& Item = StepTaskItems[CurrentStepTaskIndex];
	switch(Item.TaskState)
	{
		case EStepTaskState::None:
		{
			Item.Prepare();
			break;
		}
		case EStepTaskState::Preparing:
		{
			Item.TryExecute(DeltaSeconds);
			break;
		}
		case EStepTaskState::Executing:
		{
			Item.TryComplete(DeltaSeconds);
			break;
		}
		case EStepTaskState::Completed:
		{
			++CurrentStepTaskIndex;
			break;
		}
	}
}

void UStepBase::OnGuide()
{
	ApplyCameraAction();
	K2_OnGuide();
	if(CanContinueRuntimeCallback())
	{
		GuideRemaining = StepGuideType == EStepGuideType::TimerLoop ? FMath::Max(KINDA_SMALL_NUMBER, StepGuideIntervalTime) : -1.f;
	}
}

void UStepBase::OnComplete(EStepExecuteResult InStepExecuteResult)
{
	ClearRuntimeTimers();
	StepExecuteResult = InStepExecuteResult;
	SetRuntimeState(EStepState::Completed);
	if(!CanContinueRuntimeCallback() || StepState != EStepState::Completed)
	{
		return;
	}
	K2_OnComplete(InStepExecuteResult);
	if(!CanContinueRuntimeCallback() || StepState != EStepState::Completed)
	{
		return;
	}
	UEventModuleStatics::BroadcastEvent<FEventStepCompleted>(this, {this});
	if(CanContinueRuntimeCallback() && StepState == EStepState::Completed)
	{
		RefreshAutomaticTimers();
	}
}

void UStepBase::OnLeave()
{
	ClearRuntimeTimers();
	ReleaseCameraAction(bRestoreDefaultCameraOnLeave);
	SetRuntimeState(EStepState::Leaved);
	if(!CanContinueRuntimeCallback() || StepState != EStepState::Leaved)
	{
		return;
	}
	K2_OnLeave();
	if(CanContinueRuntimeCallback())
	{
		UEventModuleStatics::BroadcastEvent<FEventStepLeaved>(this, {this});
	}
}

void UStepBase::TickRuntime(float DeltaSeconds)
{
	const UStepModule* Module = GetStepModule();
	if(!Runtime || !Module || Module->GetStepModuleState() != EStepModuleState::Running || !Module->Owns(this))
	{
		return;
	}
	RefreshDeltaSeconds = FMath::Max(0.f, DeltaSeconds);
	if(IsEntered() && GuideRemaining >= 0.f)
	{
		GuideRemaining -= RefreshDeltaSeconds;
		if(GuideRemaining <= 0.f)
		{
			GuideRemaining = -1.f;
			Guide();
		}
	}
	if(!CanContinueRuntimeCallback() || Module->GetStepModuleState() != EStepModuleState::Running || !Module->Owns(this))
	{
		return;
	}
	switch(StepState)
	{
		case EStepState::Entered:
		{
			if(AutoExecuteRemaining >= 0.f)
			{
				AutoExecuteRemaining -= RefreshDeltaSeconds;
				if(AutoExecuteRemaining <= 0.f)
				{
					AutoExecuteRemaining = -1.f;
					Execute();
				}
			}
			break;
		}
		case EStepState::Executing:
		{
			Refresh();
			if(CanContinueRuntimeCallback() && Module->GetStepModuleState() == EStepModuleState::Running && Module->Owns(this) && StepState == EStepState::Executing && AutoCompleteRemaining >= 0.f)
			{
				AutoCompleteRemaining -= RefreshDeltaSeconds;
				if(AutoCompleteRemaining <= 0.f)
				{
					AutoCompleteRemaining = -1.f;
					Complete();
				}
			}
			break;
		}
		case EStepState::Completed:
		{
			if(AutoLeaveRemaining >= 0.f)
			{
				AutoLeaveRemaining -= RefreshDeltaSeconds;
				if(AutoLeaveRemaining <= 0.f)
				{
					AutoLeaveRemaining = -1.f;
					Leave();
				}
			}
			break;
		}
		default:
		{
			break;
		}
	}
}

void UStepBase::RefreshAutomaticTimers(bool bPreserveRemaining)
{
	auto Update = [bPreserveRemaining](float& Remaining, bool bEnabled, float Duration)
	{
		if(!bEnabled)
		{
			Remaining = -1.f;
		}
		else if(!bPreserveRemaining || Remaining < 0.f)
		{
			Remaining = FMath::Max(0.f, Duration);
		}
	};
	Update(AutoExecuteRemaining, StepState == EStepState::Entered && GetStepExecuteType() == EStepExecuteType::Automatic, AutoExecuteStepTime);
	Update(AutoCompleteRemaining, StepState == EStepState::Executing && GetStepCompleteType() == EStepCompleteType::Automatic && SubSteps.IsEmpty() && !HasStepTask(), AutoCompleteStepTime);
	Update(AutoLeaveRemaining, StepState == EStepState::Completed && GetStepLeaveType() == EStepLeaveType::Automatic, AutoLeaveStepTime);
}

UStepAsset* UStepBase::GetStepAsset() const
{
	return Runtime ? Runtime->Definition : Cast<UStepAsset>(GetOuter());
}

UStepModule* UStepBase::GetStepModule() const
{
	return Runtime ? Runtime->GetModule() : nullptr;
}

bool UStepBase::IsEntered() const
{
	return StepState == EStepState::Entered || StepState == EStepState::Executing;
}

bool UStepBase::IsCompleted(bool bCheckSubs) const
{
	return (StepState == EStepState::Completed || StepState == EStepState::Leaved) && (!bCheckSubs || IsAllSubCompleted());
}

bool UStepBase::IsSkipAble_Implementation() const
{
	return true;
}

void UStepBase::ApplyCameraAction()
{
	if(!Runtime || !IsEntered() || !bApplyCameraAction)
	{
		return;
	}
	UCameraModule* CameraModule = UCameraModule::GetPtr(GetWorld());
	ACameraManagerBase* CameraManager = CameraModule ? CameraModule->GetCameraManager() : nullptr;
	if(!CameraManager)
	{
		return;
	}
	if(CameraOverrideHandle.IsValid())
	{
		CameraManager->PopCameraConfigOverride(CameraOverrideHandle);
	}
	CameraOverrideHandle = CameraManager->PushCameraConfigOverride(CameraOverride, 20);
	if(CameraModeClass)
	{
		FCameraModeContext Context;
		Context.Target = OperationTarget.LoadSynchronous();
		Context.Anchor = CameraAnchor.LoadSynchronous();
		Context.Transition = CameraTransition;
		CameraManager->SetMode(CameraModeClass, Context);
	}
}

void UStepBase::ReleaseCameraAction(bool bRestoreMode)
{
	UCameraModule* CameraModule = UCameraModule::GetPtr(GetWorld());
	ACameraManagerBase* CameraManager = CameraModule ? CameraModule->GetCameraManager() : nullptr;
	if(CameraManager)
	{
		if(CameraOverrideHandle.IsValid())
		{
			CameraManager->PopCameraConfigOverride(CameraOverrideHandle);
		}
		if(bRestoreMode)
		{
			CameraManager->SetDefaultMode(CameraTransition);
		}
	}
	CameraOverrideHandle.Reset();
}

AActor* UStepBase::GetOperationTarget(TSubclassOf<AActor> InClass) const
{
	AActor* Target = OperationTarget.Get();
	return Target && (!InClass || Target->IsA(InClass)) ? Target : nullptr;
}

void UStepBase::SetOperationTarget(AActor* InOperationTarget, bool bResetCameraView)
{
	OperationTarget = InOperationTarget;
	if(bResetCameraView)
	{
		ApplyCameraAction();
	}
}

bool UStepBase::CheckStepCondition(UStepBase* InStep) const
{
	return StepExecuteCondition == EStepExecuteResult::None || !InStep || InStep->StepExecuteResult == StepExecuteCondition;
}

EStepExecuteType UStepBase::GetStepExecuteType() const
{
	const UStepModule* Module = GetStepModule();
	if(!HasSubStep(false) && Module && Module->GetGlobalStepExecuteType() != EStepExecuteType::None)
	{
		return Module->GetGlobalStepExecuteType();
	}
	return HasSubStep(false) ? EStepExecuteType::Automatic : StepExecuteType;
}

EStepLeaveType UStepBase::GetStepLeaveType() const
{
	const UStepModule* Module = GetStepModule();
	if(!HasSubStep(false) && Module && Module->GetGlobalStepLeaveType() != EStepLeaveType::None)
	{
		return Module->GetGlobalStepLeaveType();
	}
	return HasSubStep(false) ? EStepLeaveType::Automatic : StepLeaveType;
}

EStepCompleteType UStepBase::GetStepCompleteType() const
{
	const UStepModule* Module = GetStepModule();
	if(!HasSubStep(false) && Module && Module->GetGlobalStepCompleteType() != EStepCompleteType::None)
	{
		return Module->GetGlobalStepCompleteType();
	}
	return HasSubStep(false) ? EStepCompleteType::Procedure : StepCompleteType;
}

bool UStepBase::IsParentOf(UStepBase* InStep) const
{
	for(const UStepBase* Parent = InStep ? InStep->ParentStep : nullptr; Parent; Parent = Parent->ParentStep)
	{
		if(Parent == this)
		{
			return true;
		}
	}
	return false;
}

bool UStepBase::IsSubOf(UStepBase* InStep) const
{
	return InStep && InStep->IsParentOf(const_cast<UStepBase*>(this));
}

bool UStepBase::HasSubStep(bool bIgnoreMerge) const
{
	return !SubSteps.IsEmpty() && (bIgnoreMerge || !bMergeSubStep);
}

UStepBase* UStepBase::GetCurrentSubStep() const
{
	return SubSteps.IsValidIndex(CurrentSubStepIndex) ? SubSteps[CurrentSubStepIndex] : nullptr;
}

bool UStepBase::IsAllSubCompleted() const
{
	for(const UStepBase* Child : SubSteps)
	{
		if(!Child->IsCompleted(true))
		{
			return false;
		}
	}
	return true;
}

bool UStepBase::IsAllSubExecuteSucceed() const
{
	for(const UStepBase* Child : SubSteps)
	{
		if(Child->StepExecuteResult == EStepExecuteResult::Failed)
		{
			return false;
		}
	}
	return true;
}

bool UStepBase::HasStepTask() const
{
	return !StepTaskItems.IsEmpty();
}

bool UStepBase::IsAllTaskCompleted() const
{
	for(const FStepTaskItem& Item : StepTaskItems)
	{
		if(Item.TaskState != EStepTaskState::Completed)
		{
			return false;
		}
	}
	return true;
}

FStepTaskItem& UStepBase::AddStepTask(const FName InTaskName, float InDurationTime, float InDelayTime)
{
	checkf(!InTaskName.IsNone(), TEXT("StepTask 必须具有稳定名称。"));
	checkf(!StepTaskItems.ContainsByPredicate([InTaskName](const FStepTaskItem& Item){ return Item.TaskName == InTaskName; }), TEXT("StepTask 名称不可重复。"));
	checkf(CurrentStepTaskIndex == INDEX_NONE, TEXT("StepTask 调度开始后不能追加条目。"));
	return StepTaskItems.Emplace_GetRef(InTaskName, FMath::Max(0.f, InDurationTime), FMath::Max(0.f, InDelayTime));
}

FStepRuntimeSaveData UStepBase::CaptureRuntimeData()
{
	FStepRuntimeSaveData Data;
	Data.StepGUID = StepGUID;
	Data.State = StepState;
	Data.Result = StepExecuteResult;
	Data.CurrentChildGUID = GetCurrentSubStep() ? GetCurrentSubStep()->StepGUID : FString();
	Data.CurrentTaskName = StepTaskItems.IsValidIndex(CurrentStepTaskIndex) ? StepTaskItems[CurrentStepTaskIndex].TaskName : NAME_None;
	Data.AutoExecuteRemaining = AutoExecuteRemaining;
	Data.AutoCompleteRemaining = AutoCompleteRemaining;
	Data.AutoLeaveRemaining = AutoLeaveRemaining;
	Data.GuideRemaining = GuideRemaining;
	for(const FStepTaskItem& Item : StepTaskItems)
	{
		FStepTaskSaveData TaskData;
		TaskData.TaskName = Item.TaskName;
		TaskData.State = Item.TaskState;
		TaskData.Remaining = Item.RemainTime;
		Data.Tasks.Add(TaskData);
	}
	FMemoryWriter Writer(Data.CustomData, true);
	FObjectAndNameAsStringProxyArchive Archive(Writer, false);
	Archive.ArIsSaveGame = true;
	Serialize(Archive);
	return Data;
}

bool UStepBase::RestoreRuntimeData(const FStepRuntimeSaveData& InData, FText& OutError)
{
	if(InData.StepGUID != StepGUID || InData.State > EStepState::Leaved || InData.Result > EStepExecuteResult::Skipped)
	{
		OutError = FText::FromString(TEXT("步骤状态记录无效。"));
		return false;
	}
	CurrentSubStepIndex = InData.CurrentChildGUID.IsEmpty() ? INDEX_NONE : SubSteps.IndexOfByPredicate([&InData](const UStepBase* Step){ return Step->StepGUID == InData.CurrentChildGUID; });
	if(!InData.CurrentChildGUID.IsEmpty() && CurrentSubStepIndex == INDEX_NONE)
	{
		OutError = FText::FromString(TEXT("当前子步骤 GUID 不属于保存节点的直接孩子。"));
		return false;
	}
	if(InData.Tasks.Num() != StepTaskItems.Num())
	{
		OutError = FText::FromString(TEXT("StepTask 定义已改变，无法恢复旧步骤任务。"));
		return false;
	}
	TSet<FName> RestoredTasks;
	for(const FStepTaskSaveData& TaskData : InData.Tasks)
	{
		FStepTaskItem* Item = StepTaskItems.FindByPredicate([&TaskData](const FStepTaskItem& Candidate){ return Candidate.TaskName == TaskData.TaskName; });
		if(!Item || RestoredTasks.Contains(TaskData.TaskName) || TaskData.State > EStepTaskState::Completed || !FMath::IsFinite(TaskData.Remaining))
		{
			OutError = FText::FromString(TEXT("StepTask 保存身份或计时状态无效。"));
			return false;
		}
		RestoredTasks.Add(TaskData.TaskName);
		Item->TaskState = TaskData.State;
		Item->RemainTime = TaskData.Remaining;
	}
	CurrentStepTaskIndex = InData.CurrentTaskName.IsNone() ? (IsAllTaskCompleted() ? StepTaskItems.Num() : INDEX_NONE) : StepTaskItems.IndexOfByPredicate([&InData](const FStepTaskItem& Item){ return Item.TaskName == InData.CurrentTaskName; });
	if(!InData.CurrentTaskName.IsNone() && CurrentStepTaskIndex == INDEX_NONE)
	{
		OutError = FText::FromString(TEXT("当前 StepTask 名称不存在。"));
		return false;
	}
	const float Delays[] = {InData.AutoExecuteRemaining, InData.AutoCompleteRemaining, InData.AutoLeaveRemaining, InData.GuideRemaining};
	for(float Delay : Delays)
	{
		if(!FMath::IsFinite(Delay) || Delay < -1.f)
		{
			OutError = FText::FromString(TEXT("步骤延迟记录无效。"));
			return false;
		}
	}
	if(!InData.CustomData.IsEmpty())
	{
		FMemoryReader Reader(InData.CustomData, true);
		FObjectAndNameAsStringProxyArchive Archive(Reader, true);
		Archive.ArIsSaveGame = true;
		Serialize(Archive);
		if(Archive.IsError())
		{
			OutError = FText::FromString(TEXT("步骤自定义 SaveGame 数据损坏。"));
			return false;
		}
	}
	SetRuntimeState(InData.State, false);
	StepExecuteResult = InData.Result;
	AutoExecuteRemaining = InData.AutoExecuteRemaining;
	AutoCompleteRemaining = InData.AutoCompleteRemaining;
	AutoLeaveRemaining = InData.AutoLeaveRemaining;
	GuideRemaining = InData.GuideRemaining;
	return true;
}

void UStepBase::ResumeRuntime()
{
	if(IsEntered())
	{
		ApplyCameraAction();
	}
	K2_OnResume(StepState);
}

#if WITH_EDITOR
bool UStepBase::CanEditChange(const FProperty* InProperty) const
{
	return !Runtime && Super::CanEditChange(InProperty);
}
#endif


void UStepBase::Restore()
{
	if(UStepModule* Module = GetStepModule())
	{
		Module->RestoreStep(this);
	}
}

void UStepBase::Enter()
{
	if(UStepModule* Module = GetStepModule())
	{
		Module->EnterStep(this);
	}
}

void UStepBase::Refresh()
{
	if(UStepModule* Module = GetStepModule())
	{
		Module->RefreshStep(this);
	}
}

void UStepBase::Guide()
{
	if(UStepModule* Module = GetStepModule())
	{
		Module->GuideStep(this);
	}
}

void UStepBase::Execute()
{
	if(UStepModule* Module = GetStepModule())
	{
		Module->ExecuteStep(this);
	}
}

void UStepBase::Leave()
{
	if(UStepModule* Module = GetStepModule())
	{
		Module->LeaveStep(this);
	}
}

void UStepBase::Complete(EStepExecuteResult InStepExecuteResult)
{
	if(UStepModule* Module = GetStepModule())
	{
		Module->CompleteStep(this, InStepExecuteResult);
	}
}
