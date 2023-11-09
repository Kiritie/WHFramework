// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "AbilityModuleTypes.h"
#include "Main/Base/ModuleBase.h"
#include "Scene/Container/SceneContainerInterface.h"

#include "AbilityModule.generated.h"

class AAbilityVitalityBase;
class AAbilityCharacterBase;
class AAbilityPickUpBase;
class AAbilityActorBase;
class AAbilityPawnBase;

UCLASS()
class WHFRAMEWORK_API UAbilityModule : public UModuleBase
{
	GENERATED_BODY()
	
	GENERATED_MODULE(UAbilityModule)

public:	
	// ParamSets default values for this actor's properties
	UAbilityModule();
	
	~UAbilityModule();
	
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
	// PickUp
public:
	virtual ECollisionChannel GetPickUpTraceChannel() const;

	virtual AAbilityPickUpBase* SpawnAbilityPickUp(FAbilityItem InItem, FVector InLocation, ISceneContainerInterface* InContainer = nullptr);

	virtual AAbilityPickUpBase* SpawnAbilityPickUp(FSaveData* InSaveData, ISceneContainerInterface* InContainer = nullptr);

	//////////////////////////////////////////////////////////////////////////
	// Actor
public:
	virtual AAbilityActorBase* SpawnAbilityActor(FSaveData* InSaveData, ISceneContainerInterface* InContainer = nullptr);

	//////////////////////////////////////////////////////////////////////////
	// Vitality
public:
	virtual AAbilityVitalityBase* SpawnAbilityVitality(FSaveData* InSaveData, ISceneContainerInterface* InContainer = nullptr);

	//////////////////////////////////////////////////////////////////////////
	// Pawn
public:
	virtual AAbilityPawnBase* SpawnAbilityPawn(FSaveData* InSaveData, ISceneContainerInterface* InContainer = nullptr);

	//////////////////////////////////////////////////////////////////////////
	// Character
public:
	virtual AAbilityCharacterBase* SpawnAbilityCharacter(FSaveData* InSaveData, ISceneContainerInterface* InContainer = nullptr);
};
