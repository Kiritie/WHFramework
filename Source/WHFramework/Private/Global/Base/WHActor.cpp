// Fill out your copyright notice in the Description page of Project Settings.

#include "Global/Base/WHActor.h"

AWHActor::AWHActor()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(FName("RootComponent"));
}
