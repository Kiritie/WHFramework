// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "PhysicsEngine/PhysicsSettings.h"

#include "PhysicsVolumeTypes.generated.h"

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FPhysicsVolumeData
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Priority;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float FluidFriction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float TerminalVelocity;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bWaterVolume;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bPhysicsOnContact;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<class APhysicsVolumeBase> PhysicsVolumeClass;

public:
	FPhysicsVolumeData()
	{
		Name = NAME_None;
		Priority = 0;
		FluidFriction = UPhysicsSettings::Get()->DefaultFluidFriction;
		TerminalVelocity = UPhysicsSettings::Get()->DefaultTerminalVelocity;
		bWaterVolume = false;
		bPhysicsOnContact = true;
		PhysicsVolumeClass = nullptr;
	}
};