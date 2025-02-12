// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "AbilityModuleTypes.h"
#include "Main/Base/ModuleBase.h"
#include "Scene/Container/SceneContainerInterface.h"

#include "AbilityModule.generated.h"

class IAbilityActorInterface;
class AAbilityItemBase;
class AAbilityVitalityBase;
class AAbilityCharacterBase;
class AAbilityPickUpBase;
class AAbilityActorBase;
class AAbilityPawnBase;
class AAbilityProjectileBase;

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
	// Attribute
protected:
	UPROPERTY(EditAnywhere)
	TMap<FGameplayAttribute, FLinearColor> AttributeColorMap;

public:
	UFUNCTION(BlueprintPure)
	FLinearColor GetAttributeColor(const FGameplayAttribute& InAttribute) const;

	UFUNCTION(BlueprintCallable)
	void SetAttributeColor(const FGameplayAttribute& InAttribute, const FLinearColor& InColor);

	//////////////////////////////////////////////////////////////////////////
	// Item
public:
	virtual AAbilityItemBase* SpawnAbilityItem(FAbilityItem InItem, FVector InLocation, FRotator InRotation, ISceneContainerInterface* InContainer = nullptr);

	virtual AAbilityItemBase* SpawnAbilityItem(FAbilityItem InItem, AActor* InOwnerActor = nullptr);

	//////////////////////////////////////////////////////////////////////////
	// PickUp
public:
	virtual AAbilityPickUpBase* SpawnAbilityPickUp(FAbilityItem InItem, FVector InLocation, ISceneContainerInterface* InContainer = nullptr);

	virtual AAbilityPickUpBase* SpawnAbilityPickUp(FSaveData* InSaveData, ISceneContainerInterface* InContainer = nullptr);

	//////////////////////////////////////////////////////////////////////////
	// Projectile
public:
	virtual AAbilityProjectileBase* SpawnAbilityProjectile(const TSubclassOf<AAbilityProjectileBase>& InClass, AActor* InOwnerActor = nullptr, const FGameplayAbilitySpecHandle& InAbilityHandle = FGameplayAbilitySpecHandle());

	//////////////////////////////////////////////////////////////////////////
	// Actor
public:
	virtual AActor* SpawnAbilityActor(FSaveData* InSaveData, ISceneContainerInterface* InContainer = nullptr);
};
