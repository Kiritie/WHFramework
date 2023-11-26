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
	TrackTargetMode = ECameraTrackMode::LocationOnly;
	CameraViewParams = FCameraViewParams();

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
		if(UProcedureModule::Get(true).GetFirstProcedure() == this)
		{
			UProcedureModule::Get(true).SetFirstProcedure(nullptr);
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

	WHDebug(FString::Printf(TEXT("进入流程: %s"), *ProcedureDisplayName.ToString()), EDM_All, EDC_Procedure, EDV_Log, FColor::Cyan, 5.f);

	K2_OnEnter(InLastProcedure);

	ResetCameraView();

	if(bTrackTarget)
	{
		UCameraModuleStatics::StartTrackTarget(OperationTarget, TrackTargetMode, CameraViewParams.CameraViewSpace,
			CameraViewParams.CameraViewOffset, FVector(-1.f), CameraViewParams.CameraViewYaw,
			CameraViewParams.CameraViewPitch, CameraViewParams.CameraViewDistance, true, CameraViewParams.CameraViewMode == ECameraViewMode::Instant);
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

	UEventModuleStatics::BroadcastEvent(UEventHandle_EnterProcedure::StaticClass(), this, {this});
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

	WHDebug(FString::Printf(TEXT("离开流程: %s"), *ProcedureDisplayName.ToString()), EDM_All, EDC_Procedure, EDV_Log, FColor::Orange, 5.f);

	K2_OnLeave(InNextProcedure);

	UEventModuleStatics::BroadcastEvent(UEventHandle_LeaveProcedure::StaticClass(), this, {this});
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
	CameraViewParams.GetCameraParams();

	Modify();
}

void UProcedureBase::SetCameraView(const FCameraParams& InCameraParams)
{
	CameraViewParams.SetCameraParams(InCameraParams);

	Modify();
}
#endif

void UProcedureBase::ResetCameraView()
{
	UCameraModuleStatics::SetCameraViewParams(CameraViewParams);
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
				UCameraModuleStatics::StartTrackTarget(OperationTarget, TrackTargetMode, CameraViewParams.CameraViewSpace,
					CameraViewParams.CameraViewOffset, FVector(-1.f), CameraViewParams.CameraViewYaw,
					CameraViewParams.CameraViewPitch, CameraViewParams.CameraViewDistance, true, CameraViewParams.CameraViewMode == ECameraViewMode::Instant);
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
		
		if(PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UProcedureBase, OperationTarget))
		{
			CameraViewParams.CameraViewTarget = OperationTarget;
		}

		if(PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UProcedureBase, bFirstProcedure))
		{
			if(bFirstProcedure)
			{
				if(UProcedureModule::Get(true).GetFirstProcedure())
				{
					UProcedureModule::Get(true).GetFirstProcedure()->bFirstProcedure = false;
				}
				UProcedureModule::Get(true).SetFirstProcedure(this);
			}
			else if(UProcedureModule::Get(true).GetFirstProcedure() == this)
			{
				UProcedureModule::Get(true).SetFirstProcedure(nullptr);
			}
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif
