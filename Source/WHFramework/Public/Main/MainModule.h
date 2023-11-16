// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Base/ModuleBase.h"
#include "Base/ModuleNetworkComponentBase.h"
#include "Common/Base/WHActor.h"

#include "MainModule.generated.h"

UCLASS(hidecategories = (Tick, Replication, Collision, Actor, Input, LOD, Cooking, Hidden, WorldPartition, Hlod, Networking, Physics, LevelInstance))
class WHFRAMEWORK_API AMainModule : public AWHActor
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
	virtual void OnGenerate();

	virtual void OnDestroy();
#endif
	
	virtual void OnInitialize_Implementation() override;

	virtual void OnPreparatory_Implementation(EPhase InPhase) override;

	virtual void OnRefresh_Implementation(float DeltaSeconds) override;

	virtual void OnTermination_Implementation(EPhase InPhase) override;

protected:
	virtual bool IsDefaultLifecycle_Implementation() const override { return false; }

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
	/// 模块列表
	UPROPERTY(VisibleAnywhere)
	TArray<UModuleBase*> Modules;
	UPROPERTY()
	TMap<FName, UModuleBase*> ModuleMap;

public:
	/**
	 * 获取模块列表
	 */
	TArray<UModuleBase*>& GetModules() { return Modules; }
	/**
	 * 获取模块Map
	 */
	TMap<FName, UModuleBase*>& GetModuleMap() { return ModuleMap; }
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
		if(UModuleBase* Module = GetModuleByName<UModuleBase>(InName))
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
		if(UModuleBase* Module = GetModuleByName<UModuleBase>(InName))
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
		if(UModuleBase* Module = GetModuleByName<UModuleBase>(InName))
		{
			Module->Pause();
		}
	}
	/**
	 * 通过类型恢复模块
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
	* 通过名称恢复模块
	*/
	static void UnPauseModuleByName(const FName InName)
	{
		if(UModuleBase* Module = GetModuleByName<UModuleBase>(InName))
		{
			Module->UnPause();
		}
	}

public:
	/**
	* 获取所有模块
	*/
	template<class T>
	static TArray<T*> GetAllModule(bool bInEditor = false)
	{
		TArray<T*> Modules;
		for(auto Iter : GetAllModule(bInEditor))
		{
			if(T* Module = Cast<T>(Iter))
			{
				Modules.Add(Module);
			}
		}
		return Modules;
	}
	static TArray<UModuleBase*> GetAllModule(bool bInEditor = false)
	{
		if(AMainModule* MainModule = GetPtr(bInEditor))
		{
			return MainModule->GetModules();
		}
		return TArray<UModuleBase*>();
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
		if(AMainModule* MainModule = GetPtr(bInEditor))
		{
			if(MainModule->ModuleMap.Contains(InName))
			{
				return Cast<T>(MainModule->ModuleMap[InName]);
			}
			ensureEditorMsgf(false, FString::Printf(TEXT("Failed to get module, module name: %s"), *InName.ToString()), EDC_Default, EDV_Error); \
		}
		return nullptr;
	}
	/**
	* 通过类型获取模块网络组件
	*/
	template<class T>
	static T* GetModuleNetworkComponentByClass(TSubclassOf<T> InClass = T::StaticClass())
	{
		return Cast<T>(GetModuleNetworkComponent(InClass));
	}

protected:
	static UModuleNetworkComponentBase* GetModuleNetworkComponent(TSubclassOf<UModuleNetworkComponentBase> InClass);

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	//////////////////////////////////////////////////////////////////////////
	/// Editor
public:
	#if WITH_EDITOR
	void GenerateListItem(TArray<TSharedPtr<struct FModuleListItem>>& OutModuleListItems);

	void UpdateListItem(TArray<TSharedPtr<struct FModuleListItem>>& OutModuleListItems);
	#endif
};
