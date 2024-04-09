// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "DebugTypes.h"
#include "WHFrameworkCoreTypes.h"
#include "Main/MainTypes.h"
#include "Main/Base/ManagerBase.h"

class WHFRAMEWORKCORE_API FDebugManager : public FManagerBase
{
	GENERATED_MANAGER(FDebugManager)

public:
	// ParamSets default values for this actor's properties
	FDebugManager();

	virtual ~FDebugManager() override;
	
	static const FUniqueType Type;

public:
	UFUNCTION(BlueprintCallable)
	void LogMessage(const FString& InMessage, EDebugCategory InCategory = EDC_Default, EDebugVerbosity InVerbosity = EDV_Log);

	UFUNCTION(BlueprintCallable)
	void DebugMessage(const FString& InMessage, EDebugMode InMode = EDM_Screen, EDebugCategory InCategory = EDC_Default, EDebugVerbosity InVerbosity = EDV_Log, const FLinearColor InDisplayColor = FLinearColor(0,255,255), float InDuration = 1.5f, int InKey = -1, bool bNewerOnTop = true);

public:
	UFUNCTION(BlueprintPure)
	FDebugCategoryState GetDebugCategoryState(EDebugCategory InCategory) const;

	UFUNCTION(BlueprintCallable)
	void SetDebugCategoryState(EDebugCategory InCategory, FDebugCategoryState InState);

protected:
	TMap<TEnumAsByte<EDebugCategory>, FDebugCategoryState> _DebugCategoryStates;
};
