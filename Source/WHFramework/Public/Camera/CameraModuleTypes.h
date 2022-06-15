// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

UENUM(BlueprintType)
enum class ECameraCollisionMode : uint8
{
	None,
	All,
	PhysicsOnly,
	SightOnly
};

UENUM(BlueprintType)
enum class ETrackTargetMode : uint8
{
	LocationOnly,
	LocationAndRotation,
	LocationAndRotationAndDistance,
	LocationAndRotationAndDistanceOnce
};

UENUM(BlueprintType)
enum class ETrackTargetSpace : uint8
{
	Local,
	World
};
