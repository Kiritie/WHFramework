// Copyright 2023 Chang Qing,. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Common/CommonTypes.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "AsyncAction_ExecuteWithTransition.generated.h"

UCLASS()
class WHFRAMEWORK_API UAsyncAction_ExecuteWithTransition : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	UAsyncAction_ExecuteWithTransition(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintCallable)
	static UAsyncAction_ExecuteWithTransition* ExecuteWithTransition(float Duration);

	UPROPERTY(BlueprintAssignable)
	FSimpleDynamicMulticastDelegate OnFinish;
};