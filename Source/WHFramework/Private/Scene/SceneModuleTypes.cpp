// Fill out your copyright notice in the Description page of Project Settings.


#include "Scene/SceneModuleTypes.h"

namespace SceneTags
{
	UE_DEFINE_GAMEPLAY_TAG(Feature_Town, "World.Feature.Town");
	UE_DEFINE_GAMEPLAY_TAG(Feature_Building, "World.Feature.Building");
}

ECollisionChannel FTraceMapping::GetTraceChannel() const
{
	return TraceChannel;
}

ETraceTypeQuery FTraceMapping::GetTraceType() const
{
	return UEngineTypes::ConvertToTraceType(TraceChannel);
}
