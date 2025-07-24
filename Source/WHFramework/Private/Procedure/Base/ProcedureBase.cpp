// Fill out your copyright notice in the Description page of Project Settings.


#include "Procedure/Base/ProcedureBase.h"

#include "Camera/CameraModule.h"
#include "Camera/CameraModuleStatics.h"
#include "Debug/DebugModuleTypes.h"
#include "Event/EventModuleStatics.h"
#include "Event/Handle/Procedure/EventHandle_ProcedureEntered.h"
#include "Event/Handle/Procedure/EventHandle_ProcedureLeaved.h"
#include "Event/Handle/Procedure/EventHandle_ProcedureStateChanged.h"
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
	ProcedureGuideIntervalTime = 0.f;

	OperationTarget = nullptr;
	bTrackTarget = false;
	TrackTargetMode = ECameraTrackMode::LocationOnly;
	CameraViewParams = FCameraViewParams();
}

#if WITH_EDITOR
void UProcedureBase::OnGenerate()
{
	
}

void UProcedureBase::OnUnGenerate()
{
	if(bFirstProcedure)
	{
		if(GetProcedureAsset()->FirstProcedure == this)
		{
			GetProcedureAsset()->FirstProcedure = nullptr;
		}
	}
}
#endif

void UProcedureBase::OnStateChanged(EProcedureState InProcedureState)
{
	OnProcedureStateChanged.Broadcast(InProcedureState);
	K2_OnStateChanged(InProcedureState);

	UEventModuleStatics::BroadcastEvent(UEventHandle_ProcedureStateChanged::StaticClass(), this, {this});
}

void UProcedureBase::OnInitialize()
{
	K2_OnInitialize();

	CameraViewParams.CameraViewTarget = OperationTarget.LoadSynchronous();
}

void UProcedureBase::OnEnter(UProcedureBase* InLastProcedure)
{
	ProcedureState = EProcedureState::Entered;
	OnStateChanged(ProcedureState);

	WHDebug(FString::Printf(TEXT("进入流程: %s"), *ProcedureDisplayName.ToString()), EDM_All, EDC_Procedure, EDV_Log, FColor::Cyan, 5.f);

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

	UEventModuleStatics::BroadcastEvent(UEventHandle_ProcedureEntered::StaticClass(), this, {this});
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
		UCameraModuleStatics::EndTrackTarget(OperationTarget.LoadSynchronous());
	}

	WHDebug(FString::Printf(TEXT("离开流程: %s"), *ProcedureDisplayName.ToString()), EDM_All, EDC_Procedure, EDV_Log, FColor::Orange, 5.f);

	K2_OnLeave(InNextProcedure);

	UEventModuleStatics::BroadcastEvent(UEventHandle_ProcedureLeaved::StaticClass(), this, {this});
}

void UProcedureBase::Switch_Implementation()
{
	UProcedureModuleStatics::SwitchProcedure(this);
}

void UProcedureBase::UnSwitch_Implementation()
{
	if(IsCurrent())
	{
		UProcedureModuleStatics::SwitchProcedure(nullptr);
	}
}

void UProcedureBase::Guide_Implementation()
{
	if(IsCurrent())
	{
		OnGuide();
	}
}

void UProcedureBase::SwitchLast_Implementation()
{
	UProcedureModuleStatics::SwitchLastProcedure();
}

void UProcedureBase::SwitchNext_Implementation()
{
	UProcedureModuleStatics::SwitchNextProcedure();
}

bool UProcedureBase::IsCurrent_Implementation()
{
	return UProcedureModuleStatics::IsCurrentProcedure(this);
}

UProcedureAsset* UProcedureBase::GetProcedureAsset() const
{
	return Cast<UProcedureAsset>(GetOuter());
}

#if WITH_EDITOR
void UProcedureBase::GetCameraView()
{
	CameraViewParams.GetCameraParams(OperationTarget.LoadSynchronous());

	Modify();
}

void UProcedureBase::SetCameraView(const FCameraParams& InCameraParams)
{
	CameraViewParams.SetCameraParams(InCameraParams, OperationTarget.LoadSynchronous());

	Modify();
}
#endif

void UProcedureBase::ResetCameraView()
{
	if(IsCurrent() && CameraViewParams.IsValid())
	{
		UCameraModuleStatics::SetCameraView(FCameraViewData(OperationTarget.LoadSynchronous(), bTrackTarget, TrackTargetMode, CameraViewParams));
	}
}

AActor* UProcedureBase::GetOperationTarget(TSubclassOf<AActor> InClass) const
{
	return OperationTarget.LoadSynchronous();
}

void UProcedureBase::SetOperationTarget(AActor* InOperationTarget, bool bResetCameraView)
{
	OperationTarget = InOperationTarget;
	CameraViewParams.CameraViewTarget = InOperationTarget;

	if(bResetCameraView)
	{
		ResetCameraView();
	}
}

#if WITH_EDITOR
bool UProcedureBase::GenerateListItem(TSharedPtr<FProcedureListItem> OutProcedureListItem, const FString& InFilterText)
{
	OutProcedureListItem->Procedure = this;
	if(!InFilterText.IsEmpty())
	{
		return ProcedureDisplayName.ToString().Contains(InFilterText);
	}
	return true;
}

void UProcedureBase::UpdateListItem(TSharedPtr<FProcedureListItem> OutProcedureListItem)
{
	OutProcedureListItem->Procedure = this;
}

bool UProcedureBase::CanEditChange(const FProperty* InProperty) const
{
	if(InProperty)
	{
		const FString PropertyName = InProperty->GetName();
	}

	return Super::CanEditChange(InProperty);
}

void UProcedureBase::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	const FProperty* Property = PropertyChangedEvent.MemberProperty;

	if(Property && PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive)
	{
		const FName PropertyName = Property->GetFName();
		
		if(PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UProcedureBase, bFirstProcedure))
		{
			if(bFirstProcedure)
			{
				if(GetProcedureAsset()->FirstProcedure)
				{
					GetProcedureAsset()->FirstProcedure->bFirstProcedure = false;
				}
				GetProcedureAsset()->FirstProcedure = this;
			}
			else if(GetProcedureAsset()->FirstProcedure == this)
			{
				GetProcedureAsset()->FirstProcedure = nullptr;
			}
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif
