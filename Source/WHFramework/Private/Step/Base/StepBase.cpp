// Fill out your copyright notice in the Description page of Project Settings.


#include "Step/Base/StepBase.h"

#include "Camera/CameraModuleBPLibrary.h"
#include "Event/EventModuleBPLibrary.h"
#include "Event/Handle/Step/EventHandle_CompleteStep.h"
#include "Event/Handle/Step/EventHandle_EnterStep.h"
#include "Event/Handle/Step/EventHandle_ExecuteStep.h"
#include "Event/Handle/Step/EventHandle_LeaveStep.h"
#include "Step/StepModule.h"
#include "Step/StepModuleBPLibrary.h"

UStepBase::UStepBase()
{
	StepGUID = FGuid::NewGuid().ToString();
	StepDisplayName = FText::FromString(TEXT("Step Base"));
	StepDescription = FText::GetEmpty();

	bFirstStep = false;
	StepIndex = 0;
	StepHierarchy = 0;
	StepState = EStepState::None;

	OperationTarget = nullptr;
	bTrackTarget = false;

	CameraViewPawn = nullptr;
	CameraViewMode = EStepCameraViewMode::None;
	CameraViewSpace = EStepCameraViewSpace::Local;
	CameraViewEaseType = EEaseType::Linear;
	CameraViewDuration = 1.f;
	CameraViewOffset = FVector::ZeroVector;
	CameraViewYaw = 0.f;
	CameraViewPitch = 0.f;
	CameraViewDistance = 0.f;

	StepExecuteCondition = EStepExecuteResult::None;
	StepExecuteResult = EStepExecuteResult::None;

	StepExecuteType = EStepExecuteType::Automatic;
	AutoExecuteStepTime = 0.f;

	StepLeaveType = EStepLeaveType::Automatic;
	AutoLeaveStepTime = 0.f;

	StepCompleteType = EStepCompleteType::Procedure;
	AutoCompleteStepTime = 0.f;
	StepGuideType = EStepGuideType::None;

	CurrentSubStepIndex = -1;

	CurrentStepTaskIndex = -1;

	bMergeSubStep = false;

	SubSteps = TArray<UStepBase*>();

	RootStep = nullptr;
	ParentStep = nullptr;

	StepState = EStepState::None;

	StepListItemStates = FStepListItemStates();
}

#if WITH_EDITOR
void UStepBase::OnGenerate()
{
	
}

void UStepBase::OnUnGenerate()
{
	if(bFirstStep)
	{
		if(AStepModule* StepModule = AStepModule::Get(true))
		{
			if(StepModule->GetFirstStep() == this)
			{
				StepModule->SetFirstStep(nullptr);
			}
		}
	}
}
#endif

void UStepBase::OnStateChanged(EStepState InStepState)
{
	OnStepStateChanged.Broadcast(InStepState);
	K2_OnStateChanged(InStepState);
}

void UStepBase::OnInitialize()
{
	for (auto Iter : SubSteps)
	{
		if(Iter)
		{
			Iter->RootStep = IsRootStep() ? this : RootStep;
			Iter->ParentStep = this;
			Iter->OnInitialize();
		}
	}

	K2_OnInitialize();
}

void UStepBase::OnRestore()
{
	StepState = EStepState::None;
	OnStateChanged(StepState);

	CurrentSubStepIndex = -1;
	CurrentStepTaskIndex = -1;

	for(int32 i = 0; i < StepTaskItems.Num(); i++)
	{
		StepTaskItems[i].TaskState = EStepTaskState::None;
	}

	K2_OnRestore();

	for(int32 i = SubSteps.Num() - 1; i >= 0; i--)
	{
		if(SubSteps[i])
		{
			SubSteps[i]->OnRestore();
		}
	}
}

void UStepBase::OnEnter(UStepBase* InLastStep)
{
	StepState = EStepState::Entered;
	OnStateChanged(StepState);

	GetWorld()->GetTimerManager().ClearTimer(AutoExecuteTimerHandle);
	
	if(ParentStep)
	{
		ParentStep->CurrentSubStepIndex = StepIndex;
	}

	StepExecuteResult = EStepExecuteResult::None;

	WHDebug(FString::Printf(TEXT("进入步骤: %s"), *StepDisplayName.ToString()), EDebugMode::All, EDC_Step, EDV_Log, FColor::Cyan, 5.f);

	K2_OnEnter(InLastStep);

	UCameraModuleBPLibrary::SwitchCamera(CameraViewPawn);

	ResetCameraView();

	switch(StepGuideType)
	{
		case EStepGuideType::TimerOnce:
		{
			GetWorld()->GetTimerManager().SetTimer(StartGuideTimerHandle, this, &UStepBase::Guide, StepGuideIntervalTime, false);
			break;
		}
		default: break;
	}

	UEventModuleBPLibrary::BroadcastEvent(UEventHandle_EnterStep::StaticClass(), EEventNetType::Single, this, {this});

	if(bMergeSubStep)
	{
		for (auto Iter : SubSteps)
		{
			if(Iter)
			{
				Iter->OnEnter(InLastStep);
			}
		}
	}

	if(StepExecuteType == EStepExecuteType::Automatic && StepState != EStepState::Executing)
	{
		if(AutoExecuteStepTime > 0.f)
		{
			GetWorld()->GetTimerManager().SetTimer(AutoExecuteTimerHandle, this, &UStepBase::Execute, AutoExecuteStepTime, false);
		}
		else
		{
			Execute();
		}
	}
}

void UStepBase::OnRefresh()
{
	if(HasSubStep(false))
	{
		if(!IsAllSubCompleted())
		{
			const int32 Index = CurrentSubStepIndex + 1;
			if(SubSteps.IsValidIndex(Index))
			{
				UStepBase* SubStep = SubSteps[Index];
				if(SubStep)
				{
					if(SubStep->CheckStepCondition(GetCurrentSubStep()))
					{
						SubStep->Enter();
					}
					else
					{
						SubStep->Complete(EStepExecuteResult::Skipped);
						CurrentSubStepIndex++;
					}
				}
			}
		}
		else
		{
			Complete(IsAllSubExecuteSucceed() ? EStepExecuteResult::Succeed : EStepExecuteResult::Failed);
		}
	}
	else if(HasStepTask())
	{
		if(!IsAllTaskCompleted())
		{
			const int32 Index = CurrentStepTaskIndex + 1;
			if(StepTaskItems.IsValidIndex(Index))
			{
				FStepTaskItem& TaskItem = StepTaskItems[Index];
				if(TaskItem.IsValid())
				{
					switch(TaskItem.TaskState)
					{
						case EStepTaskState::None:
						{
							TaskItem.Prepare();
							break;
						}
						case EStepTaskState::Preparing:
						{
							TaskItem.TryExecute(GetWorld()->GetDeltaSeconds());
							break;
						}
						case EStepTaskState::Executing:
						{
							TaskItem.TryComplete(GetWorld()->GetDeltaSeconds());
							break;
						}
						case EStepTaskState::Completed:
						{
							CurrentStepTaskIndex++;
							break;
						}
					}
				}
			}
		}
		else
		{
			Complete(EStepExecuteResult::Succeed);
		}
	}

	K2_OnRefresh();
}

void UStepBase::OnGuide()
{
	ResetCameraView();

	switch(StepGuideType)
	{
		case EStepGuideType::TimerLoop:
		{
			GetWorld()->GetTimerManager().SetTimer(StartGuideTimerHandle, this, &UStepBase::OnGuide, StepGuideIntervalTime, false);
			break;
		}
		default: break;
	}

	K2_OnGuide();
}

void UStepBase::OnExecute()
{
	StepState = EStepState::Executing;
	OnStateChanged(StepState);

	GetWorld()->GetTimerManager().ClearTimer(AutoExecuteTimerHandle);

	K2_OnExecute();

	if(bTrackTarget && OperationTarget)
	{
		UCameraModuleBPLibrary::StartTrackTarget(OperationTarget);
	}

	UEventModuleBPLibrary::BroadcastEvent(UEventHandle_ExecuteStep::StaticClass(), EEventNetType::Single, this, {this});

	if(StepState != EStepState::Completed)
	{
		switch(GetStepCompleteType())
		{
			case EStepCompleteType::Automatic:
			{
				if(!HasSubStep(false))
				{
					if(AutoCompleteStepTime > 0.f)
					{
						GetWorld()->GetTimerManager().SetTimer(AutoCompleteTimerHandle, FTimerDelegate::CreateUObject(this, &UStepBase::Complete, EStepExecuteResult::Succeed), AutoCompleteStepTime, false);
					}
					else
					{
						Complete();
					}
				}
				break;
			}
			case EStepCompleteType::Skip:
			{
				Complete(EStepExecuteResult::Skipped);
			}
			default: break;
		}
	}
}

