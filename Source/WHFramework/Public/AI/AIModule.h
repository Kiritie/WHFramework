// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "Main/Base/ModuleBase.h"

#include "AIModule.generated.h"

class AAIBase;
UCLASS()
class WHFRAMEWORK_API UAIModule : public UModuleBase
{
	GENERATED_BODY()
	
	GENERATED_MODULE(UAIModule)

public:
	// ParamSets default values for this actor's properties
	UAIModule();

	~UAIModule();

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
	/// Network
public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
