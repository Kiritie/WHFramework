// Fill out your copyright notice in the Description page of Project Settings.


#include "Debug/DebugModuleStatics.h"

#include "Debug/DebugManager.h"
#include "Debug/DebugModule.h"

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
	FDebugManager::Get().LogMessage(InMessage, InCategory, InVerbosity);
}

void UDebugModuleStatics::DebugMessage(const FString& InMessage, EDebugMode InMode, EDebugCategory InCategory, EDebugVerbosity InVerbosity, const FLinearColor InDisplayColor, float InDuration, int32 InKey, bool bNewerOnTop)
{
	FDebugManager::Get().DebugMessage(InMessage, InMode, InCategory, InVerbosity, InDisplayColor, InDuration, InKey, bNewerOnTop);
}

FDebugCategoryState UDebugModuleStatics::GetDebugCategoryState(UWorld* InWorld, EDebugCategory InCategory)
{
	const UDebugModule* Module = UDebugModule::GetPtr(InWorld);
	return Module ? Module->GetDebugCategoryState(InCategory) : FDebugCategoryState();
}

void UDebugModuleStatics::SetDebugCategoryState(UWorld* InWorld, EDebugCategory InCategory, FDebugCategoryState InState)
{
	if (UDebugModule* Module = UDebugModule::GetPtr(InWorld)) Module->SetDebugCategoryState(InCategory, InState);
}
