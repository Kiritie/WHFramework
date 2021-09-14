// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "EventManagerBase.generated.h"

UCLASS()
class WHFRAMEWORK_API AEventManagerBase : public AActor
{
	GENERATED_BODY()
	
public:	
	// ParamSets default values for this actor's properties
	AEventManagerBase();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
};
