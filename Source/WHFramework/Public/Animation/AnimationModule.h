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
class WHFRAMEWORK_API AAnimationModule : public AModuleBase
{
	GENERATED_BODY()
		
	GENERATED_MODULE(AAnimationModule)

public:
	AAnimationModule();
	
	~AAnimationModule();
	
	//////////////////////////////////////////////////////////////////////////
	/// Module
public:
#if WITH_EDITOR
	virtual void OnGenerate() override;

	virtual void OnDestroy() override;
#endif

	virtual void OnInitialize_Implementation() override;

	virtual void OnPreparatory_Implementation(EPhase InPhase) override;

	virtual void OnRefresh_Implementation(float DeltaSeconds) override;

	virtual void OnPause_Implementation() override;

	virtual void OnUnPause_Implementation() override;

	virtual void OnTermination_Implementation() override;

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
