// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "WHFramework.h"

#include "AIModuleTypes.h"
#include "Main/Base/ModuleBase.h"

#include "AIModule.generated.h"

class AAIBase;
UCLASS()
class WHFRAMEWORK_API AAIModule : public AModuleBase
{
	GENERATED_BODY()
	
	MODULE_INSTANCE_DECLARE(AAIModule)

public:
	// ParamSets default values for this actor's properties
	AAIModule();

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
	/// Network
public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
