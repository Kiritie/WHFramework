// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "Engine/TargetPoint.h"
#include "Main/Base/ModuleBase.h"

#include "Engine/LatentActionManager.h"
#include "LatentAction/LatentActionModuleTypes.h"

#include "LatentActionModule.generated.h"


/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API ALatentActionModule : public AModuleBase
{
	GENERATED_BODY()

public:
	ALatentActionModule();
	
	//////////////////////////////////////////////////////////////////////////
	/// Module
public:
#if WITH_EDITOR
	virtual void OnGenerate_Implementation() override;

	virtual void OnDestroy_Implementation() override;
#endif

	virtual void OnInitialize_Implementation() override;

	virtual void OnPreparatory_Implementation() override;

	virtual void OnRefresh_Implementation(float DeltaSeconds) override;

	virtual void OnPause_Implementation() override;

	virtual void OnUnPause_Implementation() override;

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
