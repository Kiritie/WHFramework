// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "Procedure/Base/ProcedureBase.h"
#include "RootProcedureBase.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API ARootProcedureBase : public AProcedureBase
{
	GENERATED_BODY()

public:
	ARootProcedureBase();

	//////////////////////////////////////////////////////////////////////////
	/// Procedure
public:
#if WITH_EDITOR
	virtual void OnGenerate() override;

	virtual void OnUnGenerate() override;
#endif

public:
	virtual void ServerOnInitialize_Implementation() override;
	
	virtual void ServerOnPrepare_Implementation() override;
	
	virtual void ServerOnEnter_Implementation(AProcedureBase* InLastProcedure) override;
	
	virtual void ServerOnRefresh_Implementation(float DeltaSeconds) override;

	virtual void ServerOnComplete_Implementation(EProcedureExecuteResult InProcedureExecuteResult) override;
	
	virtual void ServerOnLeave_Implementation(AProcedureBase* InNextProcedure) override;
	
	virtual void ServerOnClear_Implementation() override;
};
