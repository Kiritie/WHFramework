// Copyright 2023 Chang Qing,. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "AsyncAction_ExecuteWithTransition.generated.h"

class UAnimationModule;
class APlayerController;

UCLASS()
class WHFRAMEWORK_API UAsyncAction_ExecuteWithTransition : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	UAsyncAction_ExecuteWithTransition(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, meta=(WorldContext="WorldContextObject", BlueprintInternalUseOnly="true"))
	static UAsyncAction_ExecuteWithTransition* ExecuteWithTransition(const UObject* WorldContextObject, APlayerController* InPlayer, float Duration);
	virtual void Activate() override;
	virtual void BeginDestroy() override;
	UFUNCTION(BlueprintCallable, Category="Animation")
	void Cancel();

	UPROPERTY(BlueprintAssignable)
	FSimpleDynamicMulticastDelegate OnFinish;
	UPROPERTY(BlueprintAssignable)
	FSimpleDynamicMulticastDelegate OnCanceled;

private:
	TWeakObjectPtr<UWorld> RequestWorld;
	TWeakObjectPtr<UAnimationModule> RequestModule;
	TWeakObjectPtr<APlayerController> RequestPlayer;
	FGuid RequestId;
	float RequestDuration = 0.f;
	bool bActivated = false;
	bool bCanceled = false;
};
