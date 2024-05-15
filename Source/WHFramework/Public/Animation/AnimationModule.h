// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Main/Base/ModuleBase.h"
#include "Animation/AnimationModuleTypes.h"
#include "AnimationModule.generated.h"

class ATargetPoint;
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

	void ExecuteWithDelay(float Duration, TFunction<void()> OnFinish);

	void ExecuteWithTransition(float Duration, TFunction<void()> OnFinish);

protected:
	UFUNCTION()
	void OnExecuteWithDelayFinish();

	UFUNCTION()
	void OnExecuteWithTransitionFinish();

protected:
	TFunction<void()> _OnExecuteWithDelayFinish;
	TFunction<void()> _OnExecuteWithTransitionFinish;
};
