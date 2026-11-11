// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "MainTypes.h"
#include "Type/NativeTypeIdentity.h"
#include "Base/ManagerBase.h"
#include "Containers/Ticker.h"

class WHFRAMEWORKCORE_API FMainManager : public FManagerBase
{
	GENERATED_MAIN_MANAGER(FMainManager)
	
public:
	// ParamSets default values for this actor's properties
	FMainManager();

	virtual ~FMainManager() override;
	
	static const FUniqueType Type;

	//////////////////////////////////////////////////////////////////////////
	/// Manager
public:
	virtual void OnInitialize() override;

	virtual void OnPreparatory() override;

	virtual void OnRefresh(float DeltaSeconds) override;

	virtual void OnTermination() override;
	static void Startup();
	static void Shutdown();

protected:
	virtual void OnWorldAdded(UWorld* InWorld);

protected:
	/// 管理器列表
	TMap<FUniqueType, FManagerBase*> ManagerMap;
	FDelegateHandle WorldAddedHandle;
	FTSTicker::FDelegateHandle RefreshHandle;
	bool bTerminating = false;

public:
	/**
	 * 是否存在指定类型的管理器
	 */
	template<typename T>
	static bool IsExistManager()
	{
		const FMainManager* Owner = GetPtr();
		return Owner && Owner->ManagerMap.Contains(T::Type);
	}
	/**
	 * 注册指定类型的管理器
	 */
	template<typename T>
	static void RegisterManager(bool bForce = false)
	{
		FMainManager* Owner = GetPtr();
		if (!Owner || Owner->bTerminating)
		{
			UE_LOG(LogTemp, Error, TEXT("Cannot register a manager while the registry is terminating."));
			return;
		}
		if(bForce)
		{
			UnRegisterManager<T>();
			Owner = GetPtr();
			if (!Owner || Owner->bTerminating) return;
		}
		
		if(!IsExistManager<T>())
		{
			T* Manager = new T();
			Owner->ManagerMap.Add(T::Type, Manager);
			Manager->OnInitialize();
		}
	}
	/**
	 * 取消指定类型的管理器
	 */
	template<typename T>
	static void UnRegisterManager()
	{
		FMainManager* Owner = GetPtr();
		if (!Owner) return;
		FManagerBase* Manager = nullptr;
		if (Owner->ManagerMap.RemoveAndCopyValue(T::Type, Manager) && Manager)
		{
			Manager->OnTermination();
			delete Manager;
		}
	}
	/**
	 * 获取指定类型的管理器
	 */
	template<typename T>
	static T* GetManager()
	{
		FMainManager* Owner = GetPtr();
		if(Owner)
		{
			if (FManagerBase* const* Manager = Owner->ManagerMap.Find(T::Type)) return (*Manager)->template Cast<T>();
		}
		return nullptr;
	}
	/**
	* 获取所有管理器
	*/
	static TArray<FManagerBase*> GetAllManager()
	{
		TArray<FManagerBase*> Managers;
		const FMainManager* Owner = GetPtr();
		if (!Owner) return Managers;
		for(auto Iter : Owner->ManagerMap)
		{
			Managers.Add(Iter.Value);
		}
		return Managers;
	}
};
