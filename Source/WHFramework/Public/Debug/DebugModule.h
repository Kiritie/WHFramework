// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Main/Base/ModuleBase.h"
#include "DebugModuleStatics.h"
#include "Debug/DebugManager.h"

#include "DebugModule.generated.h"

UCLASS()
class WHFRAMEWORK_API UDebugModule : public UModuleBase, public FDebugManager
{
	GENERATED_BODY()
		
	GENERATED_MODULE(UDebugModule)

public:
	// ParamSets default values for this actor's properties
	UDebugModule();

	~UDebugModule();

	//////////////////////////////////////////////////////////////////////////
	/// Module
public:
#if WITH_EDITOR
	virtual void OnGenerate() override;

	virtual void OnDestroy() override;
#endif

	virtual void OnInitialize() override;

	virtual void OnPreparatory(EPhase InPhase) override;

	virtual void OnRefresh(float DeltaSeconds) override;

	virtual void OnPause() override;

	virtual void OnUnPause() override;

	virtual void OnTermination(EPhase InPhase) override;

protected:
	UPROPERTY(EditAnywhere)
	TMap<TEnumAsByte<EDebugCategory>, FDebugCategoryState> DebugCategoryStates;
};
