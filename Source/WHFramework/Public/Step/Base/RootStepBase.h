// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "Step/Base/StepBase.h"
#include "RootStepBase.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API URootStepBase : public UStepBase
{
	GENERATED_BODY()

public:
	URootStepBase();

	//////////////////////////////////////////////////////////////////////////
	/// Step
public:
#if WITH_EDITOR
	virtual void OnGenerate() override;

	virtual void OnUnGenerate() override;
#endif

public:
	virtual void OnInitialize() override;
	
	virtual void OnRestore() override;
	
	virtual void OnEnter(UStepBase* InLastStep) override;
	
	virtual void OnRefresh() override;

	virtual void OnGuide() override;

	virtual void OnExecute() override;

	virtual void OnComplete(EStepExecuteResult InStepExecuteResult) override;
	
	virtual void OnLeave() override;
};
