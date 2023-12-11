// Fill out your copyright notice in the Description page of Project Settings.


#include "Step/Base/StepBase.h"

#include "Camera/CameraModuleStatics.h"
#include "Event/EventModuleStatics.h"
#include "Event/Handle/Step/EventHandle_CompleteStep.h"
#include "Event/Handle/Step/EventHandle_EnterStep.h"
#include "Event/Handle/Step/EventHandle_ExecuteStep.h"
#include "Event/Handle/Step/EventHandle_LeaveStep.h"
#include "Step/StepModule.h"
#include "Step/StepModuleStatics.h"

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
	TrackTargetMode = ECameraTrackMode::LocationOnly;
	CameraViewParams = FCameraViewParams();

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

	WHDebug(FString::Printf(TEXT("进入步骤: %s"), *StepDisplayName.ToString()), EDM_All, EDC_Step, EDV_Log, FColor::Cyan, 5.f);

	K2_OnEnter(InLastStep);

	ResetCameraView();

	if(bTrackTarget)
	{
		UCameraModuleStatics::StartTrackTarget(OperationTarget.LoadSynchronous(), TrackTargetMode, CameraViewParams.CameraViewSpace,
			CameraViewParams.CameraViewOffset, FVector(-1.f), CameraViewParams.CameraViewYaw,
			CameraViewParams.CameraViewPitch, CameraViewParams.CameraViewDistance, true, CameraViewParams.CameraViewMode == ECameraViewMode::Instant);
	}

	switch(StepGuideType)
	{
		case EStepGuideType::TimerOnce:
		{
			GetWorld()->GetTimerManager().SetTimer(StartGuideTimerHandle, this, &UStepBase::Guide, StepGuideIntervalTime, false);
			break;
		}
		default: break;
	}

	UEventModuleStatics::BroadcastEvent(UEventHandle_EnterStep::StaticClass(), this, {this});

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
		UCameraModuleStatics::StartTrackTarget(OperationTarget.LoadSynchronous());
	}

	UEventModuleStatics::BroadcastEvent(UEventHandle_ExecuteStep::StaticClass(), this, {this});

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
		UCameraModuleStatics::EndTrackTarget();
	}
	
	K2_OnComplete(InStepExecuteResult);

	UEventModuleStatics::BroadcastEvent(UEventHandle_CompleteStep::StaticClass(), this, {this});

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

	if(bTrackTarget)
	{
		UCameraModuleStatics::EndTrackTarget(OperationTarget.LoadSynchronous());
	}

	GetWorld()->GetTimerManager().ClearTimer(AutoLeaveTimerHandle);
	
	WHDebug(FString::Printf(TEXT("%s步骤: %s"), StepExecuteResult != EStepExecuteResult::Skipped ? TEXT("离开") : TEXT("跳过"), *StepDisplayName.ToString()), EDM_All, EDC_Step, EDV_Log, FColor::Orange, 5.f);

	K2_OnLeave();

	UEventModuleStatics::BroadcastEvent(UEventHandle_LeaveStep::StaticClass(), this, {this});

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

UStepAsset* UStepBase::GetStepAsset() const
{
	return Cast<UStepAsset>(GetOuter());
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
	CameraViewParams.GetCameraParams();

	Modify();
}

void UStepBase::SetCameraView(FCameraParams InCameraParams)
{
	CameraViewParams.SetCameraParams(InCameraParams);

	Modify();
}
#endif

void UStepBase::ResetCameraView()
{
	UCameraModuleStatics::SetCameraViewParams(CameraViewParams);
}

