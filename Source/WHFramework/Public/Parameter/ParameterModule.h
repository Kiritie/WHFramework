// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "Main/Base/ModuleBase.h"
#include "Parameter/ParameterManagerInterface.h"
#include "ParameterModule.generated.h"

class ATargetPoint;

UCLASS()
class WHFRAMEWORK_API UParameterModule : public UModuleBase, public IParameterManagerInterface
{
	GENERATED_BODY()
				
	GENERATED_MODULE(UParameterModule)

public:	
	// ParamSets default values for this actor's properties
	UParameterModule();

	~UParameterModule();

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

protected:
	virtual void LoadData(FSaveData* InSaveData, EPhase InPhase) override;

	virtual FSaveData* ToData() override;

public:
	virtual void Load_Implementation() override;

public:
	virtual FString GetModuleDebugMessage() override;

	//////////////////////////////////////////////////////////////////////////
	/// Parameter
protected:
	UPROPERTY(EditAnywhere, Replicated)
	FParameters Parameters;

public:
	UFUNCTION(BlueprintCallable, BlueprintPure)
	virtual bool HasParameter(FName InName, bool bEnsured = true) const override;

	UFUNCTION(BlueprintCallable)
	virtual void SetParameter(FName InName, FParameter InParameter) override;

	UFUNCTION(BlueprintCallable, BlueprintPure)
	virtual FParameter GetParameter(FName InName, bool bEnsured = true) const override;

	UFUNCTION(BlueprintCallable, BlueprintPure)
	virtual TArray<FParameter> GetParameters(FName InName, bool bEnsured = true) const override;
	
	UFUNCTION(BlueprintCallable)
	virtual void RemoveParameter(FName InName) override;
	
	UFUNCTION(BlueprintCallable)
	virtual void RemoveParameters(FName InName) override;

	UFUNCTION(BlueprintCallable, BlueprintPure)
	virtual TArray<FParameterSet> GetAllParameter() override;

	UFUNCTION(BlueprintCallable)
	virtual void ClearAllParameter() override;

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
