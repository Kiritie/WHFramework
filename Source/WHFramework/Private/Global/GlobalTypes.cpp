// Fill out your copyright notice in the Description page of Project Settings.


#include "Global/GlobalTypes.h"

#if WITH_EDITOR
bool GIsPlaying = false;
#else
bool GIsPlaying = true;
#endif
bool GIsSimulating = false;

FCameraParams GCopiedCameraData = FCameraParams();