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
class WHFRAMEWORK_API AFSMModule : public AModuleBase
{
	GENERATED_BODY()
		
	GENERATED_MODULE(AFSMModule)

public:
	// ParamSets default values for this actor's properties
	AFSMModule();

	~AFSMModule();

	//////////////////////////////////////////////////////////////////////////
	/// Module
public:
	#if WITH_EDITOR
	virtual void OnGenerate() override;

	virtual void OnDestroy() override;
	#endif

	virtual void OnInitialize_Implementation() override;

	virtual void OnPreparatory_Implementation(EPhase InPhase) override;

	virtual void OnRefresh_Implementation(float DeltaSeconds) override;

	virtual void OnPause_Implementation() override;

	virtual void OnUnPause_Implementation() override;

	virtual void OnTermination_Implementation(EPhase InPhase) override;

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
	void SwitchFSMGroupStateByIndex(const FName InFSMGroup, int32 InStateIndex);

	template<class T>
	void SwitchFSMGroupStateByClass(const FName InFSMGroup) { SwitchFSMGroupStateByClass(T::StaticClass()); }

	UFUNCTION(BlueprintCallable)
	void SwitchFSMGroupStateByClass(const FName InFSMGroup, TSubclassOf<UFiniteStateBase> InStateClass);

	UFUNCTION(BlueprintCallable)
	void SwitchFSMGroupDefaultState(const FName InFSMGroup);

	UFUNCTION(BlueprintCallable)
	void SwitchFSMGroupFinalState(const FName InFSMGroup);

	UFUNCTION(BlueprintCallable)
	void SwitchFSMGroupLastState(const FName InFSMGroup);

	UFUNCTION(BlueprintCallable)
	void SwitchFSMGroupNextState(const FName InFSMGroup);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FSMGroup")
	TMap<FName, FFSMGroupInfo> FSMGroupMap;

public:
	UFUNCTION(BlueprintPure)
	bool HasFSMGroup(const FName InFSMGroup) const
	{
		return FSMGroupMap.Contains(InFSMGroup);
	}

	UFUNCTION(BlueprintPure)
	TArray<UFSMComponent*>& GetFSMGroupArray(const FName InFSMGroup)
	{
		if(HasFSMGroup(InFSMGroup))
		{
			return FSMGroupMap[InFSMGroup].FSMArray;
		}
		static TArray<UFSMComponent*> FSMArray;
		return FSMArray;
	}
};
