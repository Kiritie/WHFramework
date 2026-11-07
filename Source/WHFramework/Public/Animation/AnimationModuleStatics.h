// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "AnimationModule.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "LatentActions.h"
#include "AnimationModuleStatics.generated.h"

class APlayerController;

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UAnimationModuleStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, meta=(WorldContext="WorldContextObject", Latent, LatentInfo="LatentInfo", Duration="0.2", ExpandEnumAsExecs="CancelableDelayAction", Keywords="sleep", UnsafeDuringActorConstruction="true"), Category="AnimationModuleStatics")
	static void CancelableDelay(const UObject* WorldContextObject, float Duration, TEnumAsByte<ECancelableDelayAction::Type> CancelableDelayAction, FLatentActionInfo LatentInfo);

	static FGuid ExecuteWithDelay(const UObject* WorldContextObject, UObject* InOwner, float Duration, TFunction<void()> OnFinish, TFunction<void()> OnCancel = {});

	static FGuid ExecuteWithTransition(const UObject* WorldContextObject, APlayerController* InPlayer, UObject* InOwner, float Duration, TFunction<void()> OnFinish, TFunction<void()> OnCancel = {});
};
