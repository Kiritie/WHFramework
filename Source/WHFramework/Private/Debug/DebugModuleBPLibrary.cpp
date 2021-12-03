// Fill out your copyright notice in the Description page of Project Settings.


#include "Debug/DebugModuleBPLibrary.h"

#include "Main/MainModule.h"
#include "Main/MainModuleBPLibrary.h"
#include "Debug/DebugModule.h"
#include "Debug/DebugModuleTypes.h"

void UDebugModuleBPLibrary::EnsureCrash(const FString& Message, bool bNoCrash)
{
	ensureMsgf(bNoCrash, TEXT("%s"), *Message);
}

void UDebugModuleBPLibrary::EnsureEditorCrash(const FString& Message, bool bNoCrash)
{
#if WITH_EDITOR
	ensureEditorMsgf(bNoCrash, TEXT("%s"), *Message);
#else
	if(!bNoCrash)
	{
		WH_LOG(LogTemp, Error, TEXT("Ensure Editor Crash : %s"), *Message);
	}
#endif
}
