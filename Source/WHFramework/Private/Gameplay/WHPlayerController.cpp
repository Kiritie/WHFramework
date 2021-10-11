// Fill out your copyright notice in the Description page of Project Settings.

#include "Gameplay/WHPlayerController.h"

#include "Net/UnrealNetwork.h"
#include "Audio/AudioModuleNetworkComponent.h"
#include "Character/CharacterModuleNetworkComponent.h"
#include "Event/EventModuleNetworkComponent.h"
#include "Network/NetworkModuleNetworkComponent.h"
#include "Procedure/ProcedureModule.h"
#include "Procedure/ProcedureModuleNetworkComponent.h"

AWHPlayerController::AWHPlayerController()
{
	AudioModuleNetworkComp = CreateDefaultSubobject<UAudioModuleNetworkComponent>(TEXT("AudioModuleNetworkComp"));;
	
	CharacterModuleNetworkComp = CreateDefaultSubobject<UCharacterModuleNetworkComponent>(TEXT("CharacterModuleNetworkComp"));;
	
	ProcedureModuleNetworkComp = CreateDefaultSubobject<UProcedureModuleNetworkComponent>(TEXT("ProcedureModuleNetworkComp"));;
	
	NetworkModuleNetworkComp = CreateDefaultSubobject<UNetworkModuleNetworkComponent>(TEXT("NetworkModuleNetworkComp"));;
	
	EventModuleNetworkComp = CreateDefaultSubobject<UEventModuleNetworkComponent>(TEXT("EventModuleNetworkComp"));;
}