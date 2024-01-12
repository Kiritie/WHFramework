// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "AnimationModule.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AnimationModuleStatics.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UAnimationModuleStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, meta=(Latent, LatentInfo= "LatentInfo", Duration="0.2", ExpandEnumAsExecs= "CancelableDelayAction", Keywords="sleep", UnsafeDuringActorConstruction = "true"), Category="AnimationModuleStatics")
	static void	CancelableDelay(float Duration, TEnumAsByte<ECancelableDelayAction::Type> CancelableDelayAction, FLatentActionInfo LatentInfo);

	static void ExecuteWithDelay(float Duration, TFunction<void()> OnFinish);

	static void ExecuteWithTransition(float Duration, TFunction<void()> OnFinish);
};