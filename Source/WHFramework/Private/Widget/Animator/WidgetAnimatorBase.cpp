// Fill out your copyright notice in the Description page of Project Settings.

#include "Widget/Animator/WidgetAnimatorBase.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"

UWidgetAnimatorBase::UWidgetAnimatorBase()
{
	WidgetName = NAME_None;
	SelfTag = FGameplayTag();
	AbortTags = FGameplayTagContainer();
	Duration = 1.f;
	EaseType = EEaseType::Linear;
	TargetWidget = nullptr;
	ParentWidget = nullptr;
}

void UWidgetAnimatorBase::OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams)
{
	Super::OnSpawn_Implementation(InOwner, InParams);

	ParentWidget = Cast<UUserWidget>(InOwner);
	
	if(ParentWidget)
	{
		if(!WidgetName.IsNone())
		{
			TargetWidget = ParentWidget->WidgetTree->FindWidget(WidgetName);
		}
		else
		{
			TargetWidget = ParentWidget;
		}
	}
}

void UWidgetAnimatorBase::OnDespawn_Implementation(bool bRecovery)
{
	Super::OnDespawn_Implementation(bRecovery);
}

void UWidgetAnimatorBase::Play_Implementation(const FOnWidgetAnimatorCompleted& OnCompleted, bool bInstant)
{
	_OnCompleted = OnCompleted;
}

bool UWidgetAnimatorBase::Abort_Implementation()
{
	return false;
}

void UWidgetAnimatorBase::Pause_Implementation()
{
}

void UWidgetAnimatorBase::Reset_Implementation()
{
}

void UWidgetAnimatorBase::Stop_Implementation(bool bComplete)
{
}

void UWidgetAnimatorBase::Complete_Implementation(bool bInstant)
{
	if(_OnCompleted.IsBound())
	{
		_OnCompleted.Execute(bInstant);
	}
}
