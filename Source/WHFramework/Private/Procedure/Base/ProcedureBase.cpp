// Fill out your copyright notice in the Description page of Project Settings.


#include "Procedure/Base/ProcedureBase.h"

#include "Event/EventModuleBPLibrary.h"
#include "Event/Handle/Procedure/EventHandle_EnterProcedure.h"
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
	ProcedureType = EProcedureType::Default;
	ProcedureState = EProcedureState::None;

	OperationTarget = nullptr;

	CameraViewOffset = FVector::ZeroVector;
	CameraViewYaw = 0.f;
	CameraViewPitch = -25.f;
	CameraViewDistance = 100.f;

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
	for (auto Iter : SubProcedures)
	{
		if (Iter)
		{
			Iter->OnUnGenerate();
			Iter->ConditionalBeginDestroy();
		}
	}
	SubProcedures.Empty();
}
#endif

void UProcedureBase::OnInitialize_Implementation()
{
	for (auto Iter : SubProcedures)
	{
		if (Iter)
		{
			Iter->ParentProcedure = this;
			Iter->OnInitialize();
		}
	}
}

void UProcedureBase::OnRestore_Implementation()
{
	ChangeProcedureState(EProcedureState::None);

	CurrentSubProcedureIndex = -1;
	CurrentProcedureTaskIndex = -1;

	for (auto Iter : SubProcedures)
	{
		if (Iter)
		{
			Iter->OnRestore();
		}
	}

	for(int32 i = 0; i < ProcedureTaskItems.Num(); i++)
	{
		ProcedureTaskItems[i].TaskState = EProcedureTaskState::None;
	}
}

void UProcedureBase::OnEnter_Implementation(UProcedureBase* InLastProcedure)
{
	ChangeProcedureState(EProcedureState::Entered);

	GetWorld()->GetTimerManager().ClearTimer(AutoExecuteTimerHandle);
	
	if (ParentProcedure)
	{
		ParentProcedure->CurrentSubProcedureIndex = ProcedureIndex;
	}

	ProcedureExecuteResult = EProcedureExecuteResult::None;

	switch(ProcedureGuideType)
	{
		case EProcedureGuideType::TimerOnce:
		{
			GetWorld()->GetTimerManager().SetTimer(StartGuideTimerHandle, this, &UProcedureBase::Guide, ProcedureGuideIntervalTime, false);
			break;
		}
		default: break;
	}
	ResetCameraView();

	WH_LOG(WHProcedure, Log, TEXT("进入流程: %s"), *ProcedureDisplayName.ToString());
	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan, FString::Printf(TEXT("进入流程: %s"), *ProcedureDisplayName.ToString()));

	UEventModuleBPLibrary::BroadcastEvent(UEventHandle_EnterProcedure::StaticClass(), EEventNetType::Single, this, TArray<FParameter>{FParameter::MakeObject(this)});

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

	if (bMergeSubProcedure)
	{
		for (auto Iter : SubProcedures)
		{
			if (Iter)
			{
				Iter->OnEnter(InLastProcedure);
			}
		}
	}
}

void UProcedureBase::OnRefresh_Implementation(float DeltaSeconds)
{
	if (HasSubProcedure(false))
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
}

void UProcedureBase::OnGuide_Implementation()
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
}

void UProcedureBase::OnExecute_Implementation()
{
	ChangeProcedureState(EProcedureState::Executing);
	
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

void UProcedureBase::OnComplete_Implementation(EProcedureExecuteResult InProcedureExecuteResult)
{
	ChangeProcedureState(EProcedureState::Completed);

	ProcedureExecuteResult = InProcedureExecuteResult;

	GetWorld()->GetTimerManager().ClearTimer(AutoCompleteTimerHandle);

	GetWorld()->GetTimerManager().ClearTimer(StartGuideTimerHandle);

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

void UProcedureBase::OnLeave_Implementation()
{
	ChangeProcedureState(EProcedureState::Leaved);

	GetWorld()->GetTimerManager().ClearTimer(AutoLeaveTimerHandle);
	
	WH_LOG(WHProcedure, Log, TEXT("%s流程: %s"), ProcedureExecuteResult != EProcedureExecuteResult::Skipped ? TEXT("离开") : TEXT("跳过"), *ProcedureDisplayName.ToString());
	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Orange, FString::Printf(TEXT("%s流程: %s"), ProcedureExecuteResult != EProcedureExecuteResult::Skipped ? TEXT("离开") : TEXT("跳过"), *ProcedureDisplayName.ToString()));

	UEventModuleBPLibrary::BroadcastEvent(UEventHandle_LeaveProcedure::StaticClass(), EEventNetType::Single, this, TArray<FParameter>{FParameter::MakeObject(this)});

	if (bMergeSubProcedure)
	{
		for(auto Iter : SubProcedures)
		{
			if (Iter)
			{
				Iter->OnLeave();
			}
		}
	}

	GetWorld()->GetTimerManager().ClearTimer(AutoExecuteTimerHandle); 
	GetWorld()->GetTimerManager().ClearTimer(AutoLeaveTimerHandle);
	GetWorld()->GetTimerManager().ClearTimer(AutoCompleteTimerHandle);
}

void UProcedureBase::ChangeProcedureState(EProcedureState InProcedureState)
{
	if(ProcedureState != InProcedureState)
	{
		ProcedureState = InProcedureState;
		OnChangeProcedureState(ProcedureState);
	}
}

void UProcedureBase::OnChangeProcedureState_Implementation(EProcedureState InProcedureState)
{
	
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
	UProcedureModuleBPLibrary::CompleteProcedure(this);
}

void UProcedureBase::Leave()
{
	UProcedureModuleBPLibrary::LeaveProcedure(this);
}

#if WITH_EDITOR
void UProcedureBase::GetCameraView()
{
	AWHPlayerController* PlayerController = nullptr;
	for(const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		PlayerController = UGlobalBPLibrary::GetPlayerController<AWHPlayerController>(Context.World());
		if(PlayerController) break;
	}

	if(PlayerController)
	{
		if(OperationTarget)
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
	if(AWHPlayerController* PlayerController = UGlobalBPLibrary::GetPlayerController<AWHPlayerController>(this))
	{
		if(OperationTarget)
		{
			PlayerController->SetCameraLocation(OperationTarget->GetActorLocation() + CameraViewOffset, bCameraViewInstant);
		}
		else
		{
			PlayerController->SetCameraLocation(CameraViewOffset, bCameraViewInstant);
		}
		PlayerController->SetCameraRotationAndDistance(CameraViewYaw, CameraViewPitch, CameraViewDistance, bCameraViewInstant);
	}
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

bool UProcedureBase::HasSubProcedure(bool bIgnoreMerge) const
{
	return SubProcedures.Num() > 0 && (bIgnoreMerge || !bMergeSubProcedure);
}

UProcedureBase* UProcedureBase::GetCurrentSubProcedure() const
{
	if (SubProcedures.IsValidIndex(CurrentSubProcedureIndex))
	{
		return SubProcedures[CurrentSubProcedureIndex];
	}
	return nullptr;
}

bool UProcedureBase::IsAllSubCompleted() const
{
	for (auto Iter : SubProcedures)
	{
		if (Iter)
		{
			switch(Iter->ProcedureState)
			{
				case EProcedureState::Completed:
				case EProcedureState::Leaved:
				{
					break;
				}
				default:
				{
					return false;
				}
			}
		}
	}
	return true;
}

bool UProcedureBase::IsAllSubExecuteSucceed() const
{
	for (auto Iter : SubProcedures)
	{
		if (Iter && Iter->ProcedureExecuteResult == EProcedureExecuteResult::Failed)
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
		if (Iter.TaskState != EProcedureTaskState::Completed)
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
			PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UProcedureBase, ProcedureLeaveType))
		{
			return !HasSubProcedure(false);
		}

		if(PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UProcedureBase, AutoExecuteProcedureTime))
		{
			return !HasSubProcedure(false) && ProcedureExecuteType == EProcedureExecuteType::Automatic;
		}

		if(PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UProcedureBase, AutoCompleteProcedureTime))
		{
			return !HasSubProcedure(false) && ProcedureCompleteType == EProcedureCompleteType::Automatic;
		}

		if(PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UProcedureBase, AutoLeaveProcedureTime))
		{
			return !HasSubProcedure(false) && ProcedureLeaveType == EProcedureLeaveType::Automatic;
		}

		if(PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UProcedureBase, ProcedureGuideIntervalTime))
		{
			return ProcedureGuideType == EProcedureGuideType::TimerOnce || ProcedureGuideType == EProcedureGuideType::TimerLoop;
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
				ProcedureCompleteType = EProcedureCompleteType::Automatic;
				ProcedureLeaveType = EProcedureLeaveType::Automatic;
				AutoExecuteProcedureTime = 0.f;
				AutoCompleteProcedureTime = 0.f;
				AutoLeaveProcedureTime = 0.f;
			}
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif
