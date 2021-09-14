// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
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

	//////////////////////////////////////////////////////////////////////////
	/// SpawnPool
protected:
	/// 单个对象池上限
	UPROPERTY(EditAnywhere)
	int32 Limit;

private:
	UPROPERTY()
	TMap<UClass*, class USpawnPool*> SpawnPools;

public:
	template<class T>
	T* SpawnActor(TSubclassOf<AActor> InType = nullptr)
	{
		return Cast<T>(K2_SpawnActor(InType != nullptr ? InType.Get() : T::StaticClass()));
	}

	UFUNCTION(BlueprintCallable, meta = (DeterminesOutputType = "InType", DisplayName = "Spawn Actor"))
	AActor* K2_SpawnActor(TSubclassOf<AActor> InType);

	UFUNCTION(BlueprintCallable)
	void DespawnActor(AActor* InActor);

	UFUNCTION(BlueprintCallable)
	void ClearAllActor();
};
