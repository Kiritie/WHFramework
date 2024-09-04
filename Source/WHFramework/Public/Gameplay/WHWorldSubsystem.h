// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WHWorldSubsystem.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UWHWorldSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	UWHWorldSubsystem();

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

public:
	virtual TStatId GetStatId() const override { return TStatId(); }

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override { return true; }

	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override { return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;  }
};
