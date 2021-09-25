// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "SpawnPool.h"
#include "Base/ModuleBase.h"

#include "SpawnPoolModule.generated.h"

UCLASS()
class WHFRAMEWORK_API ASpawnPoolModule : public AModuleBase
{
	GENERATED_BODY()
	
public:	
	// ParamSets default values for this actor's properties
	ASpawnPoolModule();

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

	virtual void OnTermination_Implementation() override;

	//////////////////////////////////////////////////////////////////////////
	/// SpawnPool
protected:
	/// 单个对象池上限
	UPROPERTY(EditAnywhere)
	int32 Limit;

	UPROPERTY(VisibleAnywhere)
	TMap<UClass*, USpawnPool*> SpawnPools;

public:
	template<class T>
	T* SpawnActor(TSubclassOf<AActor> InType = T::StaticClass())
	{
		if(!InType) return nullptr;

		if (!SpawnPools.Contains(InType))
		{
			USpawnPool* SpawnPool = NewObject<USpawnPool>(this);
			SpawnPool->Initialize(Limit, InType);
			SpawnPools.Add(InType, SpawnPool);
		}
		return Cast<T>(SpawnPools[InType]->Spawn());
	}

	UFUNCTION(BlueprintCallable, meta = (DeterminesOutputType = "InType", DisplayName = "Spawn Actor"))
	AActor* K2_SpawnActor(TSubclassOf<AActor> InType);

	UFUNCTION(BlueprintCallable)
	void DespawnActor(AActor* InActor);

	UFUNCTION(BlueprintCallable)
	void ClearAllActor();
};
