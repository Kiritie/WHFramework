// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "ReferencePoolInterface.h"
#include "Main/Base/ModuleBase.h"

#include "ReferencePoolModule.generated.h"

UCLASS()
class WHFRAMEWORK_API UReferencePoolModule : public UModuleBase
{
	GENERATED_BODY()

	GENERATED_MODULE(UReferencePoolModule)

public:
	UReferencePoolModule();

	~UReferencePoolModule();

	//////////////////////////////////////////////////////////////////////////
	/// ModuleBase
public:
#if WITH_EDITOR
	virtual void OnGenerate() override;

	virtual void OnDestroy() override;
#endif

	virtual void OnInitialize() override;

	virtual void OnPreparatory(EPhase InPhase) override;

	virtual void OnRefresh(float DeltaSeconds, bool bInEditor) override;

	virtual void OnPause() override;

	virtual void OnUnPause() override;

	virtual void OnTermination(EPhase InPhase) override;

	virtual FString GetModuleDebugMessage() override;

	//////////////////////////////////////////////////////////////////////////
	/// Reference
protected:
	UPROPERTY(VisibleAnywhere, Transient)
	TMap<TSubclassOf<UObject>, TObjectPtr<UObject>> References;

public:
	bool HasReference(UClass* InType) const;

	UObject* GetReference(UClass* InType);

	bool ResetReference(UClass* InType);

	bool ClearReference(UClass* InType);

	void ClearAllReference();

	template<class T>
	bool HasReference(TSubclassOf<T> InType = T::StaticClass()) const
	{
		return HasReference(InType.Get());
	}

	template<class T>
	T& GetReference(TSubclassOf<T> InType = T::StaticClass())
	{
		UObject* Object = GetReference(InType.Get());
		checkf(Object, TEXT("Failed to get runtime reference for class %s."), *GetNameSafe(InType.Get()));
		return *CastChecked<T>(Object);
	}

	template<class T>
	bool ResetReference(TSubclassOf<T> InType = T::StaticClass())
	{
		return ResetReference(InType.Get());
	}

	template<class T>
	bool ClearReference(TSubclassOf<T> InType = T::StaticClass())
	{
		return ClearReference(InType.Get());
	}

protected:
	bool IsValidReferenceType(UClass* InType) const;
};
