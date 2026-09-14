// Fill out your copyright notice in the Description page of Project Settings.


#include "Procedure/Base/ProcedureBase.h"

#include "Camera/CameraModule.h"
#include "Camera/Anchor/CameraShotAnchor.h"
#include "Camera/Manager/CameraManagerBase.h"
#include "Debug/DebugModuleTypes.h"
#include "Event/EventModuleStatics.h"
#include "Event/Events/Procedure/Event_ProcedureEntered.h"
#include "Event/Events/Procedure/Event_ProcedureLeaved.h"
#include "Event/Events/Procedure/Event_ProcedureStateChanged.h"
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
	bApplyCameraAction = false;
	bRestoreDefaultCameraOnLeave = false;
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

	UEventModuleStatics::BroadcastEvent<FEventProcedureStateChanged>(this, {this});
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

	ApplyCameraAction();

	switch(ProcedureGuideType)
	{
		case EProcedureGuideType::TimerOnce:
		{
			GetWorld()->GetTimerManager().SetTimer(StartGuideTimerHandle, this, &UProcedureBase::OnGuide, ProcedureGuideIntervalTime, false);
			break;
		}
		default: break;
	}

	UEventModuleStatics::BroadcastEvent<FEventProcedureEntered>(this, {this});
}

void UProcedureBase::OnRefresh()
{
	K2_OnRefresh();
}

void UProcedureBase::OnGuide()
{
	ApplyCameraAction();

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
	if(CameraOverrideHandle.IsValid())
	{
		if(ACameraManagerBase* CameraManager = UCameraModule::Get().GetCameraManager())
		{
			CameraManager->PopCameraConfigOverride(CameraOverrideHandle);
		}
		CameraOverrideHandle.Reset();
	}
	ProcedureState = EProcedureState::Leaved;
	OnStateChanged(ProcedureState);

	if(bRestoreDefaultCameraOnLeave)
	{
		if(ACameraManagerBase* CameraManager = UCameraModule::Get().GetCameraManager())
		{
			CameraManager->SetDefaultMode(CameraTransition);
		}
	}

	WHDebug(FString::Printf(TEXT("离开流程: %s"), *ProcedureDisplayName.ToString()), EDM_All, EDC_Procedure, EDV_Log, FColor::Orange, 5.f);

	K2_OnLeave(InNextProcedure);

	UEventModuleStatics::BroadcastEvent<FEventProcedureLeaved>(this, {this});
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

void UProcedureBase::ApplyCameraAction()
{
	if(!IsCurrent() || !bApplyCameraAction)
	{
		return;
	}

	ACameraManagerBase* CameraManager = UCameraModule::Get().GetCameraManager();
	if(!CameraManager)
	{
		return;
	}
	if(CameraOverrideHandle.IsValid())
	{
		CameraManager->PopCameraConfigOverride(CameraOverrideHandle);
		CameraOverrideHandle.Reset();
	}
	CameraOverrideHandle = CameraManager->PushCameraConfigOverride(CameraOverride, 10);

	if(CameraModeClass)
	{
		FCameraModeContext Context;
		Context.Target = OperationTarget.LoadSynchronous();
		Context.Anchor = CameraAnchor.LoadSynchronous();
		Context.Transition = CameraTransition;
		CameraManager->SetMode(CameraModeClass, Context);
	}
}

AActor* UProcedureBase::GetOperationTarget(TSubclassOf<AActor> InClass) const
{
	return OperationTarget.LoadSynchronous();
}

void UProcedureBase::SetOperationTarget(AActor* InOperationTarget, bool bResetCameraView)
{
	OperationTarget = InOperationTarget;
	if(bResetCameraView)
	{
		ApplyCameraAction();
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
