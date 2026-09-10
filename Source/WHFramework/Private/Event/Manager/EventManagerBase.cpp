#include "Event/Manager/EventManagerBase.h"

UEventManagerBase::UEventManagerBase()
{
	EventManagerName = NAME_None;
}

void UEventManagerBase::OnInitialize()
{
	K2_OnInitialize();
}

void UEventManagerBase::OnPreparatory()
{
	K2_OnPreparatory();
}

void UEventManagerBase::OnRefresh(float DeltaSeconds)
{
	K2_OnRefresh(DeltaSeconds);
}

void UEventManagerBase::OnTermination(EPhase InPhase)
{
	K2_OnTermination(InPhase);
}
