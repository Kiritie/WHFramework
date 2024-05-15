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
	void LogMessage(const FString& InMessage, EDebugCategory InCategory, EDebugVerbosity InVerbosity);
	
	void DebugMessage(const FString& InMessage, EDebugMode InMode, EDebugCategory InCategory, EDebugVerbosity InVerbosity, FLinearColor InDisplayColor, float InDuration, int32 InKey, bool bNewerOnTop);

	FDebugCategoryState GetDebugCategoryState(EDebugCategory InCategory) const;

	void SetDebugCategoryState(EDebugCategory InCategory, FDebugCategoryState InState);

protected:
	TMap<TEnumAsByte<EDebugCategory>, FDebugCategoryState> DebugCategoryStates;
};
