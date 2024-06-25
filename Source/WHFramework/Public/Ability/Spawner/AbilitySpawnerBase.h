// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Ability/AbilityModuleTypes.h"
#include "Engine/NavigationObjectBase.h"

#include "AbilitySpawnerBase.generated.h"
class UArrowComponent;
class UCapsuleComponent;
/**
 * 
 */
UCLASS(Abstract, Blueprintable, hidecategories = (Tick, Replication, Collision, Input, Cooking, Hidden, Hlod, Physics, LevelInstance))
class WHFRAMEWORK_API AAbilitySpawnerBase : public ANavigationObjectBase
{
	GENERATED_BODY()
	
public:
	AAbilitySpawnerBase();

protected:
	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	//////////////////////////////////////////////////////////////////////////
	/// Spawner
public:
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void Spawn();

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void Destroy();

protected:
	UFUNCTION(BlueprintNativeEvent)
	AActor* SpawnImpl(const FAbilityItem& InAbilityItem);

	UFUNCTION(BlueprintNativeEvent)
	void DestroyImpl(AActor* InAbilityActor);

protected:
#if WITH_EDITORONLY_DATA
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Components")
	UArrowComponent* ArrowComponent;
#endif

protected:
	UPROPERTY(EditAnywhere, Category = "Spawner")
	bool bAutoSpawn;
	
	UPROPERTY(EditAnywhere, Category = "Spawner")
	bool bAutoDestroy;
			
	UPROPERTY(EditAnywhere, Category = "Spawner")
	FAbilityItem AbilityItem;

	UPROPERTY(VisibleAnywhere, Replicated, Category = "Spawner")
	AActor* AbilityActor;

public:
#if WITH_EDITORONLY_DATA
	UArrowComponent* GetArrowComponent() const { return ArrowComponent; }
#endif

	UFUNCTION(BlueprintPure)
	virtual FAbilityItem GetAbilityItem() const { return AbilityItem; }

	UFUNCTION(BlueprintPure)
	virtual AActor* GetAbilityActor() const { return AbilityActor; }

	//////////////////////////////////////////////////////////////////////////
	/// Network
public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
