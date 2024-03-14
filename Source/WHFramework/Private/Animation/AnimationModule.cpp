// Fill out your copyright notice in the Description page of Project Settings.

#include "Animation/AnimationModule.h"

#include "DelayAction.h"
#include "Animation/Widget/WidgetTransitionMaskBase.h"
#include "Common/CommonStatics.h"
#include "Engine/LatentActionManager.h"
#include "Main/MainModule.h"
#include "Widget/WidgetModuleStatics.h"

IMPLEMENTATION_MODULE(UAnimationModule)

UAnimationModule::UAnimationModule()
{
	ModuleName = FName("AnimationModule");
	ModuleDisplayName = FText::FromString(TEXT("Animation Module"));

	bModuleRequired = true;
}

UAnimationModule::~UAnimationModule()
{
	TERMINATION_MODULE(UAnimationModule)
}

#if WITH_EDITOR
void UAnimationModule::OnGenerate()
{
	Super::OnGenerate();
}

void UAnimationModule::OnDestroy()
{
	Super::OnDestroy();

	TERMINATION_MODULE(UAnimationModule)
}
#endif

void UAnimationModule::OnInitialize()
{
	Super::OnInitialize();
}

void UAnimationModule::OnPreparatory(EPhase InPhase)
{
	Super::OnPreparatory(InPhase);
}

void UAnimationModule::OnRefresh(float DeltaSeconds)
{
	Super::OnRefresh(DeltaSeconds);
}

void UAnimationModule::OnPause()
{
	Super::OnPause();
}

void UAnimationModule::OnUnPause()
{
	Super::OnUnPause();
}

void UAnimationModule::OnTermination(EPhase InPhase)
{
	Super::OnTermination(InPhase);
}

void UAnimationModule::CancelableDelay(float Duration, ECancelableDelayAction::Type CancelableDelayAction, const FLatentActionInfo& LatentInfo)
{
	FLatentActionManager& LatentActionManager = GetWorld()->GetLatentActionManager();
	if (LatentActionManager.FindExistingAction<FCancelableDelayAction>(LatentInfo.CallbackTarget, LatentInfo.UUID) == NULL)
	{
		if(CancelableDelayAction == ECancelableDelayAction::None)
		{
			LatentActionManager.AddNewAction(LatentInfo.CallbackTarget, LatentInfo.UUID, new FCancelableDelayAction(Duration, LatentInfo));
		}
	}
	else
	{
		if(CancelableDelayAction == ECancelableDelayAction::Cancel)
		{
			LatentActionManager.FindExistingAction<FCancelableDelayAction>(LatentInfo.CallbackTarget, LatentInfo.UUID)->bExecute = false;
		}
	}
}

void UAnimationModule::ExecuteWithDelay(float Duration, TFunction<void()> OnFinish)
{
	_OnExecuteWithDelayFinish = OnFinish;
	FLatentActionManager& LatentActionManager = GetWorld()->GetLatentActionManager();
	FLatentActionInfo LatentInfo;
	LatentInfo.UUID = 0;
	LatentInfo.Linkage = 0;
	LatentInfo.CallbackTarget = this;
	LatentInfo.ExecutionFunction = GET_FUNCTION_NAME_CHECKED(UAnimationModule, OnExecuteWithDelayFinish);
	LatentActionManager.AddNewAction(LatentInfo.CallbackTarget, LatentInfo.UUID, new FDelayAction(Duration, LatentInfo));
}

void UAnimationModule::ExecuteWithTransition(float Duration, TFunction<void()> OnFinish)
{
	_OnExecuteWithTransitionFinish = OnFinish;
	FLatentActionManager& LatentActionManager = GetWorld()->GetLatentActionManager();
	FLatentActionInfo LatentInfo;
	LatentInfo.UUID = 0;
	LatentInfo.Linkage = 0;
	LatentInfo.CallbackTarget = this;
	LatentInfo.ExecutionFunction = GET_FUNCTION_NAME_CHECKED(UAnimationModule, OnExecuteWithTransitionFinish);
	LatentActionManager.AddNewAction(LatentInfo.CallbackTarget, LatentInfo.UUID, new FTransitionAction(Duration / 2.f, LatentInfo));

	UWidgetModuleStatics::OpenUserWidget<UWidgetTransitionMaskBase>({ Duration });
}

void UAnimationModule::OnExecuteWithDelayFinish()
{
	if(_OnExecuteWithDelayFinish)
	{
		_OnExecuteWithDelayFinish();
	}
}

void UAnimationModule::OnExecuteWithTransitionFinish()
{
	if(_OnExecuteWithTransitionFinish)
	{
		_OnExecuteWithTransitionFinish();
	}
}



