// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilityModuleTypes.h"
#include "Main/Base/ModuleBase.h"
#include "Scene/Container/SceneContainerInterface.h"

#include "AbilityModule.generated.h"

class AAbilityVitalityBase;
class AAbilityCharacterBase;
class AAbilityPickUpBase;
UCLASS()
class WHFRAMEWORK_API AAbilityModule : public AModuleBase
{
	GENERATED_BODY()
	
public:	
	// ParamSets default values for this actor's properties
	AAbilityModule();

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

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TMap<EInteractAction, FString> InteractActionMap;

	//////////////////////////////////////////////////////////////////////////
	// Interact
public:
	UFUNCTION(BlueprintPure)
	FText GetInteractActionDisplayName(int32 InInteractAction);

	//////////////////////////////////////////////////////////////////////////
	// PickUp
public:
	virtual AAbilityPickUpBase* SpawnPickUp(FAbilityItem InItem, FVector InLocation, ISceneContainerInterface* InContainer = nullptr);

	virtual AAbilityPickUpBase* SpawnPickUp(FSaveData* InSaveData, ISceneContainerInterface* InContainer = nullptr);

	//////////////////////////////////////////////////////////////////////////
	// Character
public:
	virtual AAbilityCharacterBase* SpawnCharacter(FSaveData* InSaveData, ISceneContainerInterface* InContainer = nullptr);

	//////////////////////////////////////////////////////////////////////////
	// Vitality
public:
	virtual AAbilityVitalityBase* SpawnVitality(FSaveData* InSaveData, ISceneContainerInterface* InContainer = nullptr);
};
