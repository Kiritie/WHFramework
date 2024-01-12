// Copyright 2023 Chang Qing,. All Rights Reserved.

#include "Animation/AsyncAction/AsyncAction_ExecuteWithTransition.h"

#include "Animation/AnimationModuleStatics.h"
#include "Common/CommonStatics.h"

UAsyncAction_ExecuteWithTransition::UAsyncAction_ExecuteWithTransition(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UAsyncAction_ExecuteWithTransition* UAsyncAction_ExecuteWithTransition::ExecuteWithTransition(float Duration)
{
	UAsyncAction_ExecuteWithTransition* ThisTask = NewObject<UAsyncAction_ExecuteWithTransition>();
	ThisTask->RegisterWithGameInstance(UCommonStatics::GetGameInstance());

	UAnimationModuleStatics::ExecuteWithTransition(Duration, [ThisTask]{ ThisTask->OnFinish.Broadcast(); } );
	
	return ThisTask;
}