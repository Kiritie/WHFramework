// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "FSMModuleTypes.h"
#include "Main/Base/ModuleBase.h"

#include "FSMModule.generated.h"

class UFiniteStateBase;
class UFSMComponent;
/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UFSMModule : public UModuleBase
{
	GENERATED_BODY()
		
	GENERATED_MODULE(UFSMModule)

public:
	// ParamSets default values for this actor's properties
	UFSMModule();

	~UFSMModule();

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

	//////////////////////////////////////////////////////////////////////////
	/// FSM
public:
	UFUNCTION(BlueprintCallable)
	void RegisterFSM(UFSMComponent* InFSM);

	UFUNCTION(BlueprintCallable)
	void UnregisterFSM(UFSMComponent* InFSM);

	//////////////////////////////////////////////////////////////////////////
	/// FSMGroup
public:
	void SwitchGroupStateByIndex(const FName InGroupName, int32 InStateIndex, const TArray<FParameter>* InParams = nullptr)
	{
		SwitchGroupStateByIndex(InGroupName, InStateIndex, InParams ? *InParams : TArray<FParameter>());
	}

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InParams"))
	void SwitchGroupStateByIndex(const FName InGroupName, int32 InStateIndex, const TArray<FParameter>& InParams);

	void SwitchGroupStateByName(const FName InGroupName, const FName InStateName, const TArray<FParameter>* InParams = nullptr)
	{
		SwitchGroupStateByName(InGroupName, InStateName, InParams ? *InParams : TArray<FParameter>());
	}

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InParams"))
	void SwitchGroupStateByName(const FName InGroupName, const FName InStateName, const TArray<FParameter>& InParams);

	template<class T>
	void SwitchGroupStateByClass(const FName InGroupName, const TArray<FParameter>* InParams = nullptr, TSubclassOf<UFiniteStateBase> InStateClass = T::StaticClass())
	{
		SwitchGroupStateByClass(InGroupName, InStateClass, InParams ? *InParams : TArray<FParameter>());
	}

	template<class T>
	void SwitchGroupStateByClass(const FName InGroupName, const TArray<FParameter>& InParams, TSubclassOf<UFiniteStateBase> InStateClass = T::StaticClass())
	{
		SwitchGroupStateByClass(InGroupName, InStateClass, InParams);
	}

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InParams"))
	void SwitchGroupStateByClass(const FName InGroupName, TSubclassOf<UFiniteStateBase> InStateClass, const TArray<FParameter>& InParams);

	void SwitchGroupDefaultState(const FName InGroupName, const TArray<FParameter>* InParams = nullptr)
	{
		SwitchGroupDefaultState(InGroupName, InParams ? *InParams : TArray<FParameter>());
	}

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InParams"))
	void SwitchGroupDefaultState(const FName InGroupName, const TArray<FParameter>& InParams);

	void SwitchGroupFinalState(const FName InGroupName, const TArray<FParameter>* InParams = nullptr)
	{
		SwitchGroupFinalState(InGroupName, InParams ? *InParams : TArray<FParameter>());
	}

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InParams"))
	void SwitchGroupFinalState(const FName InGroupName, const TArray<FParameter>& InParams);

	void SwitchGroupLastState(const FName InGroupName, const TArray<FParameter>* InParams = nullptr)
	{
		SwitchGroupLastState(InGroupName, InParams ? *InParams : TArray<FParameter>());
	}

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InParams"))
	void SwitchGroupLastState(const FName InGroupName, const TArray<FParameter>& InParams);

	void SwitchGroupNextState(const FName InGroupName, const TArray<FParameter>* InParams = nullptr)
	{
		SwitchGroupNextState(InGroupName, InParams ? *InParams : TArray<FParameter>());
	}

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InParams"))
	void SwitchGroupNextState(const FName InGroupName, const TArray<FParameter>& InParams);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FSMGroup")
	TMap<FName, FFSMGroup> GroupMap;

public:
	UFUNCTION(BlueprintPure)
	bool HasGroup(const FName InGroupName) const
	{
		return GroupMap.Contains(InGroupName);
	}

	UFUNCTION(BlueprintPure)
	FFSMGroup& GetGroup(const FName InGroupName)
	{
		if(HasGroup(InGroupName))
		{
			return GroupMap[InGroupName];
		}
		static FFSMGroup TempGroup;
		return TempGroup;
	}
};
