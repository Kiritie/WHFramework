// Fill out your copyright notice in the Description page of Project Settings.


#include "Procedure/Base/ProcedureBase.h"

#include "Camera/CameraModule.h"
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

	UEventModuleBPLibrary::BroadcastEvent(UEventHandle_EnterProcedure::StaticClass(), EEventNetType::Single, this, TArray<FParameter>{FParameter::MakeObject(this)});
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

	UEventModuleBPLibrary::BroadcastEvent(UEventHandle_LeaveProcedure::StaticClass(), EEventNetType::Single, this, TArray<FParameter>{FParameter::MakeObject(this)});
}

void UProcedureBase::Guide()
{
	if(ProcedureState == EProcedureState::Entered)
	{
		OnGuide();
	}
}

void UProcedureBase::SwitchLastProcedure()
{
	UProcedureModuleBPLibrary::SwitchLastProcedure();
}

void UProcedureBase::SwitchNextProcedure()
{
	UProcedureModuleBPLibrary::SwitchNextProcedure();
}

#if WITH_EDITOR
void UProcedureBase::GetCameraView()
{
	if(ACameraModule* CameraModule = AMainModule::GetModuleByClass<ACameraModule>())
	{
		if(CameraViewSpace == EProcedureCameraViewSpace::Local && OperationTarget)
		{
			CameraViewOffset = CameraModule->GetCurrentCameraLocation() - OperationTarget->GetActorLocation();
			CameraViewYaw = CameraModule->GetCurrentCameraRotation().Yaw - OperationTarget->GetActorRotation().Yaw;
			CameraViewPitch = CameraModule->GetCurrentCameraRotation().Pitch - OperationTarget->GetActorRotation().Pitch;
		}
		else
		{
			CameraViewOffset = CameraModule->GetCurrentCameraLocation();
			CameraViewYaw = CameraModule->GetCurrentCameraRotation().Yaw;
			CameraViewPitch = CameraModule->GetCurrentCameraRotation().Pitch;
		}
		CameraViewDistance = CameraModule->GetCurrentCameraDistance();

		Modify();
	}
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
	if(ACameraModule* CameraModule = AMainModule::GetModuleByClass<ACameraModule>())
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
				CameraModule->SetCameraLocation(CameraLocation, true);
				CameraModule->SetCameraRotation(CameraViewYaw, CameraViewPitch, true);
				CameraModule->SetCameraDistance(CameraViewDistance, true);
				break;
			}
			case EProcedureCameraViewMode::Smooth:
			{
				CameraModule->SetCameraLocation(CameraLocation, false);
				CameraModule->SetCameraRotation(CameraViewYaw, CameraViewPitch, false);
				CameraModule->SetCameraDistance(CameraViewDistance, false);
				break;
			}
			case EProcedureCameraViewMode::Duration:
			{
				CameraModule->DoCameraLocation(CameraLocation, CameraViewDuration, CameraViewEaseType);
				CameraModule->DoCameraRotation(CameraYaw, CameraPitch, CameraViewDuration, CameraViewEaseType);
				CameraModule->DoCameraDistance(CameraDistance, CameraViewDuration, CameraViewEaseType);
				break;
			}
			default: break;
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
