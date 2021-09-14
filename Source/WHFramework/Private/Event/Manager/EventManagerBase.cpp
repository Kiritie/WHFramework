// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Manager/EventManagerBase.h"

#include "Event/EventModuleBPLibrary.h"

// ParamSets default values
AEventManagerBase::AEventManagerBase()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;

	RootComponent = CreateDefaultSubobject<USceneComponent>(FName("RootComponent"));
}

// Called when the game starts or when spawned
void AEventManagerBase::BeginPlay()
{
	Super::BeginPlay();
}
