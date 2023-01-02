// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "WHFramework.h"

#include "ReferencePool.h"
#include "ReferencePoolInterface.h"
#include "Main/Base/ModuleBase.h"

#include "ReferencePoolModule.generated.h"

UCLASS()
class WHFRAMEWORK_API AReferencePoolModule : public AModuleBase
{
	GENERATED_BODY()
		
	MODULE_INSTANCE_DECLARE(AReferencePoolModule)

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
	TMap<TSubclassOf<UObject>, UReferencePool*> ReferencePools;

public:
	template<class T>
	bool HasReference(TSubclassOf<T> InType = T::StaticClass())
	{
		if(!InType || !InType->ImplementsInterface(UReferencePoolInterface::StaticClass())) return false;

		if(ReferencePools.Contains(InType))
		{
			return ReferencePools[InType]->IsEmpty();
		}
		return false;
	}

	template<class T>
	void CreateReference(UObject* InObject = nullptr, TSubclassOf<T> InType = T::StaticClass())
	{
		if(!InType || !InType->ImplementsInterface(UReferencePoolInterface::StaticClass())) return;

		GetOrCreatePool(InType)->Create(InObject);
	}

	template<class T>
	T& GetReference(bool bReset = false, TSubclassOf<T> InType = T::StaticClass())
	{
		if(!InType || !InType->ImplementsInterface(UReferencePoolInterface::StaticClass())) return *NewObject<T>(GetTransientPackage(), InType);

		return GetOrCreatePool(InType)->template Get<T>(bReset);
	}

	template<class T>
	void ResetReference(TSubclassOf<T> InType = T::StaticClass())
	{
		if(!InType || !InType->ImplementsInterface(UReferencePoolInterface::StaticClass())) return;

		if(ReferencePools.Contains(InType))
		{
			ReferencePools[InType]->Reset();
		}
	}

	template<class T>
	void ClearReference(TSubclassOf<T> InType = T::StaticClass())
	{
		if(!InType || !InType->ImplementsInterface(UReferencePoolInterface::StaticClass())) return;

		if(ReferencePools.Contains(InType))
		{
			ReferencePools[InType]->Clear();
		}
	}

	void ClearAllReference();

protected:
	template<class T>
	UReferencePool* GetOrCreatePool(TSubclassOf<T> InType = T::StaticClass())
	{
		if(!InType || !InType->ImplementsInterface(UReferencePoolInterface::StaticClass())) return nullptr;

		if(!ReferencePools.Contains(InType))
		{
			UReferencePool* ReferencePool = NewObject<UReferencePool>(this);
			ReferencePool->Initialize(InType);
			ReferencePools.Add(InType, ReferencePool);
		}
		return ReferencePools[InType];
	}
};