void UStepBase::SetOperationTarget(AActor* InOperationTarget, bool bResetCameraView)
{
	if(StepState == EStepState::Entered)
	{
		if(InOperationTarget)
		{
			if(bResetCameraView)
			{
				ResetCameraView();
			}
			if(bTrackTarget)
			{
				UCameraModuleStatics::StartTrackTarget(OperationTarget.LoadSynchronous(), TrackTargetMode, CameraViewParams.CameraViewSpace,
					CameraViewParams.CameraViewOffset, FVector(-1.f), CameraViewParams.CameraViewYaw,
					CameraViewParams.CameraViewPitch, CameraViewParams.CameraViewDistance, true, CameraViewParams.CameraViewMode == ECameraViewMode::Instant);
			}
		}
		else
		{
			if(bTrackTarget)
			{
				UCameraModuleStatics::EndTrackTarget(OperationTarget.LoadSynchronous());
			}
		}
	}
	OperationTarget = InOperationTarget;
}

bool UStepBase::CheckStepCondition(UStepBase* InStep) const
{
	return StepExecuteCondition == EStepExecuteResult::None || !InStep || InStep->StepExecuteResult == StepExecuteCondition;
}

EStepExecuteType UStepBase::GetStepExecuteType() const
{
	if(!HasSubStep(false))
	{
		if(UStepModule::Get().GetGlobalStepExecuteType() != EStepExecuteType::None)
		{
			return UStepModule::Get().GetGlobalStepExecuteType();
		}
	}
	return StepExecuteType;
}

EStepLeaveType UStepBase::GetStepLeaveType() const
{
	if(!HasSubStep(false))
	{
		if(UStepModule::Get().GetGlobalStepLeaveType() != EStepLeaveType::None)
		{
			return UStepModule::Get().GetGlobalStepLeaveType();
		}
	}
	return StepLeaveType;
}

EStepCompleteType UStepBase::GetStepCompleteType() const
{
	if(!HasSubStep(false))
	{
		if(UStepModule::Get().GetGlobalStepCompleteType() != EStepCompleteType::None)
		{
			return UStepModule::Get().GetGlobalStepCompleteType();
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
	UStepModuleStatics::RestoreStep(this);
}

void UStepBase::Enter()
{
	UStepModuleStatics::EnterStep(this);
}

void UStepBase::Refresh()
{
	UStepModuleStatics::RefreshStep(this);
}

void UStepBase::Guide()
{
	UStepModuleStatics::GuideStep(this);
}

void UStepBase::Execute()
{
	UStepModuleStatics::ExecuteStep(this);
}

void UStepBase::Complete(EStepExecuteResult InStepExecuteResult)
{
	UStepModuleStatics::CompleteStep(this, InStepExecuteResult);
}

void UStepBase::Leave()
{
	UStepModuleStatics::LeaveStep(this);
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

#if WITH_EDITOR
bool UStepBase::GenerateListItem(TSharedPtr<FStepListItem> OutStepListItem, const FString& InFilterText)
{
	OutStepListItem->Step = this;
	for (int32 i = 0; i < SubSteps.Num(); i++)
	{
		if(SubSteps[i])
		{
			auto Item = MakeShared<FStepListItem>();
			Item->ParentListItem = OutStepListItem;
			if(SubSteps[i]->GenerateListItem(Item, InFilterText))
			{
				OutStepListItem->SubListItems.Add(Item);
				return true;
			}
		}
	}
	if(!InFilterText.IsEmpty())
	{
		OutStepListItem->GetStates().bExpanded = true;
		return StepDisplayName.ToString().Contains(InFilterText);
	}
	return true;
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
			if(OutStepListItem->SubListItems.IsValidIndex(i))
			{
				SubSteps[i]->UpdateListItem(OutStepListItem->SubListItems[i]);
			}
		}
	}
}

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

		if(PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UStepBase, OperationTarget))
		{
			CameraViewParams.CameraViewTarget = OperationTarget.LoadSynchronous();
		}

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
			if(bFirstStep)
			{
				if(GetStepAsset()->GetFirstStep())
				{
					GetStepAsset()->GetFirstStep()->bFirstStep = false;
				}
				GetStepAsset()->SetFirstStep(this);
			}
			else if(GetStepAsset()->GetFirstStep() == this)
			{
				GetStepAsset()->SetFirstStep(nullptr);
			}
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif
