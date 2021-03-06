// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Main/Base/Module.h"
#include "MainModuleTypes.h"
#include "Debug/DebugModuleTypes.h"
#include "Main/Base/ModuleBase.h"
#include "Main/Base/ModuleNetworkComponent.h"
#include "Gameplay/WHPlayerController.h"
#include "Kismet/GameplayStatics.h"

#include "MainModule.generated.h"

class AModuleBase;

UCLASS()
class WHFRAMEWORK_API AMainModule : public AWHActor
{
	GENERATED_BODY()
	
public:	
	// ParamSets default values for this actor's properties
	AMainModule();

	virtual ~AMainModule() override;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	//////////////////////////////////////////////////////////////////////////
	/// Instance
protected:
	static AMainModule* Instance;
	#if WITH_EDITOR
	static AMainModule* InstanceEditor;
	#endif

public:
	static AMainModule* Get(bool bInEditor = false);

	//////////////////////////////////////////////////////////////////////////
	/// Module
public:	
#if WITH_EDITOR
	UFUNCTION(CallInEditor, BlueprintNativeEvent, Category = "MainModule")
	void GenerateModules();
    
	UFUNCTION(CallInEditor, BlueprintNativeEvent, Category = "MainModule")
	void DestroyModules();
#endif

	UFUNCTION(BlueprintNativeEvent, Category = "MainModule")
	void InitializeModules();
    
	UFUNCTION(BlueprintNativeEvent, Category = "MainModule")
	void PreparatoryModules();

	UFUNCTION(BlueprintNativeEvent, Category = "MainModule")
    void RefreshModules(float DeltaSeconds);
    
	UFUNCTION(BlueprintNativeEvent, Category = "MainModule")
    void PauseModules();
    
	UFUNCTION(BlueprintNativeEvent, Category = "MainModule")
    void UnPauseModules();

	UFUNCTION(BlueprintNativeEvent, Category = "MainModule")
	void TerminationModules();

protected:
	/// 模块类
	UPROPERTY(EditAnywhere, Category = "MainModule")
	TArray<TSubclassOf<AModuleBase>> ModuleClasses;
	/// 模块列表
	UPROPERTY(EditAnywhere, Replicated, Category = "MainModule")
	TArray<TScriptInterface<IModule>> ModuleRefs;

private:
	UPROPERTY()
	TMap<FName, TScriptInterface<IModule>> ModuleMap;

public:
	/**
	* 获取所有模块
	*/
	static TArray<TScriptInterface<IModule>> GetAllModule(bool bInEditor = false)
	{
		if(Get(bInEditor) && Get(bInEditor)->IsValidLowLevel())
		{
			return Get(bInEditor)->ModuleRefs;
		}
		return TArray<TScriptInterface<IModule>>();
	}
	/**
	 * 通过类型获取模块
	 */
	template<class T>
	static T* GetModuleByClass(bool bInEditor = false, TSubclassOf<AModuleBase> InModuleClass = T::StaticClass())
	{
		if(Get(bInEditor) && Get(bInEditor)->IsValidLowLevel())
		{
			const FName ModuleName = IModule::Execute_GetModuleName(InModuleClass.GetDefaultObject());
			return GetModuleByName<T>(ModuleName, bInEditor);
		}
		return nullptr;
	}
	/**
	* 通过名称获取模块
	*/
	template<class T>
	static T* GetModuleByName(const FName InModuleName, bool bInEditor = false)
	{
		if(Get(bInEditor) && Get(bInEditor)->IsValidLowLevel())
		{
			if(Get(bInEditor)->ModuleMap.Contains(InModuleName))
			{
				return Cast<T>(Get(bInEditor)->ModuleMap[InModuleName].GetObject());
			}
		}
		return nullptr;
	}
	/**
	* 通过类型获取模块网络组件
	*/
	template<class T>
	static T* GetModuleNetworkComponentByClass(bool bInEditor = false, TSubclassOf<UModuleNetworkComponent> InModuleNetworkComponentClass = T::StaticClass())
	{
		return Cast<T>(GetModuleNetworkComponentByClass(InModuleNetworkComponentClass, bInEditor));
	}
	
	static UModuleNetworkComponent* GetModuleNetworkComponentByClass(TSubclassOf<UModuleNetworkComponent> InModuleNetworkComponentClass, bool bInEditor = false);

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
