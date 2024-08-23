// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Ability/AbilityModuleTypes.h"
#include "Common/Base/WHActor.h"
#include "Engine/NavigationObjectBase.h"

#include "AbilitySpawnerBase.generated.h"
class UTextRenderComponent;
class UArrowComponent;
class UCapsuleComponent;
/**
 * 
 */
UCLASS(Abstract, Blueprintable, hidecategories = (Tick, Replication, Collision, Input, Cooking, Hidden, Hlod, Physics, LevelInstance))
class WHFRAMEWORK_API AAbilitySpawnerBase : public ANavigationObjectBase, public IWHActorInterface, public ISceneActorInterface, public IObjectPoolInterface
{
	GENERATED_BODY()
	
public:
	AAbilitySpawnerBase();

	//////////////////////////////////////////////////////////////////////////
	/// WHActor
public:
	virtual void OnInitialize_Implementation() override;

	virtual void OnPreparatory_Implementation(EPhase InPhase) override;

	virtual void OnRefresh_Implementation(float DeltaSeconds) override;

	virtual void OnTermination_Implementation(EPhase InPhase) override;

protected:
	virtual bool IsDefaultLifecycle_Implementation() const override { return true; }

	//////////////////////////////////////////////////////////////////////////
	/// ObjectPool
public:
	virtual int32 GetLimit_Implementation() const override { return -1; }

	virtual void OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams) override;
		
	virtual void OnDespawn_Implementation(bool bRecovery) override;

	//////////////////////////////////////////////////////////////////////////
	/// SceneActor
protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SceneActor")
	FGuid ActorID;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SceneActor")
	bool bVisible;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SceneActor")
	TScriptInterface<ISceneContainerInterface> Container;
	
public:
	virtual FGuid GetActorID_Implementation() const override { return ActorID; }

	virtual void SetActorID_Implementation(const FString& InID) override { ActorID = FGuid(InID); }

	virtual TScriptInterface<ISceneContainerInterface> GetContainer_Implementation() const override { return Container; }

	virtual void SetContainer_Implementation(const TScriptInterface<ISceneContainerInterface>& InContainer) override { Container = InContainer; }

	virtual bool IsVisible_Implementation() const override { return bVisible; }

	virtual void SetActorVisible_Implementation(bool bInVisible) override;

protected:
	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	virtual void Tick(float DeltaSeconds) override;

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

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Components")
	UTextRenderComponent* TextComponent;
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
	
	UTextRenderComponent* GetTextComponent() const { return TextComponent; }
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
