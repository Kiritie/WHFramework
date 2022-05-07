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

	bFirstProcedure = false;
	ProcedureIndex = 0;
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
	
	ProcedureEnterType = EProcedureEnterType::Automatic;

	ProcedureExecuteCondition = EProcedureExecuteResult::None;
	ProcedureExecuteResult = EProcedureExecuteResult::None;

	ProcedureExecuteType = EProcedureExecuteType::Automatic;
	AutoExecuteProcedureTime = 0.f;

	ProcedureLeaveType = EProcedureLeaveType::Automatic;
	AutoLeaveProcedureTime = 0.f;

	ProcedureCompleteType = EProcedureCompleteType::Procedure;
	AutoCompleteProcedureTime = 0.f;
	ProcedureGuideType = EProcedureGuideType::None;

	ProcedureState = EProcedureState::None;

	ProcedureListItemStates = FProcedureListItemStates();
}

#if WITH_EDITOR
void UProcedureBase::OnGenerate()
{
	
}

void UProcedureBase::OnUnGenerate()
{
	if(bFirstProcedure)
	{
		if(AProcedureModule* ProcedureModule = AMainModule::GetModuleByClass<AProcedureModule>(true))
		{
			if(ProcedureModule->GetFirstProcedure() == this)
			{
				ProcedureModule->SetFirstProcedure(nullptr);
			}
		}
	}
	ConditionalBeginDestroy();
}

void UProcedureBase::OnDuplicate(UProcedureBase* InNewProcedure)
{
	InNewProcedure->ProcedureName = ProcedureName;
	InNewProcedure->ProcedureDisplayName = ProcedureDisplayName;
	InNewProcedure->ProcedureDescription = ProcedureDescription;
	InNewProcedure->ProcedureIndex = ProcedureIndex;
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
	InNewProcedure->ProcedureEnterType = ProcedureEnterType;
	InNewProcedure->ProcedureExecuteCondition = ProcedureExecuteCondition;
	InNewProcedure->ProcedureExecuteType = ProcedureExecuteType;
	InNewProcedure->AutoExecuteProcedureTime = AutoExecuteProcedureTime;
	InNewProcedure->ProcedureCompleteType = ProcedureCompleteType;
	InNewProcedure->AutoCompleteProcedureTime = AutoCompleteProcedureTime;
	InNewProcedure->ProcedureLeaveType = ProcedureLeaveType;
	InNewProcedure->AutoLeaveProcedureTime = AutoLeaveProcedureTime;
	InNewProcedure->ProcedureGuideType = ProcedureGuideType;
	InNewProcedure->ProcedureGuideIntervalTime = ProcedureGuideIntervalTime;
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
	K2_OnInitialize();
}

void UProcedureBase::OnRestore()
{
	ProcedureState = EProcedureState::None;
	OnStateChanged(ProcedureState);

	K2_OnRestore();
}

void UProcedureBase::OnEnter(UProcedureBase* InLastProcedure)
{
	ProcedureState = EProcedureState::Entered;
	OnStateChanged(ProcedureState);

	GetWorld()->GetTimerManager().ClearTimer(AutoExecuteTimerHandle);

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
				if(AutoCompleteProcedureTime > 0.f)
				{
					GetWorld()->GetTimerManager().SetTimer(AutoCompleteTimerHandle, FTimerDelegate::CreateUObject(this, &UProcedureBase::Complete, EProcedureExecuteResult::Succeed), AutoCompleteProcedureTime, false);
				}
				else
				{
					Complete();
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
}

bool UProcedureBase::IsEntered() const
{
	return ProcedureState == EProcedureState::Entered || ProcedureState == EProcedureState::Executing;
}

bool UProcedureBase::IsCompleted() const
{
	return (ProcedureState == EProcedureState::Completed || ProcedureState == EProcedureState::Leaved);
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

bool UProcedureBase::CheckProcedureCondition(UProcedureBase* InProcedure) const
{
	return ProcedureExecuteCondition == EProcedureExecuteResult::None || !InProcedure || InProcedure->ProcedureExecuteResult == ProcedureExecuteCondition;
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

void UProcedureBase::GenerateListItem(TSharedPtr<FProcedureListItem> OutProcedureListItem)
{
	OutProcedureListItem->Procedure = this;
}

void UProcedureBase::UpdateListItem(TSharedPtr<FProcedureListItem> OutProcedureListItem)
{
	OutProcedureListItem->Procedure = this;
}

#if WITH_EDITOR
bool UProcedureBase::CanEditChange(const FProperty* InProperty) const
{
	if(InProperty)
	{
		FString PropertyName = InProperty->GetName();

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

		if(PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UProcedureBase, bFirstProcedure))
		{
			if(AProcedureModule* ProcedureModule = AMainModule::GetModuleByClass<AProcedureModule>(true))
			{
				if(bFirstProcedure)
				{
					if(ProcedureModule->GetFirstProcedure())
					{
						ProcedureModule->GetFirstProcedure()->bFirstProcedure = false;
					}
					ProcedureModule->SetFirstProcedure(this);
				}
				else if(ProcedureModule->GetFirstProcedure() == this)
				{
					ProcedureModule->SetFirstProcedure(nullptr);
				}
			}
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif
