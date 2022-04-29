// Fill out your copyright notice in the Description page of Project Settings.


#include "Procedure/Base/ProcedureBase.h"

#include "Event/EventModuleBPLibrary.h"
#include "Event/Handle/Procedure/EventHandle_CompleteProcedure.h"
#include "Event/Handle/Procedure/EventHandle_EnterProcedure.h"
#include "Event/Handle/Procedure/EventHandle_ExecuteProcedure.h"
#include "Event/Handle/Procedure/EventHandle_LeaveProcedure.h"
#include "Gameplay/WHPlayerController.h"
#include "Global/GlobalBPLibrary.h"
#include "Main/MainModule.h"
#include "Main/MainModuleBPLibrary.h"
#include "Procedure/ProcedureModule.h"
#include "Procedure/ProcedureModuleBPLibrary.h"

UProcedureBase::UProcedureBase()
{
	ProcedureName = FName("ProcedureBase");
	ProcedureDisplayName = FText::FromString(TEXT("Procedure Base"));
	ProcedureDescription = FText::GetEmpty();

	ProcedureIndex = 0;
	ProcedureHierarchy = 0;
	ProcedureType = EProcedureType::Default;
	ProcedureState = EProcedureState::None;

	OperationTarget = nullptr;
	bTrackTarget = false;

	CameraViewMode = EProcedureCameraViewMode::None;
	CameraViewSpace = EProcedureCameraViewSpace::Local;
	CameraViewEaseType = EEaseType::Linear;
	CameraViewDuration = 1.f;
	CameraViewOffset = FVector::ZeroVector;
	CameraViewYaw = 0.f;
	CameraViewPitch = 0.f;
	CameraViewDistance = 0.f;

	ProcedureExecuteCondition = EProcedureExecuteResult::None;
	ProcedureExecuteResult = EProcedureExecuteResult::None;

	ProcedureExecuteType = EProcedureExecuteType::Automatic;
	AutoExecuteProcedureTime = 0.f;

	ProcedureLeaveType = EProcedureLeaveType::Automatic;
	AutoLeaveProcedureTime = 0.f;

	ProcedureCompleteType = EProcedureCompleteType::Procedure;
	AutoCompleteProcedureTime = 0.f;
	ProcedureGuideType = EProcedureGuideType::None;

	CurrentSubProcedureIndex = -1;

	CurrentProcedureTaskIndex = -1;

	bMergeSubProcedure = false;

	SubProcedures = TArray<UProcedureBase*>();

	ParentProcedure = nullptr;

	ProcedureState = EProcedureState::None;

	ProcedureListItemStates = FProcedureListItemStates();
}

#if WITH_EDITOR
void UProcedureBase::OnGenerate()
{
	
}

void UProcedureBase::OnUnGenerate()
{
	ConditionalBeginDestroy();
}

void UProcedureBase::OnDuplicate(UProcedureBase* InNewProcedure)
{
	InNewProcedure->OperationTarget = OperationTarget;
	InNewProcedure->bTrackTarget = bTrackTarget;
	InNewProcedure->CameraViewMode = CameraViewMode;
	InNewProcedure->CameraViewSpace = CameraViewSpace;
	InNewProcedure->CameraViewEaseType = CameraViewEaseType;
	InNewProcedure->CameraViewDuration = CameraViewDuration;
	InNewProcedure->CameraViewOffset = CameraViewOffset;
	InNewProcedure->CameraViewYaw = CameraViewYaw;
	InNewProcedure->CameraViewPitch = CameraViewPitch;
	InNewProcedure->CameraViewDistance = CameraViewDistance;
	InNewProcedure->ProcedureName = ProcedureName;
	InNewProcedure->ProcedureDisplayName = ProcedureDisplayName;
	InNewProcedure->ProcedureDescription = ProcedureDescription;
	InNewProcedure->ProcedureIndex = ProcedureIndex;
	InNewProcedure->ProcedureHierarchy = ProcedureHierarchy;
	InNewProcedure->ProcedureExecuteCondition = ProcedureExecuteCondition;
	InNewProcedure->ProcedureExecuteType = ProcedureExecuteType;
	InNewProcedure->AutoExecuteProcedureTime = AutoExecuteProcedureTime;
	InNewProcedure->ProcedureCompleteType = ProcedureCompleteType;
	InNewProcedure->AutoCompleteProcedureTime = AutoCompleteProcedureTime;
	InNewProcedure->ProcedureLeaveType = ProcedureLeaveType;
	InNewProcedure->AutoLeaveProcedureTime = AutoLeaveProcedureTime;
	InNewProcedure->ProcedureGuideType = ProcedureGuideType;
	InNewProcedure->ProcedureGuideIntervalTime = ProcedureGuideIntervalTime;
	InNewProcedure->ParentProcedure = ParentProcedure;
	InNewProcedure->bMergeSubProcedure = bMergeSubProcedure;
	InNewProcedure->SubProcedures = SubProcedures;
	InNewProcedure->ProcedureListItemStates = ProcedureListItemStates;
}
#endif

void UProcedureBase::OnStateChanged(EProcedureState InProcedureState)
{
	OnProcedureStateChanged.Broadcast(InProcedureState);
	K2_OnStateChanged(InProcedureState);
}

void UProcedureBase::OnInitialize()
{
	for (auto Iter : SubProcedures)
	{
		if(Iter)
		{
			Iter->ParentProcedure = this;
			Iter->OnInitialize();
		}
	}

	K2_OnInitialize();
}

void UProcedureBase::OnRestore()
{
	ProcedureState = EProcedureState::None;
	OnStateChanged(ProcedureState);

	CurrentSubProcedureIndex = -1;
	CurrentProcedureTaskIndex = -1;

	for(int32 i = 0; i < ProcedureTaskItems.Num(); i++)
	{
		ProcedureTaskItems[i].TaskState = EProcedureTaskState::None;
	}

	K2_OnRestore();

	for(int32 i = SubProcedures.Num() - 1; i >= 0; i--)
	{
		if(SubProcedures[i])
		{
			SubProcedures[i]->OnRestore();
		}
	}
}

void UProcedureBase::OnEnter(UProcedureBase* InLastProcedure)
{
	ProcedureState = EProcedureState::Entered;
	OnStateChanged(ProcedureState);

	GetWorld()->GetTimerManager().ClearTimer(AutoExecuteTimerHandle);
	
	if(ParentProcedure)
	{
		ParentProcedure->CurrentSubProcedureIndex = ProcedureIndex;
	}

	ProcedureExecuteResult = EProcedureExecuteResult::None;

	WH_LOG(WH_Procedure, Log, TEXT("进入流程: %s"), *ProcedureDisplayName.ToString());
	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan, FString::Printf(TEXT("进入流程: %s"), *ProcedureDisplayName.ToString()));

	K2_OnEnter(InLastProcedure);

	ResetCameraView();

	switch(ProcedureGuideType)
	{
		case EProcedureGuideType::TimerOnce:
		{
			GetWorld()->GetTimerManager().SetTimer(StartGuideTimerHandle, this, &UProcedureBase::Guide, ProcedureGuideIntervalTime, false);
			break;
		}
		default: break;
	}

	UEventModuleBPLibrary::BroadcastEvent(UEventHandle_EnterProcedure::StaticClass(), EEventNetType::Single, this, TArray<FParameter>{FParameter::MakeObject(this)});

	if(bMergeSubProcedure)
	{
		for (auto Iter : SubProcedures)
		{
			if(Iter)
			{
				Iter->OnEnter(InLastProcedure);
			}
		}
	}

	if(ProcedureExecuteType == EProcedureExecuteType::Automatic && ProcedureState != EProcedureState::Executing)
	{
		if(AutoExecuteProcedureTime > 0.f)
		{
			GetWorld()->GetTimerManager().SetTimer(AutoExecuteTimerHandle, this, &UProcedureBase::Execute, AutoExecuteProcedureTime, false);
		}
		else
		{
			Execute();
		}
	}
}

void UProcedureBase::OnRefresh()
{
	if(HasSubProcedure(false))
	{
		if(!IsAllSubCompleted())
		{
			const int32 Index = CurrentSubProcedureIndex + 1;
			if(SubProcedures.IsValidIndex(Index))
			{
				UProcedureBase* SubProcedure = SubProcedures[Index];
				if(SubProcedure)
				{
					if(SubProcedure->CheckProcedureCondition(GetCurrentSubProcedure()))
					{
						SubProcedure->Enter();
					}
					else
					{
						SubProcedure->Complete(EProcedureExecuteResult::Skipped);
						CurrentSubProcedureIndex++;
					}
				}
			}
		}
		else
		{
			Complete(IsAllSubExecuteSucceed() ? EProcedureExecuteResult::Succeed : EProcedureExecuteResult::Failed);
		}
	}
	else if(HasProcedureTask())
	{
		if(!IsAllTaskCompleted())
		{
			const int32 Index = CurrentProcedureTaskIndex + 1;
			if(ProcedureTaskItems.IsValidIndex(Index))
			{
				FProcedureTaskItem& TaskItem = ProcedureTaskItems[Index];
				if(TaskItem.IsValid())
				{
					switch(TaskItem.TaskState)
					{
						case EProcedureTaskState::None:
						{
							TaskItem.Prepare();
							break;
						}
						case EProcedureTaskState::Preparing:
						{
							TaskItem.TryExecute(GetWorld()->GetDeltaSeconds());
							break;
						}
						case EProcedureTaskState::Executing:
						{
							TaskItem.TryComplete(GetWorld()->GetDeltaSeconds());
							break;
						}
						case EProcedureTaskState::Completed:
						{
							CurrentProcedureTaskIndex++;
							break;
						}
					}
				}
			}
		}
		else
		{
			Complete(EProcedureExecuteResult::Succeed);
		}
	}

	K2_OnRefresh();
}

void UProcedureBase::OnGuide()
{
	ResetCameraView();

	switch(ProcedureGuideType)
	{
		case EProcedureGuideType::TimerLoop:
		{
			GetWorld()->GetTimerManager().SetTimer(StartGuideTimerHandle, this, &UProcedureBase::OnGuide, ProcedureGuideIntervalTime, false);
			break;
		}
		default: break;
	}

	K2_OnGuide();
}

void UProcedureBase::OnExecute()
{
	ProcedureState = EProcedureState::Executing;
	OnStateChanged(ProcedureState);

	GetWorld()->GetTimerManager().ClearTimer(AutoExecuteTimerHandle);

	K2_OnExecute();

	if(bTrackTarget && OperationTarget)
	{
		if(AWHPlayerController* PlayerController = UGlobalBPLibrary::GetPlayerController<AWHPlayerController>(this))
		{
			PlayerController->StartTrackTarget(OperationTarget);
		}
	}

	UEventModuleBPLibrary::BroadcastEvent(UEventHandle_ExecuteProcedure::StaticClass(), EEventNetType::Single, this, TArray<FParameter>{FParameter::MakeObject(this)});

	if(ProcedureState != EProcedureState::Completed)
	{
		switch(GetProcedureCompleteType())
		{
			case EProcedureCompleteType::Automatic:
			{
				if(!HasSubProcedure(false))
				{
					if(AutoCompleteProcedureTime > 0.f)
					{
						GetWorld()->GetTimerManager().SetTimer(AutoCompleteTimerHandle, FTimerDelegate::CreateUObject(this, &UProcedureBase::Complete, EProcedureExecuteResult::Succeed), AutoCompleteProcedureTime, false);
					}
					else
					{
						Complete();
					}
				}
				break;
			}
			case EProcedureCompleteType::Skip:
			{
				Complete(EProcedureExecuteResult::Skipped);
			}
			default: break;
		}
	}
}

void UProcedureBase::OnComplete(EProcedureExecuteResult InProcedureExecuteResult)
{
	ProcedureState = EProcedureState::Completed;
	OnStateChanged(ProcedureState);

	ProcedureExecuteResult = InProcedureExecuteResult;

	GetWorld()->GetTimerManager().ClearTimer(AutoCompleteTimerHandle);

	GetWorld()->GetTimerManager().ClearTimer(StartGuideTimerHandle);

	if(bTrackTarget && OperationTarget)
	{
		if(AWHPlayerController* PlayerController = UGlobalBPLibrary::GetPlayerController<AWHPlayerController>(this))
		{
			PlayerController->EndTrackTarget();
		}
	}
	
	K2_OnComplete(InProcedureExecuteResult);

	UEventModuleBPLibrary::BroadcastEvent(UEventHandle_CompleteProcedure::StaticClass(), EEventNetType::Single, this, TArray<FParameter>{FParameter::MakeObject(this)});

	if(GetProcedureLeaveType() == EProcedureLeaveType::Automatic && ProcedureState != EProcedureState::Leaved)
	{
		if(ProcedureExecuteResult != EProcedureExecuteResult::Skipped && AutoLeaveProcedureTime > 0.f)
		{
			GetWorld()->GetTimerManager().SetTimer(AutoLeaveTimerHandle, this, &UProcedureBase::Leave, AutoLeaveProcedureTime, false);
		}
		else
		{
			Leave();
		}
	}
}

