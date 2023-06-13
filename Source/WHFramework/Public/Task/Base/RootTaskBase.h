// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "Task/Base/TaskBase.h"
#include "RootTaskBase.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API URootTaskBase : public UTaskBase
{
	GENERATED_BODY()

public:
	URootTaskBase();

	//////////////////////////////////////////////////////////////////////////
	/// Task
public:
#if WITH_EDITOR
	virtual void OnGenerate() override;

	virtual void OnUnGenerate() override;
#endif

public:
	virtual void OnInitialize() override;
	
	virtual void OnRestore() override;
	
	virtual void OnEnter(UTaskBase* InLastTask) override;
	
	virtual void OnRefresh() override;

	virtual void OnGuide() override;

	virtual void OnExecute() override;

	virtual void OnComplete(ETaskExecuteResult InTaskExecuteResult) override;
	
	virtual void OnLeave() override;
};
