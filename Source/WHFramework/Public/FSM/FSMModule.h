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
	UFUNCTION(BlueprintCallable)
	void SwitchGroupStateByIndex(const FName InGroupName, int32 InStateIndex);

	template<class T>
	void SwitchGroupStateByClass(const FName InGroupName) { SwitchGroupStateByClass(InGroupName, T::StaticClass()); }

	UFUNCTION(BlueprintCallable)
	void SwitchGroupStateByClass(const FName InGroupName, TSubclassOf<UFiniteStateBase> InStateClass);

	UFUNCTION(BlueprintCallable)
	void SwitchGroupDefaultState(const FName InGroupName);

	UFUNCTION(BlueprintCallable)
	void SwitchGroupFinalState(const FName InGroupName);

	UFUNCTION(BlueprintCallable)
	void SwitchGroupLastState(const FName InGroupName);

	UFUNCTION(BlueprintCallable)
	void SwitchGroupNextState(const FName InGroupName);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FSMGroup")
	TMap<FName, FFSMGroupInfo> GroupMap;

public:
	UFUNCTION(BlueprintPure)
	bool HasGroup(const FName InGroupName) const
	{
		return GroupMap.Contains(InGroupName);
	}

	UFUNCTION(BlueprintPure)
	TArray<UFSMComponent*>& GetGroups(const FName InGroupName)
	{
		if(HasGroup(InGroupName))
		{
			return GroupMap[InGroupName].FSMArray;
		}
		static TArray<UFSMComponent*> FSMArray;
		return FSMArray;
	}
};