void UProcedureBase::OnLeave()
{
	ProcedureState = EProcedureState::Leaved;
	OnStateChanged(ProcedureState);

	GetWorld()->GetTimerManager().ClearTimer(AutoLeaveTimerHandle);
	
	WH_LOG(WH_Procedure, Log, TEXT("%s流程: %s"), ProcedureExecuteResult != EProcedureExecuteResult::Skipped ? TEXT("离开") : TEXT("跳过"), *ProcedureDisplayName.ToString());
	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Orange, FString::Printf(TEXT("%s流程: %s"), ProcedureExecuteResult != EProcedureExecuteResult::Skipped ? TEXT("离开") : TEXT("跳过"), *ProcedureDisplayName.ToString()));

	K2_OnLeave();

	UEventModuleBPLibrary::BroadcastEvent(UEventHandle_LeaveProcedure::StaticClass(), EEventNetType::Single, this, TArray<FParameter>{FParameter::MakeObject(this)});

	if(bMergeSubProcedure)
	{
		for(auto Iter : SubProcedures)
		{
			if(Iter)
			{
				Iter->OnLeave();
			}
		}
	}
}

bool UProcedureBase::IsEntered() const
{
	return ProcedureState == EProcedureState::Entered || ProcedureState == EProcedureState::Executing;
}

bool UProcedureBase::IsCompleted(bool bCheckSubs) const
{
	return (ProcedureState == EProcedureState::Completed || ProcedureState == EProcedureState::Leaved) && (!bCheckSubs || IsAllSubCompleted());
}

bool UProcedureBase::CheckProcedureCondition(UProcedureBase* InProcedure) const
{
	return ProcedureExecuteCondition == EProcedureExecuteResult::None || !InProcedure || InProcedure->ProcedureExecuteResult == ProcedureExecuteCondition;
}

EProcedureExecuteType UProcedureBase::GetProcedureExecuteType() const
{
	if(!HasSubProcedure(false))
	{
		if(AProcedureModule* ProcedureModule = AMainModule::GetModuleByClass<AProcedureModule>())
		{
			if(ProcedureModule->GetGlobalProcedureExecuteType() != EProcedureExecuteType::None)
			{
				return ProcedureModule->GetGlobalProcedureExecuteType();
			}
		}
	}
	return ProcedureExecuteType;
}

EProcedureLeaveType UProcedureBase::GetProcedureLeaveType() const
{
	if(!HasSubProcedure(false))
	{
		if(AProcedureModule* ProcedureModule = AMainModule::GetModuleByClass<AProcedureModule>())
		{
			if(ProcedureModule->GetGlobalProcedureLeaveType() != EProcedureLeaveType::None)
			{
				return ProcedureModule->GetGlobalProcedureLeaveType();
			}
		}
	}
	return ProcedureLeaveType;
}

EProcedureCompleteType UProcedureBase::GetProcedureCompleteType() const
{
	if(!HasSubProcedure(false))
	{
		if(AProcedureModule* ProcedureModule = AMainModule::GetModuleByClass<AProcedureModule>())
		{
			if(ProcedureModule->GetGlobalProcedureCompleteType() != EProcedureCompleteType::None)
			{
				return ProcedureModule->GetGlobalProcedureCompleteType();
			}
		}
	}
	return ProcedureCompleteType;
}

bool UProcedureBase::IsParentOf(UProcedureBase* InProcedure) const
{
	if(InProcedure && InProcedure->ParentProcedure)
	{
		if(InProcedure->ParentProcedure == this) return true;
		return InProcedure->ParentProcedure->IsParentOf(InProcedure);
	}
	return false;
}

void UProcedureBase::Restore()
{
	UProcedureModuleBPLibrary::RestoreProcedure(this);
}

void UProcedureBase::Enter()
{
	UProcedureModuleBPLibrary::EnterProcedure(this);
}

void UProcedureBase::Refresh()
{
	UProcedureModuleBPLibrary::RefreshProcedure(this);
}

void UProcedureBase::Guide()
{
	UProcedureModuleBPLibrary::GuideProcedure(this);
}

void UProcedureBase::Execute()
{
	UProcedureModuleBPLibrary::ExecuteProcedure(this);
}

void UProcedureBase::Complete(EProcedureExecuteResult InProcedureExecuteResult)
{
	UProcedureModuleBPLibrary::CompleteProcedure(this, InProcedureExecuteResult);
}

void UProcedureBase::Leave()
{
	UProcedureModuleBPLibrary::LeaveProcedure(this);
}

#if WITH_EDITOR
void UProcedureBase::GetCameraView()
{
	AWHPlayerController* PlayerController = UGlobalBPLibrary::GetObjectInExistedWorld<AWHPlayerController>([](const UWorld* World) {
		return UGlobalBPLibrary::GetPlayerController<AWHPlayerController>(World);
	});

	if(PlayerController)
	{
		if(CameraViewSpace == EProcedureCameraViewSpace::Local && OperationTarget)
		{
			CameraViewOffset = PlayerController->GetCurrentCameraLocation() - OperationTarget->GetActorLocation();
		}
		else
		{
			CameraViewOffset = PlayerController->GetCurrentCameraLocation();
		}
		CameraViewYaw = PlayerController->GetCurrentCameraRotation().Yaw;
		CameraViewPitch = PlayerController->GetCurrentCameraRotation().Pitch;
		CameraViewDistance = PlayerController->GetCurrentCameraDistance();

		Modify();
	}
}
#endif

void UProcedureBase::ResetCameraView()
{
	if(CameraViewMode == EProcedureCameraViewMode::None) return;
	
	if(AWHPlayerController* PlayerController = UGlobalBPLibrary::GetPlayerController<AWHPlayerController>(this))
	{
		if(CameraViewMode == EProcedureCameraViewMode::Duration)
		{
			PlayerController->DoCameraLocation(CameraViewSpace == EProcedureCameraViewSpace::Local && OperationTarget ? OperationTarget->GetActorLocation() + CameraViewOffset : CameraViewOffset, CameraViewDuration, CameraViewEaseType);
			PlayerController->DoCameraRotation(CameraViewYaw, CameraViewPitch, CameraViewDuration, CameraViewEaseType);
			PlayerController->DoCameraDistance(CameraViewDistance, CameraViewDuration, CameraViewEaseType);
		}
		else
		{
			PlayerController->SetCameraLocation(CameraViewSpace == EProcedureCameraViewSpace::Local && OperationTarget ? OperationTarget->GetActorLocation() + CameraViewOffset : CameraViewOffset, CameraViewMode == EProcedureCameraViewMode::Instant);
			PlayerController->SetCameraRotation(CameraViewYaw, CameraViewPitch, CameraViewMode == EProcedureCameraViewMode::Instant);
			PlayerController->SetCameraDistance(CameraViewDistance, CameraViewMode == EProcedureCameraViewMode::Instant);
		}
	}
}

bool UProcedureBase::HasSubProcedure(bool bIgnoreMerge) const
{
	return SubProcedures.Num() > 0 && (bIgnoreMerge || !bMergeSubProcedure);
}

UProcedureBase* UProcedureBase::GetCurrentSubProcedure() const
{
	if(SubProcedures.IsValidIndex(CurrentSubProcedureIndex))
	{
		return SubProcedures[CurrentSubProcedureIndex];
	}
	return nullptr;
}

bool UProcedureBase::IsSubOf(UProcedureBase* InProcedure) const
{
	if(InProcedure && ParentProcedure)
	{
		if(InProcedure == ParentProcedure) return true;
		return ParentProcedure->IsSubOf(InProcedure);
	}
	return false;
}

bool UProcedureBase::IsAllSubCompleted() const
{
	for (auto Iter : SubProcedures)
	{
		if(Iter && !Iter->IsCompleted())
		{
			return false;
		}
	}
	return true;
}

bool UProcedureBase::IsAllSubExecuteSucceed() const
{
	for (auto Iter : SubProcedures)
	{
		if(Iter && Iter->ProcedureExecuteResult == EProcedureExecuteResult::Failed)
		{
			return false;
		}
	}
	return true;
}

bool UProcedureBase::HasProcedureTask() const
{
	return ProcedureTaskItems.Num() > 0;
}

bool UProcedureBase::IsAllTaskCompleted() const
{
	for (auto Iter : ProcedureTaskItems)
	{
		if(Iter.TaskState != EProcedureTaskState::Completed)
		{
			return false;
		}
	}
	return true;
}

FProcedureTaskItem& UProcedureBase::AddProcedureTask(const FName InTaskName, float InDurationTime, float InStartDelayTime)
{
	ProcedureTaskItems.Add(FProcedureTaskItem(InTaskName, InDurationTime, InStartDelayTime));
	return ProcedureTaskItems[ProcedureTaskItems.Num() - 1];
}

void UProcedureBase::GenerateListItem(TSharedPtr<FProcedureListItem> OutProcedureListItem)
{
	OutProcedureListItem->Procedure = this;
	for (int32 i = 0; i < SubProcedures.Num(); i++)
	{
		if(SubProcedures[i])
		{
			auto Item = MakeShared<FProcedureListItem>();
			Item->ParentListItem = OutProcedureListItem;
			OutProcedureListItem->SubListItems.Add(Item);
			SubProcedures[i]->GenerateListItem(Item);
		}
	}
}

void UProcedureBase::UpdateListItem(TSharedPtr<FProcedureListItem> OutProcedureListItem)
{
	OutProcedureListItem->Procedure = this;
	for (int32 i = 0; i < SubProcedures.Num(); i++)
	{
		if(SubProcedures[i])
		{
			SubProcedures[i]->ProcedureIndex = i;
			SubProcedures[i]->ProcedureHierarchy = ProcedureHierarchy + 1;
			SubProcedures[i]->ParentProcedure = this;
			SubProcedures[i]->UpdateListItem(OutProcedureListItem->SubListItems[i]);
		}
	}
}
#if WITH_EDITOR
bool UProcedureBase::CanEditChange(const FProperty* InProperty) const
{
	if(InProperty)
	{
		FString PropertyName = InProperty->GetName();

		if(PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UProcedureBase, ProcedureExecuteType) ||
			PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UProcedureBase, ProcedureCompleteType) ||
			PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UProcedureBase, ProcedureLeaveType) ||
			PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UProcedureBase, ProcedureGuideType))
		{
			return !HasSubProcedure(false);
		}

		if(PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UProcedureBase, AutoExecuteProcedureTime))
		{
			return ProcedureExecuteType == EProcedureExecuteType::Automatic;
		}

		if(PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UProcedureBase, AutoCompleteProcedureTime))
		{
			return ProcedureCompleteType == EProcedureCompleteType::Automatic;
		}

		if(PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UProcedureBase, AutoLeaveProcedureTime))
		{
			return ProcedureLeaveType == EProcedureLeaveType::Automatic;
		}

		if(PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UProcedureBase, ProcedureGuideIntervalTime))
		{
			return !HasSubProcedure(false) && (ProcedureGuideType == EProcedureGuideType::TimerOnce || ProcedureGuideType == EProcedureGuideType::TimerLoop);
		}
	}

	return Super::CanEditChange(InProperty);
}

void UProcedureBase::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	FProperty* Property = PropertyChangedEvent.MemberProperty;

	if(Property && PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive)
	{
		auto PropertyName = Property->GetFName();

		if(PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UProcedureBase, SubProcedures) ||
			PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UProcedureBase, bMergeSubProcedure))
		{
			if(HasSubProcedure(false))
			{
				ProcedureExecuteType = EProcedureExecuteType::Automatic;
				ProcedureCompleteType = EProcedureCompleteType::Procedure;
				AutoCompleteProcedureTime = 0.f;
				ProcedureLeaveType = EProcedureLeaveType::Automatic;
				ProcedureGuideType = EProcedureGuideType::None;				
				ProcedureGuideIntervalTime = 0.f;
			}
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif
