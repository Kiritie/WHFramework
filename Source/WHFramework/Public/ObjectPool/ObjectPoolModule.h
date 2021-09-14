// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "Base/ModuleBase.h"

#include "ObjectPoolModule.generated.h"

class UObjectPool;

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

	virtual void OnRefresh_Implementation(float DeltaSeconds) override;

	virtual void OnPause_Implementation() override;

	virtual void OnUnPause_Implementation() override;

	//////////////////////////////////////////////////////////////////////////
	/// ObjectPool
protected:
	/// 单个引用池上限
	UPROPERTY(EditAnywhere)
	int32 Limit;

private:
	UPROPERTY()
	TMap<UClass*, UObjectPool*> ObjectPools;

public:
	template<class T>
	T* SpawnObject(TSubclassOf<UObject> InType = nullptr)
	{
		return Cast<T>(K2_SpawnObject(InType != nullptr ? InType.Get() : T::StaticClass()));
	}

	UFUNCTION(BlueprintCallable, meta = (DeterminesOutputType = "InType", DisplayName = "Spawn Object"))
	UObject* K2_SpawnObject(TSubclassOf<UObject> InType);

	UFUNCTION(BlueprintCallable)
	void DespawnObject(UObject* InObject);

	UFUNCTION(BlueprintCallable)
	void ClearAllObject();
};
