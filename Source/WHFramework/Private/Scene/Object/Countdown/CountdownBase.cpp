// Fill out your copyright notice in the Description page of Project Settings.


#include "Scene/Object/Countdown/CountdownBase.h"

#include "TimerManager.h"
#include "Engine/TargetPoint.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "Scene/Object/Countdown/CountdownTypes.h"

void FCountdownHelper::StartCountdown(const UObject* WorldContext)
{
	if (UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr)
	{
		FActorSpawnParameters ActorSpawnParameters;
		ActorSpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Countdown = World->SpawnActor<ACountdownBase>(CountdownClass, CountdownSpawnPosition->GetActorTransform(), ActorSpawnParameters);
		if(Countdown)
		{
			Countdown->ServerSetTotalCountdownTime(CountdownTime);
		}

		World->GetTimerManager().SetTimer(CountdownTimer, FTimerDelegate::CreateRaw(this, &FCountdownHelper::OnCountdownFinished), CountdownTime, false);
	}
}

void FCountdownHelper::StopCountdown(const UObject* WorldContext)
{
	if(Countdown)
	{
		Countdown->Destroy();
	}

	if(CountdownTimer.IsValid())
	{
		if (UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr)
		{
			World->GetTimerManager().ClearTimer(CountdownTimer);
		}
	}
}

void FCountdownHelper::OnCountdownFinished()
{
	OnCountdownFinishedDelegate.Broadcast();

	if (Countdown)
	{
		StopCountdown(Countdown);	
	}	
}

bool FCountdownHelper::IsCountdown() const
{
	return Countdown && Countdown->IsValidLowLevel();
}

ACountdownBase::ACountdownBase()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.SetTickFunctionEnable(true);

	bReplicates = true;
}

void ACountdownBase::BeginPlay()
{
	Super::BeginPlay();

	ServerSetTotalCountdownTime(TotalCountdownTime);
}

void ACountdownBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (CountdownTime > 0.f)
	{
		CountdownTime -= DeltaSeconds;
		if (HasAuthority())
		{
			ServerCountdownTime = CountdownTime;
		}
	}
}

void ACountdownBase::ServerSetTotalCountdownTime(float InCountdownTime)
{
	TotalCountdownTime = InCountdownTime;
	OnRep_TotalCountdownTime();
}

void ACountdownBase::OnRep_TotalCountdownTime()
{
	CountdownTime = TotalCountdownTime;
	
	UpdateCountdownDisplayStyle();
}

void ACountdownBase::OnRep_ServerCountdownTime()
{
	float Time = CountdownTime - ServerCountdownTime;
	if (Time > 2.f || Time < -2.f)
	{
		CountdownTime = ServerCountdownTime;
	}

	UpdateCountdownDisplayStyle();
}

void ACountdownBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ACountdownBase, ServerCountdownTime);
	DOREPLIFETIME(ACountdownBase, TotalCountdownTime);
}