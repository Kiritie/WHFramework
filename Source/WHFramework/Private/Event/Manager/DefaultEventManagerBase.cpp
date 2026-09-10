#include "Event/Manager/DefaultEventManagerBase.h"

#include "Event/EventModuleStatics.h"

UDefaultEventManagerBase::UDefaultEventManagerBase()
{
	EventManagerName = FName("Default");
}

void UDefaultEventManagerBase::OnInitialize()
{
	Super::OnInitialize();
	UEventModuleStatics::SubscribeEvent<FEventGameInited>(this, &ThisClass::OnGameInited);
	UEventModuleStatics::SubscribeEvent<FEventGameStarted>(this, &ThisClass::OnGameStarted);
	UEventModuleStatics::SubscribeEvent<FEventGameExited>(this, &ThisClass::OnGameExited);
}

void UDefaultEventManagerBase::OnPreparatory()
{
	Super::OnPreparatory();
}

void UDefaultEventManagerBase::OnRefresh(float DeltaSeconds)
{
	Super::OnRefresh(DeltaSeconds);
}

void UDefaultEventManagerBase::OnTermination(EPhase InPhase)
{
	if(PHASEC(InPhase, EPhase::Final))
	{
		UEventModuleStatics::UnsubscribeEvent<FEventGameInited>(this);
		UEventModuleStatics::UnsubscribeEvent<FEventGameStarted>(this);
		UEventModuleStatics::UnsubscribeEvent<FEventGameExited>(this);
	}
	Super::OnTermination(InPhase);
}

void UDefaultEventManagerBase::OnGameInited_Implementation(UObject* InSender, const FEventGameInited& InEvent)
{
}

void UDefaultEventManagerBase::OnGameExited_Implementation(UObject* InSender, const FEventGameExited& InEvent)
{
}

void UDefaultEventManagerBase::OnGameStarted_Implementation(UObject* InSender, const FEventGameStarted& InEvent)
{
}
