// Fill out your copyright notice in the Description page of Project Settings.


#include "Scene/SceneModuleTypes.h"

ECollisionChannel FTraceMapping::GetTraceChannel() const
{
	return TraceChannel;
}

ETraceTypeQuery FTraceMapping::GetTraceType() const
{
	return UEngineTypes::ConvertToTraceType(TraceChannel);
}
