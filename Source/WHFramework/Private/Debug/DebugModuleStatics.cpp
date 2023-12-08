// Fill out your copyright notice in the Description page of Project Settings.


#include "Debug/DebugModuleStatics.h"

#include "Debug/DebugModule.h"
#include "Debug/DebugModuleTypes.h"

bool UDebugModuleStatics::EnsureEditor(bool InExpression)
{
	return ensureEditor(InExpression);
}

bool UDebugModuleStatics::EnsureEditorMsgf(bool InExpression, const FString& InMessage, EDebugCategory InCategory, EDebugVerbosity InVerbosity)
{
	return ensureEditorMsgf(InExpression, InMessage, InCategory, InVerbosity);
}

void UDebugModuleStatics::LogMessage(const FString& InMessage, EDebugCategory InCategory, EDebugVerbosity InVerbosity)
{
	UDebugModule::Get().LogMessage(InMessage, InCategory, InVerbosity);
}

void UDebugModuleStatics::DebugMessage(const FString& InMessage, EDebugMode InMode, EDebugCategory InCategory, EDebugVerbosity InVerbosity, const FLinearColor InDisplayColor, float InDuration, int32 InKey, bool bNewerOnTop)
{
	UDebugModule::Get().DebugMessage(InMessage, InMode, InCategory, InVerbosity, InDisplayColor, InDuration, InKey, bNewerOnTop);
}

FDebugCategoryState UDebugModuleStatics::GetDebugCategoryState(EDebugCategory InCategory)
{
	return UDebugModule::Get().GetDebugCategoryState(InCategory);
}

void UDebugModuleStatics::SetDebugCategoryState(EDebugCategory InCategory, FDebugCategoryState InState)
{
	UDebugModule::Get().SetDebugCategoryState(InCategory, InState);
}
