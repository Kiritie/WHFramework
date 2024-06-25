// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "AbilityModuleTypes.h"
#include "Main/Base/ModuleBase.h"
#include "Scene/Container/SceneContainerInterface.h"

#include "AbilityModule.generated.h"

class AAbilityItemBase;
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
	// Item
public:
	virtual AAbilityItemBase* SpawnAbilityItem(FAbilityItem InItem, FVector InLocation = FVector::ZeroVector, FRotator InRotation = FRotator::ZeroRotator, ISceneContainerInterface* InContainer = nullptr);

	virtual AAbilityItemBase* SpawnAbilityItem(FAbilityItem InItem, AActor* InOwnerActor);

	//////////////////////////////////////////////////////////////////////////
	// PickUp
public:
	virtual AAbilityPickUpBase* SpawnAbilityPickUp(FAbilityItem InItem, FVector InLocation = FVector::ZeroVector, ISceneContainerInterface* InContainer = nullptr);

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
