// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "MainTypes.h"
#include "Base/ManagerBase.h"

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

protected:
#if WITH_ENGINE
	virtual void OnWorldAdded(UWorld* InWorld);
#endif

protected:
	/// 管理器列表
	TMap<FUniqueType, FManagerBase*> ManagerMap;

public:
	/**
	 * 是否存在指定类型的管理器
	 */
	template<typename T>
	static bool IsExistManager()
	{
		return Get().ManagerMap.Contains(T::Type);
	}
	/**
	 * 注册指定类型的管理器
	 */
	template<typename T>
	static void RegisterManager(bool bForce = false)
	{
		if(bForce) UnRegisterManager<T>();
		
		if(!IsExistManager<T>())
		{
			T* Manager = new T();
			Manager->OnInitialize();
			Get().ManagerMap.Add(T::Type, Manager);
		}
	}
	/**
	 * 取消指定类型的管理器
	 */
	template<typename T>
	static void UnRegisterManager()
	{
		if(T* Manager = GetManager<T>())
		{
			Manager->OnTermination();
			Get().ManagerMap.Remove(T::Type);
		}
	}
	/**
	 * 获取指定类型的管理器
	 */
	template<typename T>
	static T* GetManager()
	{
		if(IsExistManager<T>())
		{
			return Get().ManagerMap[T::Type]->template CastTo<T>();
		}
		return nullptr;
	}
	/**
	* 获取所有管理器
	*/
	static TArray<FManagerBase*> GetAllManager()
	{
		TArray<FManagerBase*> Managers;
		for(auto Iter : Get().ManagerMap)
		{
			Managers.Add(Iter.Value);
		}
		return Managers;
	}
};
