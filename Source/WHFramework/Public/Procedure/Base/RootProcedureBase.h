// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "Procedure/Base/ProcedureBase.h"
#include "RootProcedureBase.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API URootProcedureBase : public UProcedureBase
{
	GENERATED_BODY()

public:
	URootProcedureBase();

	//////////////////////////////////////////////////////////////////////////
	/// Procedure
public:
#if WITH_EDITOR
	virtual void OnGenerate() override;

	virtual void OnUnGenerate() override;
#endif

public:
	virtual void OnInitialize_Implementation() override;
	
	virtual void OnRestore_Implementation() override;
	
	virtual void OnEnter_Implementation(UProcedureBase* InLastProcedure) override;
	
	virtual void OnRefresh_Implementation(float DeltaSeconds) override;

	virtual void OnGuide_Implementation() override;

	virtual void OnExecute_Implementation() override;

	virtual void OnComplete_Implementation(EProcedureExecuteResult InProcedureExecuteResult) override;
	
	virtual void OnLeave_Implementation() override;
};
