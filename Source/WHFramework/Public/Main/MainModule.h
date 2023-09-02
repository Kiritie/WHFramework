// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Base/ModuleNetworkComponentBase.h"
#include "Base/ModuleBase.h"

#include "MainModule.generated.h"

UCLASS()
class WHFRAMEWORK_API AMainModule : public AModuleBase
{
#if WITH_EDITOR
	friend class FMainModuleDetailsPanel;
#endif
	
	GENERATED_BODY()
	
	GENERATED_MAIN_MODULE(AMainModule)

public:	
	// ParamSets default values for this actor's properties
	AMainModule();

	~AMainModule();
	
	//////////////////////////////////////////////////////////////////////////
	/// Module
public:
#if WITH_EDITOR
	virtual void OnGenerate() override;

	virtual void OnDestroy() override;
#endif
	
	virtual void OnInitialize_Implementation() override;

	virtual void OnPreparatory_Implementation(EPhase InPhase) override;

	virtual void OnRefresh_Implementation(float DeltaSeconds) override;

	virtual void OnPause_Implementation() override;

	virtual void OnUnPause_Implementation() override;

	virtual void OnTermination_Implementation(EPhase InPhase) override;

	//////////////////////////////////////////////////////////////////////////
	/// Modules
public:	
#if WITH_EDITOR
	virtual void GenerateModules();
    
	virtual void DestroyModules();
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
	TArray<AModuleBase*> ModuleRefs;
	UPROPERTY()
	TMap<FName, AModuleBase*> ModuleMap;

public:
	/**
	 * 通过类型运行模块
	 */
	template<class T>
	static void RunModuleByClass(TSubclassOf<T> InClass = T::StaticClass())
	{
		if(T* Module = GetModuleByClass<T>(false, InClass))
		{
			Module->Run();
		}
	}
	/**
	* 通过名称运行模块
	*/
	static void RunModuleByName(const FName InName)
	{
		if(AModuleBase* Module = GetModuleByName<AModuleBase>(InName))
		{
			Module->Run();
		}
	}
	/**
	 * 通过类型重置模块
	 */
	template<class T>
	static void ResetModuleByClass(TSubclassOf<T> InClass = T::StaticClass())
	{
		if(T* Module = GetModuleByClass<T>(false, InClass))
		{
			Module->Reset();
		}
	}
	/**
	* 通过名称重置模块
	*/
	static void ResetModuleByName(const FName InName)
	{
		if(AModuleBase* Module = GetModuleByName<AModuleBase>(InName))
		{
			Module->Reset();
		}
	}
	/**
	 * 通过类型暂停模块
	 */
	template<class T>
	static void PauseModuleByClass(TSubclassOf<T> InClass = T::StaticClass())
	{
		if(T* Module = GetModuleByClass<T>(false, InClass))
		{
			Module->Pause();
		}
	}
	/**
	* 通过名称暂停模块
	*/
	static void PauseModuleByName(const FName InName)
	{
		if(AModuleBase* Module = GetModuleByName<AModuleBase>(InName))
		{
			Module->Pause();
		}
	}
	/**
	 * 通过类型取消暂停模块
	 */
	template<class T>
	static void UnPauseModuleByClass(TSubclassOf<T> InClass = T::StaticClass())
	{
		if(T* Module = GetModuleByClass<T>(false, InClass))
		{
			Module->UnPause();
		}
	}
	/**
	* 通过名称取消暂停模块
	*/
	static void UnPauseModuleByName(const FName InName)
	{
		if(AModuleBase* Module = GetModuleByName<AModuleBase>(InName))
		{
			Module->UnPause();
		}
	}

public:
	/**
	* 获取所有模块
	*/
	static TArray<AModuleBase*> GetAllModule(bool bInEditor = false)
	{
		AMainModule* MainModule = Get(bInEditor);
		if(MainModule && MainModule->IsValidLowLevel())
		{
			return MainModule->ModuleRefs;
		}
		return TArray<AModuleBase*>();
	}
	/**
	 * 通过类型获取模块
	 */
	template<class T>
	static T* GetModuleByClass(bool bInEditor = false, TSubclassOf<T> InClass = T::StaticClass())
	{
		const FName ModuleName = InClass.GetDefaultObject()->GetModuleName();
		return GetModuleByName<T>(ModuleName, bInEditor);
	}
	/**
	* 通过名称获取模块
	*/
	template<class T>
	static T* GetModuleByName(const FName InName, bool bInEditor = false)
	{
		AMainModule* MainModule = Get(bInEditor);
		if(MainModule && MainModule->IsValidLowLevel())
		{
			if(MainModule->ModuleMap.Contains(InName))
			{
				return Cast<T>(MainModule->ModuleMap[InName]);
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
	
	static UModuleNetworkComponentBase* GetModuleNetworkComponentByClass(TSubclassOf<UModuleNetworkComponentBase> InModuleNetworkComponentClass, bool bInEditor = false);

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
