// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "ObjectPool.h"
#include "ObjectPoolInterface.h"
#include "Main/Base/ModuleBase.h"

#include "ObjectPoolModule.generated.h"

UCLASS()
class WHFRAMEWORK_API UObjectPoolModule : public UModuleBase
{
	GENERATED_BODY()
		
	GENERATED_MODULE(UObjectPoolModule)

public:
	// ParamSets default values for this actor's properties
	UObjectPoolModule();

	~UObjectPoolModule();

	//////////////////////////////////////////////////////////////////////////
	/// Module
public:
	#if WITH_EDITOR
	virtual void OnGenerate() override;

	virtual void OnDestroy() override;
	#endif

	virtual void OnInitialize() override;

	virtual void OnPreparatory(EPhase InPhase) override;

	virtual void OnRefresh(float DeltaSeconds) override;

	virtual void OnPause() override;

	virtual void OnUnPause() override;

	virtual void OnTermination(EPhase InPhase) override;

	//////////////////////////////////////////////////////////////////////////
	/// ObjectPool
protected:
	/// 默认引用池上限
	UPROPERTY(EditAnywhere)
	int32 DefaultLimit;

	UPROPERTY(VisibleAnywhere, Transient)
	TMap<TSubclassOf<UObject>, UObjectPool*> ObjectPools;

public:
	template<class T>
	bool HasPool(TSubclassOf<UObject> InType = T::StaticClass())
	{
		return HasPool(InType);
	}
	UFUNCTION(BlueprintPure)
	bool HasPool(TSubclassOf<UObject> InType) const;
	
	template<class T>
	UObjectPool* GetPool(TSubclassOf<UObject> InType = T::StaticClass())
	{
		return GetPool(InType);
	}
	UFUNCTION(BlueprintPure)
	UObjectPool* GetPool(TSubclassOf<UObject> InType) const;
	
	template<class T>
	UObjectPool* CreatePool(TSubclassOf<UObject> InType = T::StaticClass())
	{
		return CreatePool(InType);
	}
	UFUNCTION(BlueprintCallable)
	UObjectPool* CreatePool(TSubclassOf<UObject> InType);

	template<class T>
	void DestroyPool(TSubclassOf<UObject> InType = T::StaticClass())
	{
		DestroyPool(InType);
	}
	UFUNCTION(BlueprintCallable)
	void DestroyPool(TSubclassOf<UObject> InType);

	template<class T>
	bool HasObject(TSubclassOf<UObject> InType = T::StaticClass())
	{
		return HasObject(InType);
	}

	UFUNCTION(BlueprintCallable)
	bool HasObject(TSubclassOf<UObject> InType);

	template<class T>
	T* SpawnObject(const TArray<FParameter>* InParams = nullptr, TSubclassOf<UObject> InType = T::StaticClass())
	{
		return Cast<T>(SpawnObject(InType, InParams ? *InParams : TArray<FParameter>()));
	}
	template<class T>
	T* SpawnObject(const TArray<FParameter>& InParams, TSubclassOf<UObject> InType = T::StaticClass())
	{
		return Cast<T>(SpawnObject(InType, InParams));
	}
	UFUNCTION(BlueprintCallable, meta = (DeterminesOutputType = "InType", AutoCreateRefTerm = "InParams"))
	UObject* SpawnObject(TSubclassOf<UObject> InType, const TArray<FParameter>& InParams);

	UFUNCTION(BlueprintCallable)
	void DespawnObject(UObject* InObject, bool bRecovery = true);

	template<class T>
	void DespawnObjects(TArray<T*> InObjects, bool bRecovery = true)
	{
		for(auto Iter : InObjects)
		{
			DespawnObject(Iter, bRecovery);
		}
	}
	UFUNCTION(BlueprintCallable)
	void DespawnObjects(TArray<UObject*> InObjects, bool bRecovery = true);

	template<class T>
	void ClearObject(TSubclassOf<UObject> InType = T::StaticClass())
	{
		ClearObject(InType);
	}
	UFUNCTION(BlueprintCallable)
	void ClearObject(TSubclassOf<UObject> InType);

	UFUNCTION(BlueprintCallable)
	void ClearAllObject();
};
