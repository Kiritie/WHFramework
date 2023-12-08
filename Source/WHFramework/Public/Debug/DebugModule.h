// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Main/Base/ModuleBase.h"
#include "DebugModuleStatics.h"

#include "DebugModule.generated.h"

UCLASS()
class WHFRAMEWORK_API UDebugModule : public UModuleBase
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

public:
	UFUNCTION(BlueprintCallable)
	virtual void LogMessage(const FString& InMessage, EDebugCategory InCategory = EDC_Default, EDebugVerbosity InVerbosity = EDV_Log);

	UFUNCTION(BlueprintCallable)
	virtual void DebugMessage(const FString& InMessage, EDebugMode InMode = EDM_Screen, EDebugCategory InCategory = EDC_Default, EDebugVerbosity InVerbosity = EDV_Log, const FLinearColor InDisplayColor = FLinearColor(0,255,255), float InDuration = 1.5f, int InKey = -1, bool bNewerOnTop = true);

public:
	UFUNCTION(BlueprintPure)
	FDebugCategoryState GetDebugCategoryState(EDebugCategory InCategory) const;

	UFUNCTION(BlueprintCallable)
	void SetDebugCategoryState(EDebugCategory InCategory, FDebugCategoryState InState);

protected:
	UPROPERTY(EditAnywhere)
	TMap<TEnumAsByte<EDebugCategory>, FDebugCategoryState> DebugCategoryStates;
};
