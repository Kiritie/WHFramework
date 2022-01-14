// Fill out your copyright notice in the Description page of Project Settings.


#include "Widget/User/UserWidgetBase.h"

#include "Main/MainModule.h"
#include "Widget/WidgetModule.h"
#include "Widget/WidgetModuleBPLibrary.h"

UUserWidgetBase::UUserWidgetBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	WidgetName = NAME_None;
	WidgetType = EWidgetType::None;
	WidgetState = EWidgetState::None;
	InputMode = EInputMode::None;
	OwnerActor = nullptr;
}

void UUserWidgetBase::OnCreate_Implementation()
{
	
}

void UUserWidgetBase::OnInitialize_Implementation(AActor* InOwner)
{
	OwnerActor = InOwner;
}

void UUserWidgetBase::OnOpen_Implementation(const TArray<FParameter>& InParams, bool bInstant)
{
	WidgetState = EWidgetState::Opening;
	
	switch (WidgetType)
	{
		case EWidgetType::Permanent:
		case EWidgetType::SemiPermanent:
		{
			SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}
		case EWidgetType::Temporary:
		case EWidgetType::SemiTemporary:
		{
			AddToViewport();
		}
		default: break;
	}

	if(bInstant)
	{
		FinishOpen(bInstant);
	}
}

void UUserWidgetBase::OnClose_Implementation(bool bInstant)
{
	WidgetState = EWidgetState::Closing;

	if(bInstant)
	{
		FinishClose(bInstant);
	}
}

void UUserWidgetBase::OnReset_Implementation()
{
}

void UUserWidgetBase::OnRefresh_Implementation()
{
}

void UUserWidgetBase::OnDestroy_Implementation()
{
	if(IsInViewport())
	{
		RemoveFromViewport();
	}
}

void UUserWidgetBase::Open_Implementation(bool bInstant)
{
	UWidgetModuleBPLibrary::OpenUserWidget<UUserWidgetBase>(nullptr, bInstant, GetClass());
}

void UUserWidgetBase::Close_Implementation(bool bInstant)
{
	UWidgetModuleBPLibrary::CloseUserWidget<UUserWidgetBase>(bInstant, GetClass());
}

void UUserWidgetBase::Toggle_Implementation(bool bInstant)
{
	if(WidgetState == EWidgetState::Opening || WidgetState == EWidgetState::Closing) return;
	
	if(WidgetState != EWidgetState::Opened)
	{
		Open(bInstant);
	}
	else
	{
		Close(bInstant);
	}
}

void UUserWidgetBase::Reset_Implementation()
{
	OnReset();
}

void UUserWidgetBase::Refresh_Implementation()
{
	if(WidgetState == EWidgetState::Opened)
	{
		OnRefresh();
	}
}

void UUserWidgetBase::Destroy_Implementation()
{
	UWidgetModuleBPLibrary::DestroyUserWidget<UUserWidgetBase>(GetClass());
}

void UUserWidgetBase::FinishOpen_Implementation(bool bInstant)
{
	WidgetState = EWidgetState::Opened;

	Refresh();
	
	if(AInputModule* InputModule = AMainModule::GetModuleByClass<AInputModule>())
	{
		InputModule->UpdateInputMode();
	}
}

void UUserWidgetBase::FinishClose_Implementation(bool bInstant)
{
	WidgetState = EWidgetState::Closed;

	switch (WidgetType)
	{
		case EWidgetType::Permanent:
		case EWidgetType::SemiPermanent:
		{
			SetVisibility(ESlateVisibility::Hidden);
			break;
		}
		case EWidgetType::Temporary:
		{
			if(!bInstant && GetLastWidget())
			{
				GetLastWidget()->Open();
			}
		}
		case EWidgetType::SemiTemporary:
		{
			RemoveFromViewport();
		}
		default: break;
	}

	if(AInputModule* InputModule = AMainModule::GetModuleByClass<AInputModule>())
	{
		InputModule->UpdateInputMode();
	}
}
