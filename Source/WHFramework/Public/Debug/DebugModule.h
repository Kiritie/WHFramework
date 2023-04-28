// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "WHFramework.h"
#include "Main/Base/ModuleBase.h"
#include "DebugModuleBPLibrary.h"

#include "DebugModule.generated.h"

UCLASS()
class WHFRAMEWORK_API ADebugModule : public AModuleBase
{
	GENERATED_BODY()
		
	GENERATED_MODULE(ADebugModule)

public:
	// ParamSets default values for this actor's properties
	ADebugModule();

	~ADebugModule();

	//////////////////////////////////////////////////////////////////////////
	/// Module
public:
#if WITH_EDITOR
	virtual void OnGenerate_Implementation() override;

	virtual void OnDestroy_Implementation() override;
#endif

	virtual void OnInitialize_Implementation() override;

	virtual void OnPreparatory_Implementation(EPhase InPhase) override;

	virtual void OnRefresh_Implementation(float DeltaSeconds) override;

	virtual void OnPause_Implementation() override;

	virtual void OnUnPause_Implementation() override;

	virtual void OnTermination_Implementation() override;

public:
	UFUNCTION(BlueprintPure)
	FDebugCategoryState GetDebugCategoryState(EDebugCategory InCategory) const;

	UFUNCTION(BlueprintCallable)
	void SetDebugCategoryState(EDebugCategory InCategory, FDebugCategoryState InState);

protected:
	UPROPERTY(EditAnywhere)
	TMap<EDebugCategory, FDebugCategoryState> DebugCategoryStates;
};
