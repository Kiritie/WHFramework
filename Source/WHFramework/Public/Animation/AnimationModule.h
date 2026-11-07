// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Common/LifecycleTypes.h"
#include "Main/MainModuleAccess.h"
#include "Main/Base/ModuleBase.h"
#include "Animation/AnimationModuleTypes.h"
#include "AnimationModule.generated.h"

class ATargetPoint;
class UAnimationRequest;
class APlayerController;
struct FLatentActionInfo;
/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UAnimationModule : public UModuleBase
{
	GENERATED_BODY()
		
	GENERATED_MODULE(UAnimationModule)

public:
	UAnimationModule();
	
	~UAnimationModule();
	
	//////////////////////////////////////////////////////////////////////////
	/// ModuleBase
public:
#if WITH_EDITOR
	virtual void OnGenerate() override;

	virtual void OnDestroy() override;
#endif

	virtual void OnInitialize() override;

	virtual void OnPreparatory(EPhase InPhase) override;

	virtual void OnRefresh(float DeltaSeconds, bool bInEditor) override;

	virtual void OnPause() override;

	virtual void OnUnPause() override;

	virtual void OnTermination(EPhase InPhase) override;

	//////////////////////////////////////////////////////////////////////////
	/// Flow
public:
	void CancelableDelay(float Duration, ECancelableDelayAction::Type CancelableDelayAction, const FLatentActionInfo& LatentInfo);

	FGuid ExecuteWithDelay(UObject* InOwner, float InDuration, TFunction<void()> InFinish, TFunction<void()> InCancel = {});
	FGuid ExecuteWithTransition(APlayerController* InPlayer, UObject* InOwner, float InDuration, TFunction<void()> InFinish, TFunction<void()> InCancel = {});
	bool CancelRequest(const FGuid& InRequestId);

private:
	friend class UAnimationRequest;
	FGuid StartRequest(UObject* InOwner, float InDuration, APlayerController* InTransitionPlayer, TFunction<void()> InFinish, TFunction<void()> InCancel);
	bool QueueRequestCompletion(const FGuid& InRequestId, UAnimationRequest* InRequest);
	void FinishRequest(const FGuid& InRequestId, UAnimationRequest* InRequest);
	UPROPERTY(Transient)
	TMap<FGuid, TObjectPtr<UAnimationRequest>> Requests;
	UPROPERTY(Transient)
	TMap<FGuid, TObjectPtr<UAnimationRequest>> PendingDispatch;
	struct FBlueprintDelay
	{
		TWeakObjectPtr<UObject> Target;
		int32 UUID = 0;
		FGuid RequestId;
	};
	TArray<FBlueprintDelay> BlueprintDelays;
	int32 NextLatentId = 1;
	bool bAcceptingRequests = false;
};