void UStepBase::OnComplete(EStepExecuteResult InStepExecuteResult)
{
	StepState = EStepState::Completed;
	OnStateChanged(StepState);

	StepExecuteResult = InStepExecuteResult;

	GetWorld()->GetTimerManager().ClearTimer(AutoCompleteTimerHandle);

	GetWorld()->GetTimerManager().ClearTimer(StartGuideTimerHandle);

	if(bTrackTarget && OperationTarget)
	{
		UCameraModuleBPLibrary::EndTrackTarget();
	}
	
	K2_OnComplete(InStepExecuteResult);

	UEventModuleBPLibrary::BroadcastEvent(UEventHandle_CompleteStep::StaticClass(), EEventNetType::Single, this, {this});

	if(GetStepLeaveType() == EStepLeaveType::Automatic && StepState != EStepState::Leaved)
	{
		if(StepExecuteResult != EStepExecuteResult::Skipped && AutoLeaveStepTime > 0.f)
		{
			GetWorld()->GetTimerManager().SetTimer(AutoLeaveTimerHandle, this, &UStepBase::Leave, AutoLeaveStepTime, false);
		}
		else
		{
			Leave();
		}
	}
}

void UStepBase::OnLeave()
{
	StepState = EStepState::Leaved;
	OnStateChanged(StepState);

	GetWorld()->GetTimerManager().ClearTimer(AutoLeaveTimerHandle);
	
	WHDebug(FString::Printf(TEXT("%s步骤: %s"), StepExecuteResult != EStepExecuteResult::Skipped ? TEXT("离开") : TEXT("跳过"), *StepDisplayName.ToString()), EDebugMode::All, EDC_Step, EDV_Log, FColor::Orange, 5.f);

	K2_OnLeave();

	UEventModuleBPLibrary::BroadcastEvent(UEventHandle_LeaveStep::StaticClass(), EEventNetType::Single, this, {this});

	if(bMergeSubStep)
	{
		for(auto Iter : SubSteps)
		{
			if(Iter)
			{
				Iter->OnLeave();
			}
		}
	}
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

#if WITH_EDITOR
void UStepBase::GetCameraView()
{
	if(CameraViewSpace == EStepCameraViewSpace::Local && OperationTarget)
	{
		CameraViewOffset = UCameraModuleBPLibrary::GetCameraLocation() - OperationTarget->GetActorLocation();
		CameraViewYaw = UCameraModuleBPLibrary::GetCameraRotation().Yaw - OperationTarget->GetActorRotation().Yaw;
		CameraViewPitch = UCameraModuleBPLibrary::GetCameraRotation().Pitch - OperationTarget->GetActorRotation().Pitch;
	}
	else
	{
		CameraViewOffset = UCameraModuleBPLibrary::GetCameraLocation();
		CameraViewYaw = UCameraModuleBPLibrary::GetCameraRotation().Yaw;
		CameraViewPitch = UCameraModuleBPLibrary::GetCameraRotation().Pitch;
	}
	CameraViewDistance = UCameraModuleBPLibrary::GetCameraDistance();

	Modify();
}

void UStepBase::SetCameraView(FCameraParams InCameraParams)
{
	if(CameraViewSpace == EStepCameraViewSpace::Local && OperationTarget)
	{
		CameraViewOffset = InCameraParams.CameraLocation - OperationTarget->GetActorLocation();
		CameraViewYaw = InCameraParams.CameraRotation.Yaw - OperationTarget->GetActorRotation().Yaw;
		CameraViewPitch = InCameraParams.CameraRotation.Pitch - OperationTarget->GetActorRotation().Pitch;
	}
	else
	{
		CameraViewOffset = InCameraParams.CameraLocation;
		CameraViewYaw = InCameraParams.CameraRotation.Yaw;
		CameraViewPitch = InCameraParams.CameraRotation.Pitch;
	}
	CameraViewDistance = InCameraParams.CameraDistance;

	Modify();
}
#endif

void UStepBase::ResetCameraView()
{
	FVector CameraLocation;
	float CameraYaw;
	float CameraPitch;
	float CameraDistance;
	if(CameraViewSpace == EStepCameraViewSpace::Local && OperationTarget)
	{
		CameraLocation = OperationTarget->GetActorLocation() + CameraViewOffset;
		CameraYaw = OperationTarget->GetActorRotation().Yaw + CameraViewYaw;
		CameraPitch = OperationTarget->GetActorRotation().Pitch + CameraViewPitch;
		CameraDistance = CameraViewDistance;
	}
	else
	{
		CameraLocation = CameraViewOffset;
		CameraYaw = CameraViewYaw;
		CameraPitch = CameraViewPitch;
		CameraDistance = CameraViewDistance;
	}
	switch(CameraViewMode)
	{
		case EStepCameraViewMode::Instant:
		{
			UCameraModuleBPLibrary::SetCameraLocation(CameraLocation, true);
			UCameraModuleBPLibrary::SetCameraRotation(CameraViewYaw, CameraViewPitch, true);
			UCameraModuleBPLibrary::SetCameraDistance(CameraViewDistance, true);
			break;
		}
		case EStepCameraViewMode::Smooth:
		{
			UCameraModuleBPLibrary::SetCameraLocation(CameraLocation, false);
			UCameraModuleBPLibrary::SetCameraRotation(CameraViewYaw, CameraViewPitch, false);
			UCameraModuleBPLibrary::SetCameraDistance(CameraViewDistance, false);
			break;
		}
		case EStepCameraViewMode::Duration:
		{
			UCameraModuleBPLibrary::DoCameraLocation(CameraLocation, CameraViewDuration, CameraViewEaseType);
			UCameraModuleBPLibrary::DoCameraRotation(CameraYaw, CameraPitch, CameraViewDuration, CameraViewEaseType);
			UCameraModuleBPLibrary::DoCameraDistance(CameraDistance, CameraViewDuration, CameraViewEaseType);
			break;
		}
		default: break;
	}
}

bool UStepBase::CheckStepCondition(UStepBase* InStep) const
{
	return StepExecuteCondition == EStepExecuteResult::None || !InStep || InStep->StepExecuteResult == StepExecuteCondition;
}

EStepExecuteType UStepBase::GetStepExecuteType() const
{
	if(!HasSubStep(false))
	{
		if(AStepModule* StepModule = AStepModule::Get())
		{
			if(StepModule->GetGlobalStepExecuteType() != EStepExecuteType::None)
			{
				return StepModule->GetGlobalStepExecuteType();
			}
		}
	}
	return StepExecuteType;
}

EStepLeaveType UStepBase::GetStepLeaveType() const
{
	if(!HasSubStep(false))
	{
		if(AStepModule* StepModule = AStepModule::Get())
		{
			if(StepModule->GetGlobalStepLeaveType() != EStepLeaveType::None)
			{
				return StepModule->GetGlobalStepLeaveType();
			}
		}
	}
	return StepLeaveType;
}

EStepCompleteType UStepBase::GetStepCompleteType() const
{
	if(!HasSubStep(false))
	{
		if(AStepModule* StepModule = AStepModule::Get())
		{
			if(StepModule->GetGlobalStepCompleteType() != EStepCompleteType::None)
			{
				return StepModule->GetGlobalStepCompleteType();
			}
		}
	}
	return StepCompleteType;
}

bool UStepBase::IsParentOf(UStepBase* InStep) const
{
	if(InStep && InStep->ParentStep)
	{
		if(InStep->ParentStep == this) return true;
		return InStep->ParentStep->IsParentOf(InStep);
	}
	return false;
}

void UStepBase::Restore()
{
	UStepModuleBPLibrary::RestoreStep(this);
}

void UStepBase::Enter()
{
	UStepModuleBPLibrary::EnterStep(this);
}

void UStepBase::Refresh()
{
	UStepModuleBPLibrary::RefreshStep(this);
}

void UStepBase::Guide()
{
	UStepModuleBPLibrary::GuideStep(this);
}

void UStepBase::Execute()
{
	UStepModuleBPLibrary::ExecuteStep(this);
}

void UStepBase::Complete(EStepExecuteResult InStepExecuteResult)
{
	UStepModuleBPLibrary::CompleteStep(this, InStepExecuteResult);
}

void UStepBase::Leave()
{
	UStepModuleBPLibrary::LeaveStep(this);
}

bool UStepBase::HasSubStep(bool bIgnoreMerge) const
{
	return SubSteps.Num() > 0 && (bIgnoreMerge || !bMergeSubStep);
}

UStepBase* UStepBase::GetCurrentSubStep() const
{
	if(SubSteps.IsValidIndex(CurrentSubStepIndex))
	{
		return SubSteps[CurrentSubStepIndex];
	}
	return nullptr;
}

bool UStepBase::IsSubOf(UStepBase* InStep) const
{
	if(InStep && ParentStep)
	{
		if(InStep == ParentStep) return true;
		return ParentStep->IsSubOf(InStep);
	}
	return false;
}

bool UStepBase::IsAllSubCompleted() const
{
	for (auto Iter : SubSteps)
	{
		if(Iter && !Iter->IsCompleted())
		{
			return false;
		}
	}
	return true;
}

bool UStepBase::IsAllSubExecuteSucceed() const
{
	for (auto Iter : SubSteps)
	{
		if(Iter && Iter->StepExecuteResult == EStepExecuteResult::Failed)
		{
			return false;
		}
	}
	return true;
}

bool UStepBase::HasStepTask() const
{
	return StepTaskItems.Num() > 0;
}

bool UStepBase::IsAllTaskCompleted() const
{
	for (auto Iter : StepTaskItems)
	{
		if(Iter.TaskState != EStepTaskState::Completed)
		{
			return false;
		}
	}
	return true;
}

FStepTaskItem& UStepBase::AddStepTask(const FName InTaskName, float InDurationTime, float InStartDelayTime)
{
	StepTaskItems.Add(FStepTaskItem(InTaskName, InDurationTime, InStartDelayTime));
	return StepTaskItems[StepTaskItems.Num() - 1];
}

void UStepBase::GenerateListItem(TSharedPtr<FStepListItem> OutStepListItem)
{
	OutStepListItem->Step = this;
	for (int32 i = 0; i < SubSteps.Num(); i++)
	{
		if(SubSteps[i])
		{
			auto Item = MakeShared<FStepListItem>();
			Item->ParentListItem = OutStepListItem;
			OutStepListItem->SubListItems.Add(Item);
			SubSteps[i]->GenerateListItem(Item);
		}
	}
}

void UStepBase::UpdateListItem(TSharedPtr<FStepListItem> OutStepListItem)
{
	OutStepListItem->Step = this;
	for (int32 i = 0; i < SubSteps.Num(); i++)
	{
		if(SubSteps[i])
		{
			SubSteps[i]->StepIndex = i;
			SubSteps[i]->StepHierarchy = StepHierarchy + 1;
			SubSteps[i]->RootStep = IsRootStep() ? this : RootStep;
			SubSteps[i]->ParentStep = this;
			SubSteps[i]->UpdateListItem(OutStepListItem->SubListItems[i]);
		}
	}
}
#if WITH_EDITOR
bool UStepBase::CanEditChange(const FProperty* InProperty) const
{
	if(InProperty)
	{
		FString PropertyName = InProperty->GetName();

		if(PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UStepBase, StepExecuteType) ||
			PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UStepBase, StepCompleteType) ||
			PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UStepBase, StepLeaveType) ||
			PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UStepBase, StepGuideType))
		{
			return !HasSubStep(false);
		}

		if(PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UStepBase, AutoExecuteStepTime))
		{
			return StepExecuteType == EStepExecuteType::Automatic;
		}

		if(PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UStepBase, AutoCompleteStepTime))
		{
			return StepCompleteType == EStepCompleteType::Automatic;
		}

		if(PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UStepBase, AutoLeaveStepTime))
		{
			return StepLeaveType == EStepLeaveType::Automatic;
		}

		if(PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UStepBase, StepGuideIntervalTime))
		{
			return !HasSubStep(false) && (StepGuideType == EStepGuideType::TimerOnce || StepGuideType == EStepGuideType::TimerLoop);
		}
	}

	return Super::CanEditChange(InProperty);
}

void UStepBase::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	FProperty* Property = PropertyChangedEvent.MemberProperty;

	if(Property && PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive)
	{
		auto PropertyName = Property->GetFName();

		if(PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UStepBase, SubSteps) ||
			PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UStepBase, bMergeSubStep))
		{
			if(HasSubStep(false))
			{
				StepExecuteType = EStepExecuteType::Automatic;
				StepCompleteType = EStepCompleteType::Procedure;
				AutoCompleteStepTime = 0.f;
				StepLeaveType = EStepLeaveType::Automatic;
				StepGuideType = EStepGuideType::None;				
				StepGuideIntervalTime = 0.f;
			}
		}

		if(PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UStepBase, bFirstStep))
		{
			if(AStepModule* StepModule = AStepModule::Get(true))
			{
				if(bFirstStep)
				{
					if(StepModule->GetFirstStep())
					{
						StepModule->GetFirstStep()->bFirstStep = false;
					}
					StepModule->SetFirstStep(this);
				}
				else if(StepModule->GetFirstStep() == this)
				{
					StepModule->SetFirstStep(nullptr);
				}
			}
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif
