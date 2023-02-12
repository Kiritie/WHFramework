// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "WHFramework.h"
#include "Base/ModuleNetworkComponent.h"
#include "Base/ModuleBase.h"

#include "MainModule.generated.h"

UCLASS()
class WHFRAMEWORK_API AMainModule : public AModuleBase
{
	GENERATED_BODY()
	
	MODULE_INSTANCE_DECLARE(AMainModule)

public:	
	// ParamSets default values for this actor's properties
	AMainModule();
	
	//////////////////////////////////////////////////////////////////////////
	/// Module
public:
#if WITH_EDITOR
	virtual void OnGenerate_Implementation() override;

	virtual void OnDestroy_Implementation() override;
#endif
	
	virtual void OnInitialize_Implementation() override;

	virtual void OnPreparatory_Implementation(EPhase InPhase) override;

	virtual void OnRefresh_Implementation(float DeltaSeconds) override;

	virtual void OnPause_Implementation() override;

	virtual void OnUnPause_Implementation() override;

	virtual void OnTermination_Implementation() override;

protected:
	virtual bool IsCustomLifecycle_Implementation() const override { return true; }

	//////////////////////////////////////////////////////////////////////////
	/// Modules
public:	
#if WITH_EDITOR
	UFUNCTION(CallInEditor, BlueprintNativeEvent)
	void GenerateModules();
    
	UFUNCTION(CallInEditor, BlueprintNativeEvent)
	void DestroyModules();
#endif

	UFUNCTION(BlueprintNativeEvent)
    void PauseModules();
    
	UFUNCTION(BlueprintNativeEvent)
    void UnPauseModules();

protected:
	/// 模块类
	UPROPERTY(EditAnywhere)
	TArray<TSubclassOf<AModuleBase>> ModuleClasses;
	/// 模块列表
	UPROPERTY(EditAnywhere, Replicated)
	TArray<TScriptInterface<IModule>> ModuleRefs;
	UPROPERTY()
	TMap<FName, TScriptInterface<IModule>> ModuleMap;

public:
	/**
	 * 通过类型运行模块
	 */
	template<class T>
	static void RunModuleByClass(TSubclassOf<T> InModuleClass = T::StaticClass())
	{
		if(T* Module = GetModuleByClass<T>(false, InModuleClass))
		{
			Module->Execute_Run(Module);
		}
	}
	/**
	* 通过名称运行模块
	*/
	static void RunModuleByName(const FName InModuleName)
	{
		if(TScriptInterface<IModule> Module = GetModuleByName<UObject>(InModuleName))
		{
			Module->Execute_Run(Module.GetObject());
		}
	}
	/**
	 * 通过类型暂停模块
	 */
	template<class T>
	static void PauseModuleByClass(TSubclassOf<T> InModuleClass = T::StaticClass())
	{
		if(T* Module = GetModuleByClass<T>(false, InModuleClass))
		{
			Module->Execute_Pause(Module);
		}
	}
	/**
	* 通过名称暂停模块
	*/
	static void PauseModuleByName(const FName InModuleName)
	{
		if(TScriptInterface<IModule> Module = GetModuleByName<UObject>(InModuleName))
		{
			Module->Execute_Pause(Module.GetObject());
		}
	}
	/**
	 * 通过类型取消暂停模块
	 */
	template<class T>
	static void UnPauseModuleByClass(TSubclassOf<T> InModuleClass = T::StaticClass())
	{
		if(T* Module = GetModuleByClass<T>(false, InModuleClass))
		{
			Module->Execute_UnPause(Module);
		}
	}
	/**
	* 通过名称取消暂停模块
	*/
	static void UnPauseModuleByName(const FName InModuleName)
	{
		if(TScriptInterface<IModule> Module = GetModuleByName<UObject>(InModuleName))
		{
			Module->Execute_UnPause(Module.GetObject());
		}
	}

public:
	/**
	* 获取所有模块
	*/
	static TArray<TScriptInterface<IModule>> GetAllModule(bool bInEditor = false)
	{
		AMainModule* MainModule = Get(bInEditor);
		if(MainModule && MainModule->IsValidLowLevel())
		{
			return MainModule->ModuleRefs;
		}
		return TArray<TScriptInterface<IModule>>();
	}
	/**
	 * 通过类型获取模块
	 */
	template<class T>
	static T* GetModuleByClass(bool bInEditor = false, TSubclassOf<T> InModuleClass = T::StaticClass())
	{
		const FName ModuleName = IModule::Execute_GetModuleName(InModuleClass.GetDefaultObject());
		return GetModuleByName<T>(ModuleName, bInEditor);
	}
	/**
	* 通过名称获取模块
	*/
	template<class T>
	static T* GetModuleByName(const FName InModuleName, bool bInEditor = false)
	{
		AMainModule* MainModule = Get(bInEditor);
		if(MainModule && MainModule->IsValidLowLevel())
		{
			if(MainModule->ModuleMap.Contains(InModuleName))
			{
				return Cast<T>(MainModule->ModuleMap[InModuleName].GetObject());
			}
		}
		return nullptr;
	}
	/**
	* 通过类型获取模块网络组件
	*/
	template<class T>
	static T* GetModuleNetworkComponentByClass(bool bInEditor = false, TSubclassOf<T> InModuleNetworkComponentClass = T::StaticClass())
	{
		return Cast<T>(GetModuleNetworkComponentByClass(InModuleNetworkComponentClass, bInEditor));
	}
	
	static UModuleNetworkComponent* GetModuleNetworkComponentByClass(TSubclassOf<UModuleNetworkComponent> InModuleNetworkComponentClass, bool bInEditor = false);

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
