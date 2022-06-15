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
	UPROPERTY(VisibleAnywhere, Transient)
	TMap<UClass*, UReferencePool*> ReferencePools;

public:
	template<class T>
	bool HasReference(TSubclassOf<UObject> InType = T::StaticClass())
	{
		if(!InType || !InType->ImplementsInterface(UReferencePoolInterface::StaticClass())) return false;

		if(ReferencePools.Contains(InType))
		{
			return ReferencePools[InType]->IsEmpty();
		}
		return false;
	}

	template<class T>
	T& GetReference(TSubclassOf<UObject> InType = T::StaticClass())
	{
		if(!InType || !InType->ImplementsInterface(UReferencePoolInterface::StaticClass())) return *NewObject<T>();

		if(!ReferencePools.Contains(InType))
		{
			UReferencePool* ReferencePool = NewObject<UReferencePool>(this);
			ReferencePool->Initialize(InType);
			ReferencePools.Add(InType, ReferencePool);
		}
		return static_cast<T&>(ReferencePools[InType]->Get());
	}

	template<class T>
	void SetReference(UObject* InObject, TSubclassOf<UObject> InType = T::StaticClass())
	{
		if(!InType || !InType->ImplementsInterface(UReferencePoolInterface::StaticClass())) return;

		if(!ReferencePools.Contains(InType))
		{
			UReferencePool* ReferencePool = NewObject<UReferencePool>(this);
			ReferencePool->Initialize(InType);
			ReferencePools.Add(InType, ReferencePool);
		}
		ReferencePools[InType]->Set(InObject);
	}

	template<class T>
	void ResetReference(TSubclassOf<UObject> InType = T::StaticClass())
	{
		if(!InType || !InType->ImplementsInterface(UReferencePoolInterface::StaticClass())) return;

		if(ReferencePools.Contains(InType))
		{
			ReferencePools[InType]->Reset();
		}
	}

	template<class T>
	void ClearReference(TSubclassOf<UObject> InType = T::StaticClass())
	{
		if(!InType || !InType->ImplementsInterface(UReferencePoolInterface::StaticClass())) return;

		if(ReferencePools.Contains(InType))
		{
			ReferencePools[InType]->Clear();
		}
	}

	void ClearAllReference();
};
