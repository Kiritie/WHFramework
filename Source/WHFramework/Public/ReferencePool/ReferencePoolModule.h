// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "ReferencePool.h"
#include "ReferencePoolInterface.h"
#include "Main/Base/ModuleBase.h"

#include "ReferencePoolModule.generated.h"

UCLASS()
class WHFRAMEWORK_API AReferencePoolModule : public AModuleBase
{
	GENERATED_BODY()
	
public:	
	// ParamSets default values for this actor's properties
	AReferencePoolModule();

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
	/// ReferencePool
protected:
	/// 单个对象池上限
	UPROPERTY(EditAnywhere)
	int32 Limit;

	UPROPERTY(VisibleAnywhere, Transient)
	TMap<UClass*, UReferencePool*> ReferencePools;

public:
	template<class T>
	bool HasReference(TSubclassOf<AActor> InType = T::StaticClass())
	{
		if(!InType || !InType->ImplementsInterface(UReferencePoolInterface::StaticClass())) return false;

		if (ReferencePools.Contains(InType))
		{
			return ReferencePools[InType]->GetCount() > 0;
		}
		return false;
	}
	
	template<class T>
	T* SpawnReference(TSubclassOf<AActor> InType = T::StaticClass())
	{
		if(!InType || !InType->ImplementsInterface(UReferencePoolInterface::StaticClass())) return nullptr;

		if (!ReferencePools.Contains(InType))
		{
			UReferencePool* ReferencePool = NewObject<UReferencePool>(this);
			const int32 TempLimit = IReferencePoolInterface::Execute_GetLimit(InType.GetDefaultObject());
			ReferencePool->Initialize(TempLimit != 0 ? TempLimit: Limit, InType);
			ReferencePools.Add(InType, ReferencePool);
		}
		return Cast<T>(ReferencePools[InType]->Spawn());
	}

	UFUNCTION(BlueprintCallable, meta = (DeterminesOutputType = "InType", DisplayName = "Spawn Actor"))
	AActor* K2_SpawnReference(TSubclassOf<AActor> InType);

	UFUNCTION(BlueprintCallable)
	void DespawnReference(AActor* InActor);

	UFUNCTION(BlueprintCallable)
	void ClearAllActor();
};
