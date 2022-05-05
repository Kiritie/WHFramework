// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "ObjectPool.h"
#include "ObjectPoolInterface.h"
#include "Actor/ActorPool.h"
#include "Main/Base/ModuleBase.h"
#include "Widget/WidgetPool.h"
#include "Widget/Interfaces/BaseWidgetInterface.h"

#include "ObjectPoolModule.generated.h"

UCLASS()
class WHFRAMEWORK_API AObjectPoolModule : public AModuleBase
{
	GENERATED_BODY()

public:
	// ParamSets default values for this actor's properties
	AObjectPoolModule();

	//////////////////////////////////////////////////////////////////////////
	/// Module
public:
	#if WITH_EDITOR
	virtual void OnGenerate_Implementation() override;

	virtual void OnDestroy_Implementation() override;
	#endif

	virtual void OnInitialize_Implementation() override;

	virtual void OnPreparatory_Implementation() override;

	virtual void OnRefresh_Implementation(float DeltaSeconds) override;

	virtual void OnPause_Implementation() override;

	virtual void OnUnPause_Implementation() override;

	virtual void OnTermination_Implementation() override;

	//////////////////////////////////////////////////////////////////////////
	/// ObjectPool
protected:
	/// 单个引用池上限
	UPROPERTY(EditAnywhere)
	int32 Limit;

	UPROPERTY(VisibleAnywhere, Transient)
	TMap<UClass*, UObjectPool*> ObjectPools;

public:
	template<class T>
	bool HasObject(TSubclassOf<UObject> InType = T::StaticClass())
	{
		if(!InType || !InType->ImplementsInterface(UObjectPoolInterface::StaticClass())) return false;

		if(ObjectPools.Contains(InType))
		{
			return ObjectPools[InType]->GetCount() > 0;
		}
		return false;
	}

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Has Object"))
	bool K2_HasObject(TSubclassOf<UObject> InType);

	template<class T>
	T* SpawnObject(TSubclassOf<UObject> InType = T::StaticClass())
	{
		if(!InType || !InType->ImplementsInterface(UObjectPoolInterface::StaticClass())) return nullptr;

		if(!ObjectPools.Contains(InType))
		{
			UObjectPool* ObjectPool = nullptr;
			if(InType->IsChildOf(AActor::StaticClass()))
			{
				ObjectPool = NewObject<UActorPool>(this);
			}
			else if(InType->ImplementsInterface(UBaseWidgetInterface::StaticClass()))
			{
				ObjectPool = NewObject<UWidgetPool>(this);
			}
			else
			{
				ObjectPool = NewObject<UObjectPool>(this);
			}
			const int32 TempLimit = IObjectPoolInterface::Execute_GetLimit(InType.GetDefaultObject());
			ObjectPool->Initialize(TempLimit != 0 ? TempLimit : Limit, InType);
			ObjectPools.Add(InType, ObjectPool);
		}
		return Cast<T>(ObjectPools[InType]->Spawn());
	}

	UFUNCTION(BlueprintCallable, meta = (DeterminesOutputType = "InType", DisplayName = "Spawn Object"))
	UObject* K2_SpawnObject(TSubclassOf<UObject> InType);

	UFUNCTION(BlueprintCallable)
	void DespawnObject(UObject* InObject);

	template<class T>
	void ClearObject(TSubclassOf<UObject> InType = T::StaticClass())
	{
		if(!InType || !InType->ImplementsInterface(UObjectPoolInterface::StaticClass())) return;

		if(ObjectPools.Contains(InType))
		{
			ObjectPools[InType]->Clear();
		}
	}

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Clear Object"))
	void K2_ClearObject(TSubclassOf<UObject> InType);

	UFUNCTION(BlueprintCallable)
	void ClearAllObject();
};
