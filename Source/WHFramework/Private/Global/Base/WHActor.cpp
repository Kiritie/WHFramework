// Fill out your copyright notice in the Description page of Project Settings.

#include "Global/Base/WHActor.h"

#include "Scene/SceneModuleBPLibrary.h"
#include "Scene/Container/SceneContainerInterface.h"

AWHActor::AWHActor()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(FName("RootComponent"));

	ActorID = FGuid::NewGuid();
	bVisible = true;
	Container = nullptr;
}

void AWHActor::OnSpawn_Implementation(const TArray<FParameter>& InParams)
{
	Execute_SetActorVisible(this, true);
	
	if(Execute_IsAddToList(this))
	{
		USceneModuleBPLibrary::AddSceneActor(this);
	}
}

void AWHActor::OnDespawn_Implementation()
{
	Execute_SetActorVisible(this, false);
	
	if(Execute_IsAddToList(this))
	{
		USceneModuleBPLibrary::RemoveSceneActor(this);
	}
	else if(Container)
	{
		Container->RemoveSceneActor(this);
	}

	Container = nullptr;
}

void AWHActor::SetActorVisible_Implementation(bool bNewVisible)
{
	bVisible = bNewVisible;
	GetRootComponent()->SetVisibility(bNewVisible, true);
	TArray<AActor*> AttachedActors;
	GetAttachedActors(AttachedActors);
	for(auto Iter : AttachedActors)
	{
		ISceneActorInterface::Execute_SetActorVisible(Iter, bNewVisible);
	}
}
