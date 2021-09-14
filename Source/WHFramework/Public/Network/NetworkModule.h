// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Base/ModuleBase.h"
#include "Parameter/ParameterModuleTypes.h"

#include "NetworkModule.generated.h"

UCLASS()
class WHFRAMEWORK_API ANetworkModule : public AModuleBase
{
	GENERATED_BODY()
	
public:	
	// ParamSets default values for this actor's properties
	ANetworkModule();
	
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
	/// Execute Object Func
public:
	bool ExecuteObjectFunc(UObject* InObject, const FName& InFuncName, void* Params);

	//////////////////////////////////////////////////////////////////////////
	/// Execute Actor Func
public:
	UFUNCTION(BlueprintCallable)
	void ServerExecuteActorFunc(AActor* InActor, FName InFuncName);

	UFUNCTION(BlueprintCallable)
	void ServerExecuteActorFuncOneParam(AActor* InActor, FName InFuncName, FParameter InParam);

	UFUNCTION(BlueprintCallable)
	void ServerExecuteActorFuncTwoParam(AActor* InActor, FName InFuncName, FParameter InParam1, FParameter InParam2);

	UFUNCTION(BlueprintCallable)
	void ServerExecuteActorFuncThreeParam(AActor* InActor, FName InFuncName, FParameter InParam1, FParameter InParam2, FParameter InParam3);

	UFUNCTION(BlueprintCallable)
	void ServerExecuteActorFuncFourParam(AActor* InActor, FName InFuncName, FParameter InParam1, FParameter InParam2, FParameter InParam3, FParameter InParam4);

	UFUNCTION(BlueprintCallable)
	void ServerExecuteActorFuncFiveParam(AActor* InActor, FName InFuncName, FParameter InParam1, FParameter InParam2, FParameter InParam3, FParameter InParam4, FParameter InParam5);
};
