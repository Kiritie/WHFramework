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
	virtual void OnInitialize() override;
	
	virtual void OnRestore() override;
	
	virtual void OnEnter(UProcedureBase* InLastProcedure) override;
	
	virtual void OnRefresh() override;

	virtual void OnGuide() override;

	virtual void OnExecute() override;

	virtual void OnComplete(EProcedureExecuteResult InProcedureExecuteResult) override;
	
	virtual void OnLeave() override;
};
