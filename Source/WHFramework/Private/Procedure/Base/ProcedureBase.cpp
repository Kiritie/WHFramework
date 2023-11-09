// Fill out your copyright notice in the Description page of Project Settings.


#include "Procedure/Base/ProcedureBase.h"

#include "Camera/CameraModule.h"
#include "Camera/CameraModuleStatics.h"
#include "Debug/DebugModuleTypes.h"
#include "Event/EventModuleStatics.h"
#include "Event/Handle/Procedure/EventHandle_EnterProcedure.h"
#include "Event/Handle/Procedure/EventHandle_LeaveProcedure.h"
#include "Procedure/ProcedureModule.h"
#include "Procedure/ProcedureModuleStatics.h"

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
		if(UProcedureModule* ProcedureModule = UProcedureModule::Get(true))
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

	WHDebug(FString::Printf(TEXT("进入流程: %s"), *ProcedureDisplayName.ToString()), EM_All, EDC_Procedure, EDV_Log, FColor::Cyan, 5.f);

	K2_OnEnter(InLastProcedure);

	if(CameraViewPawn)
	{
		UCameraModuleStatics::SwitchCamera(CameraViewPawn);
	}

	ResetCameraView();

	if(bTrackTarget)
	{
		UCameraModuleStatics::StartTrackTarget(OperationTarget, TrackTargetMode, static_cast<ETrackTargetSpace>(CameraViewSpace), CameraViewOffset, FVector(-1.f), CameraViewYaw, CameraViewPitch, CameraViewDistance, true, CameraViewMode == EProcedureCameraViewMode::Instant);
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

	UEventModuleStatics::BroadcastEvent(UEventHandle_EnterProcedure::StaticClass(), EEventNetType::Single, this, {this});
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
		UCameraModuleStatics::EndTrackTarget(OperationTarget);
	}

	WHDebug(FString::Printf(TEXT("离开流程: %s"), *ProcedureDisplayName.ToString()), EM_All, EDC_Procedure, EDV_Log, FColor::Orange, 5.f);

	K2_OnLeave(InNextProcedure);

	UEventModuleStatics::BroadcastEvent(UEventHandle_LeaveProcedure::StaticClass(), EEventNetType::Single, this, {this});
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
	UProcedureModuleStatics::SwitchLastProcedure();
}

void UProcedureBase::SwitchNext()
{
	UProcedureModuleStatics::SwitchNextProcedure();
}

bool UProcedureBase::IsCurrent()
{
	return UProcedureModuleStatics::IsCurrentProcedure(this);
}

#if WITH_EDITOR
void UProcedureBase::GetCameraView()
{
	if(CameraViewSpace == EProcedureCameraViewSpace::Local && OperationTarget)
	{
		CameraViewOffset = UCameraModuleStatics::GetCameraLocation() - OperationTarget->GetActorLocation();
		CameraViewYaw = UCameraModuleStatics::GetCameraRotation().Yaw - OperationTarget->GetActorRotation().Yaw;
		CameraViewPitch = UCameraModuleStatics::GetCameraRotation().Pitch - OperationTarget->GetActorRotation().Pitch;
	}
	else
	{
		CameraViewOffset = UCameraModuleStatics::GetCameraLocation();
		CameraViewYaw = UCameraModuleStatics::GetCameraRotation().Yaw;
		CameraViewPitch = UCameraModuleStatics::GetCameraRotation().Pitch;
	}
	CameraViewDistance = UCameraModuleStatics::GetCameraDistance();

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
			UCameraModuleStatics::SetCameraLocation(CameraLocation, true);
			UCameraModuleStatics::SetCameraRotation(CameraViewYaw, CameraViewPitch, true);
			UCameraModuleStatics::SetCameraDistance(CameraViewDistance, true);
			break;
		}
		case EProcedureCameraViewMode::Smooth:
		{
			UCameraModuleStatics::SetCameraLocation(CameraLocation, false);
			UCameraModuleStatics::SetCameraRotation(CameraViewYaw, CameraViewPitch, false);
			UCameraModuleStatics::SetCameraDistance(CameraViewDistance, false);
			break;
		}
		case EProcedureCameraViewMode::Duration:
		{
			UCameraModuleStatics::DoCameraLocation(CameraLocation, CameraViewDuration, CameraViewEaseType);
			UCameraModuleStatics::DoCameraRotation(CameraYaw, CameraPitch, CameraViewDuration, CameraViewEaseType);
			UCameraModuleStatics::DoCameraDistance(CameraDistance, CameraViewDuration, CameraViewEaseType);
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
				UCameraModuleStatics::StartTrackTarget(InOperationTarget, TrackTargetMode, static_cast<ETrackTargetSpace>(CameraViewSpace), CameraViewOffset, FVector(-1.f), CameraViewYaw, CameraViewPitch, CameraViewDistance, true, CameraViewMode == EProcedureCameraViewMode::Instant);
			}
		}
		else
		{
			if(bTrackTarget)
			{
				UCameraModuleStatics::EndTrackTarget(OperationTarget);
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
			if(UProcedureModule* ProcedureModule = UProcedureModule::Get(true))
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
