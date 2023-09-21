// Fill out your copyright notice in the Description page of Project Settings.


#include "Procedure/Base/ProcedureBase.h"

#include "Camera/CameraModule.h"
#include "Camera/CameraModuleBPLibrary.h"
#include "Event/EventModuleBPLibrary.h"
#include "Event/Handle/Procedure/EventHandle_EnterProcedure.h"
#include "Event/Handle/Procedure/EventHandle_LeaveProcedure.h"
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

	WHDebug(FString::Printf(TEXT("进入流程: %s"), *ProcedureDisplayName.ToString()), EDebugMode::All, EDC_Procedure, EDV_Log, FColor::Cyan, 5.f);

	K2_OnEnter(InLastProcedure);

	UCameraModuleBPLibrary::SwitchCamera(CameraViewPawn);

	ResetCameraView();

	if(bTrackTarget)
	{
		UCameraModuleBPLibrary::StartTrackTarget(OperationTarget, TrackTargetMode, static_cast<ETrackTargetSpace>(CameraViewSpace), CameraViewOffset, CameraViewYaw, CameraViewPitch, CameraViewDistance, true, CameraViewMode == EProcedureCameraViewMode::Instant);
	}

	switch(ProcedureGuideType)
	{
		case EProcedureGuideType::TimerOnce:
		{
			GetWorld()->GetTimerManager().SetTimer(StartGuideTimerHandle, this, &UProcedureBase::OnGuide, ProcedureGuideIntervalTime, false);
			break;
		}
		default: break;
	}

	UEventModuleBPLibrary::BroadcastEvent(UEventHandle_EnterProcedure::StaticClass(), EEventNetType::Single, this, {this});
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

	if(bTrackTarget)
	{
		UCameraModuleBPLibrary::EndTrackTarget(OperationTarget);
	}

	WHDebug(FString::Printf(TEXT("离开流程: %s"), *ProcedureDisplayName.ToString()), EDebugMode::All, EDC_Procedure, EDV_Log, FColor::Orange, 5.f);

	K2_OnLeave(InNextProcedure);

	UEventModuleBPLibrary::BroadcastEvent(UEventHandle_LeaveProcedure::StaticClass(), EEventNetType::Single, this, {this});
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
	if(ProcedureState == EProcedureState::Entered)
	{
		if(InOperationTarget)
		{
			if(bResetCameraView)
			{
				ResetCameraView();
			}
			if(bTrackTarget)
			{
				UCameraModuleBPLibrary::StartTrackTarget(InOperationTarget, TrackTargetMode, static_cast<ETrackTargetSpace>(CameraViewSpace), CameraViewOffset, CameraViewYaw, CameraViewPitch, CameraViewDistance, true, CameraViewMode == EProcedureCameraViewMode::Instant);
			}
		}
		else
		{
			if(bTrackTarget)
			{
				UCameraModuleBPLibrary::EndTrackTarget(OperationTarget);
			}
		}
	}
	OperationTarget = InOperationTarget;
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
