// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Main/MainModuleAccess.h"
#include "Base/ModuleBase.h"
#include "Base/ModuleNetworkComponentBase.h"
#include "Common/Base/WHActor.h"

#include "MainModule.generated.h"

class UInventoryModule;
class AWorldNetworkRouteAuthority;

UCLASS(hidecategories = (Tick, Replication, Collision, Actor, Input, LOD, Cooking, Hidden, Hlod, Networking, Physics, LevelInstance))
class WHFRAMEWORK_API AMainModule : public AWHActor, public FTickableGameObject
{
#if WITH_EDITOR
	friend class FMainModuleCustomization;
#endif
	
	GENERATED_BODY()
	
	GENERATED_MAIN_MODULE(AMainModule)

public:
	// ParamSets default values for this actor's properties
	AMainModule();

	~AMainModule();
	
	//////////////////////////////////////////////////////////////////////////
	/// ModuleBase
public:
#if WITH_EDITOR
	virtual void OnGenerate();

	virtual void OnDestroy();
#endif
	
	virtual void OnInitialize_Implementation() override;

	virtual void OnPreparatory_Implementation() override;

	virtual void OnRefresh_Implementation(float DeltaSeconds) override;

	virtual void OnTermination_Implementation() override;

protected:
	virtual bool IsUseDefaultLifecycle_Implementation() const override { return false; }

public:
	virtual void PostInitializeComponents() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void Tick(float DeltaSeconds) override;

	virtual UWorld* GetTickableGameObjectWorld() const override { return GetWorld(); }

	virtual ETickableTickType GetTickableTickType() const override;

	virtual TStatId GetStatId() const override;
	
	virtual bool IsTickableInEditor() const override { return true; }

	virtual bool IsTickableWhenPaused() const override { return true; }

	virtual bool IsTickable() const override { return true; }
	bool CanResolveModules() const
	{
		return !bModulesTerminated;
	}
	bool IsTerminationInProgress() const { return bTerminationInProgress; }

public:
#if WITH_EDITOR
	virtual bool IsUserManaged() const override { return false; }

	virtual bool IsLockLocation() const override { return true; }

	virtual bool CanChangeIsSpatiallyLoadedFlag() const override { return false; }
#endif

	//////////////////////////////////////////////////////////////////////////
	/// Modules
public:	
#if WITH_EDITOR
	void GenerateModules();
    
	void DestroyModules();
#endif

	UFUNCTION(BlueprintNativeEvent)
    void PauseModules();
    
	UFUNCTION(BlueprintNativeEvent)
    void UnPauseModules();

protected:
	bool ValidateAndOrderModules();
	void EnsureWorldNetworkRoute();
	bool bModulesPrepared = false;
	bool bModulesTerminated = false;
	bool bTerminationInProgress = false;
	bool bModuleConfigurationValid = true;
	TWeakObjectPtr<AWorldNetworkRouteAuthority> WorldRouteAuthority;
	double LastControlRefreshTime = 0.0;

	UPROPERTY(EditAnywhere, Category = "Modules")
	TSubclassOf<UInventoryModule> InventoryModuleClass;

	/// 模块列表
	UPROPERTY(VisibleAnywhere)
	TArray<UModuleBase*> Modules;
	UPROPERTY()
	TMap<FName, UModuleBase*> ModuleMap;

public:
	bool RegisterModule(UModuleBase* InModule, int32 InIndex = INDEX_NONE);
	bool ReplaceModule(FName InName, UModuleBase* InModule);
	bool UnregisterModule(FName InName);
	bool MoveModule(FName InName, int32 InIndex);

	/**
	 * 是否在编辑器下
	 */
	bool IsInEditor() const { return GetWorld() && GetWorld()->IsEditorWorld() && !GetWorld()->IsGameWorld(); }
	/**
	 * 获取模块列表
	 */
	const TArray<UModuleBase*>& GetModules() const { return Modules; }
	/**
	 * 获取模块Map
	 */
	const TMap<FName, UModuleBase*>& GetModuleMap() const { return ModuleMap; }
	/**
	* 通过类型获取模块网络组件
	*/
	template<class T>
	static T* GetModuleNetworkComponentByClass(APlayerController* InController, TSubclassOf<T> InClass = T::StaticClass())
	{
		return Cast<T>(GetModuleNetworkComponent(InController, InClass));
	}

protected:
	static UModuleNetworkComponentBase* GetModuleNetworkComponent(APlayerController* InController, TSubclassOf<UModuleNetworkComponentBase> InClass);
	void RefreshModuleIndices();

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	//////////////////////////////////////////////////////////////////////////
	/// Editor
public:
#if WITH_EDITOR
	void GenerateModuleListItem(TArray<TSharedPtr<struct FModuleListItem>>& OutModuleListItems, const FString& InFilterText = TEXT(""));

	void UpdateModuleListItem(TArray<TSharedPtr<struct FModuleListItem>>& OutModuleListItems);

	bool CanAddModule(TSubclassOf<UModuleBase> InModuleClass) const;
#endif
};
