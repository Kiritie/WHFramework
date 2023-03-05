// Fill out your copyright notice in the Description page of Project Settings.


#include "Procedure/Base/ProcedureBase.h"

#include "Camera/CameraModuleBPLibrary.h"
#include "Event/EventModuleBPLibrary.h"
#include "Event/Handle/Procedure/EventHandle_EnterProcedure.h"
#include "Event/Handle/Procedure/EventHandle_LeaveProcedure.h"
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
	ProcedureGuideType = EProcedureGuideType::None;

	OperationTarget = nullptr;
	bTrackTarget = false;
	TrackTargetMode = ETrackTargetMode::LocationOnly;

	CameraViewPawn = nullptr;
	CameraViewMode = EProcedureCameraViewMode::None;
	CameraViewSpace = EProcedureCameraViewSpace::Local;
	CameraViewEaseType = EEaseType::Linear;
	CameraViewDuration = 1.f;
	CameraViewOffset = FVector::ZeroVector;
	CameraViewYaw = 0.f;
	CameraViewPitch = 0.f;
	CameraViewDistance = 0.f;

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
		if(AProcedureModule* ProcedureModule = AProcedureModule::Get(true))
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
	InNewProcedure->TrackTargetMode = TrackTargetMode;
	InNewProcedure->CameraViewPawn = CameraViewPawn;
	InNewProcedure->CameraViewMode = CameraViewMode;
	InNewProcedure->CameraViewSpace = CameraViewSpace;
	InNewProcedure->CameraViewEaseType = CameraViewEaseType;
	InNewProcedure->CameraViewDuration = CameraViewDuration;
	InNewProcedure->CameraViewOffset = CameraViewOffset;
	InNewProcedure->CameraViewYaw = CameraViewYaw;
	InNewProcedure->CameraViewPitch = CameraViewPitch;
	InNewProcedure->CameraViewDistance = CameraViewDistance;
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

void UProcedureBase::OnEnter(UProcedureBase* InLastProcedure)
{
	ProcedureState = EProcedureState::Entered;
	OnStateChanged(ProcedureState);

	WHLog(WH_Procedure, Log, TEXT("进入流程: %s"), *ProcedureDisplayName.ToString());
	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan, FString::Printf(TEXT("进入流程: %s"), *ProcedureDisplayName.ToString()));

	K2_OnEnter(InLastProcedure);

	UCameraModuleBPLibrary::SwitchCamera(CameraViewPawn);

	if(bTrackTarget)
	{
		UCameraModuleBPLibrary::StartTrackTarget(OperationTarget, TrackTargetMode, static_cast<ETrackTargetSpace>(CameraViewSpace), CameraViewOffset, CameraViewYaw, CameraViewPitch, CameraViewDistance, true, CameraViewMode == EProcedureCameraViewMode::Instant);
	}

	ResetCameraView();

	switch(ProcedureGuideType)
	{
		case EProcedureGuideType::TimerOnce:
		{
			GetWorld()->GetTimerManager().SetTimer(StartGuideTimerHandle, this, &UProcedureBase::OnGuide, ProcedureGuideIntervalTime, false);
			break;
		}
		default: break;
	}

	UEventModuleBPLibrary::BroadcastEvent(UEventHandle_EnterProcedure::StaticClass(), EEventNetType::Single, this, {FParameter::MakeObject(this)});
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

void UProcedureBase::OnLeave(UProcedureBase* InNextProcedure)
{
	ProcedureState = EProcedureState::Leaved;
	OnStateChanged(ProcedureState);
	
	WHLog(WH_Procedure, Log, TEXT("离开流程: %s"), *ProcedureDisplayName.ToString());
	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Orange, FString::Printf(TEXT("离开流程: %s"), *ProcedureDisplayName.ToString()));

	K2_OnLeave(InNextProcedure);

	UEventModuleBPLibrary::BroadcastEvent(UEventHandle_LeaveProcedure::StaticClass(), EEventNetType::Single, this, {FParameter::MakeObject(this)});
}

void UProcedureBase::Guide()
{
	if(ProcedureState == EProcedureState::Entered)
	{
		OnGuide();
	}
}

void UProcedureBase::SwitchLast()
{
	UProcedureModuleBPLibrary::SwitchLastProcedure();
}

void UProcedureBase::SwitchNext()
{
	UProcedureModuleBPLibrary::SwitchNextProcedure();
}

bool UProcedureBase::IsCurrent()
{
	return UProcedureModuleBPLibrary::IsCurrentProcedure(this);
}

#if WITH_EDITOR
void UProcedureBase::GetCameraView()
{
	if(CameraViewSpace == EProcedureCameraViewSpace::Local && OperationTarget)
	{
		CameraViewOffset = UCameraModuleBPLibrary::GetCurrentCameraLocation() - OperationTarget->GetActorLocation();
		CameraViewYaw = UCameraModuleBPLibrary::GetCurrentCameraRotation().Yaw - OperationTarget->GetActorRotation().Yaw;
		CameraViewPitch = UCameraModuleBPLibrary::GetCurrentCameraRotation().Pitch - OperationTarget->GetActorRotation().Pitch;
	}
	else
	{
		CameraViewOffset = UCameraModuleBPLibrary::GetCurrentCameraLocation();
		CameraViewYaw = UCameraModuleBPLibrary::GetCurrentCameraRotation().Yaw;
		CameraViewPitch = UCameraModuleBPLibrary::GetCurrentCameraRotation().Pitch;
	}
	CameraViewDistance = UCameraModuleBPLibrary::GetCurrentCameraDistance();

	Modify();
}

void UProcedureBase::SetCameraView(FCameraParams InCameraParams)
{
	if(CameraViewSpace == EProcedureCameraViewSpace::Local && OperationTarget)
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

void UProcedureBase::ResetCameraView()
{
	FVector CameraLocation;
	float CameraYaw;
	float CameraPitch;
	float CameraDistance;
	if(CameraViewSpace == EProcedureCameraViewSpace::Local && OperationTarget)
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
		case EProcedureCameraViewMode::Instant:
		{
			UCameraModuleBPLibrary::SetCameraLocation(CameraLocation, true);
			UCameraModuleBPLibrary::SetCameraRotation(CameraViewYaw, CameraViewPitch, true);
			UCameraModuleBPLibrary::SetCameraDistance(CameraViewDistance, true);
			break;
		}
		case EProcedureCameraViewMode::Smooth:
		{
			UCameraModuleBPLibrary::SetCameraLocation(CameraLocation, false);
			UCameraModuleBPLibrary::SetCameraRotation(CameraViewYaw, CameraViewPitch, false);
			UCameraModuleBPLibrary::SetCameraDistance(CameraViewDistance, false);
			break;
		}
		case EProcedureCameraViewMode::Duration:
		{
			UCameraModuleBPLibrary::DoCameraLocation(CameraLocation, CameraViewDuration, CameraViewEaseType);
			UCameraModuleBPLibrary::DoCameraRotation(CameraYaw, CameraPitch, CameraViewDuration, CameraViewEaseType);
			UCameraModuleBPLibrary::DoCameraDistance(CameraDistance, CameraViewDuration, CameraViewEaseType);
			break;
		}
		default: break;
	}
}

void UProcedureBase::SetOperationTarget(AActor* InOperationTarget, bool bResetCameraView)
{
	if(OperationTarget != InOperationTarget)
	{
		OperationTarget = InOperationTarget;
		if(OperationTarget)
		{
			if(bTrackTarget)
			{
				UCameraModuleBPLibrary::StartTrackTarget(OperationTarget, TrackTargetMode, static_cast<ETrackTargetSpace>(CameraViewSpace), CameraViewOffset, CameraViewYaw, CameraViewPitch, CameraViewDistance, true, CameraViewMode == EProcedureCameraViewMode::Instant);
			}
			if(bResetCameraView)
			{
				ResetCameraView();
			}
		}
		else
		{
			if(bTrackTarget)
			{
				UCameraModuleBPLibrary::EndTrackTarget();
			}
		}
	}
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
			if(AProcedureModule* ProcedureModule = AProcedureModule::Get(true))
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
