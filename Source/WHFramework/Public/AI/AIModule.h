// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "AIModuleTypes.h"
#include "Main/Base/ModuleBase.h"

#include "AIModule.generated.h"

class AAIBase;
UCLASS()
class WHFRAMEWORK_API AAIModule : public AModuleBase
{
	GENERATED_BODY()
	
	GENERATED_MODULE(AAIModule)

public:
	// ParamSets default values for this actor's properties
	AAIModule();

	~AAIModule();

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
	/// Network
public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
