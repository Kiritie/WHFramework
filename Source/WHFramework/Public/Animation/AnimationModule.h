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
	/// Module
public:
#if WITH_EDITOR
	virtual void OnGenerate() override;

	virtual void OnDestroy() override;
#endif

	virtual void OnInitialize() override;

	virtual void OnPreparatory(EPhase InPhase) override;

	virtual void OnRefresh(float DeltaSeconds) override;

	virtual void OnPause() override;

	virtual void OnUnPause() override;

	virtual void OnTermination(EPhase InPhase) override;

	//////////////////////////////////////////////////////////////////////////
	/// Translation
public:
	void MoveActorTo(AActor* InActor, ATargetPoint* InTargetPoint, FTransform InTargetTransform,bool bUseRotator, bool bUseScale, float ApplicationTime, bool bEaseIn, bool bEaseOut, float BlendExp, bool bForceShortestRotationPath, EMoveActorAction::Type MoveAction, FLatentActionInfo LatentInfo);

	void RotatorActorTo(AActor* InActor, FRotator InRotator, float ApplicationTime, EMoveActorAction::Type MoveAction, FLatentActionInfo LatentInfo);

	void ScaleActorTo(AActor* InActor, FVector InScale, float ApplicationTime, EMoveActorAction::Type MoveAction, FLatentActionInfo LatentInfo);

	//////////////////////////////////////////////////////////////////////////
	/// Flow
public:
	void CancelableDelay(UObject* InWorldContext, float Duration, ECancelableDelayAction::Type CancelableDelayAction, FLatentActionInfo LatentInfo);
};
