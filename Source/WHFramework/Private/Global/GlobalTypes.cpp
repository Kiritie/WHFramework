// Fill out your copyright notice in the Description page of Project Settings.


#include "Global/GlobalTypes.h"

#include "Global/GlobalBPLibrary.h"

#if WITH_EDITOR
bool GIsPlaying = false;
#else
bool GIsPlaying = true;
#endif

bool GIsSimulating = false;

UObject* GetWorldContext(bool bInEditor)
{
	return UGlobalBPLibrary::GetWorldContext(bInEditor);
}
