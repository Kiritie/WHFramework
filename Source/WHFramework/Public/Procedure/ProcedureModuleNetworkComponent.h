// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "Base/ModuleNetworkComponent.h"

#include "ProcedureModuleNetworkComponent.generated.h"

/**
* 
*/
UCLASS()
class WHFRAMEWORK_API UProcedureModuleNetworkComponent : public UModuleNetworkComponent
{
	GENERATED_BODY()

public:
	UProcedureModuleNetworkComponent();

public:
	UFUNCTION(BlueprintCallable, Server, WithValidation, Reliable)
	void ServerEnterProcedure(class AProcedureBase* InProcedure);

	UFUNCTION(BlueprintCallable, Server, WithValidation, Reliable)
	void ServerLeaveProcedure(class AProcedureBase* InProcedure);

	UFUNCTION(BlueprintCallable, Server, WithValidation, Reliable)
	void ServerSkipProcedure(class AProcedureBase* InProcedure);
};
