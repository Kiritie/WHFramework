// Fill out your copyright notice in the Description page of Project Settings.


#include "Common/CommonTypes.h"

#include "Common/CommonBPLibrary.h"

#if WITH_EDITOR
bool GIsPlaying = false;
#else
bool GIsPlaying = true;
#endif

bool GIsSimulating = false;

UObject* GetWorldContext(bool bInEditor)
{
	return UCommonBPLibrary::GetWorldContext(bInEditor);
}
