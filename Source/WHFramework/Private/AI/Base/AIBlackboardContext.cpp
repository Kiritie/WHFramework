#include "AI/Base/AIBlackboardContext.h"

#include "AI/Base/AIBlackboardBase.h"
#include "AI/Base/AIControllerBase.h"
#include "GameFramework/Pawn.h"

UAIBlackboardContext::UAIBlackboardContext()
{
	Component = nullptr;
}

void UAIBlackboardContext::Initialize(const UAIBlackboardBase* InDefinition, AAIControllerBase* InController, UBlackboardComponent* InComponent)
{
	Uninitialize();
	Definition = const_cast<UAIBlackboardBase*>(InDefinition);
	Controller = InController;
	Component = InComponent;
	Agent = InController ? InController->GetPawn() : nullptr;
	Reset();
}

void UAIBlackboardContext::Uninitialize()
{
	OnBlackboardValuePreChange.Clear();
	OnBlackboardValueChanged.Clear();
	Component = nullptr;
	Controller.Reset();
	Agent.Reset();
	Definition.Reset();
}

void UAIBlackboardContext::Reset()
{
	if (Component)
	{
		OnReset();
	}
}

void UAIBlackboardContext::ResetValue(FName InValueName)
{
	if (Component && Component->GetKeyID(InValueName) != FBlackboard::InvalidKey)
	{
		OnValueReset(InValueName);
	}
}

void UAIBlackboardContext::Refresh()
{
	if (Component && Agent.IsValid())
	{
		OnRefresh();
	}
}

void UAIBlackboardContext::OnReset()
{
	ResetIsLostTarget();
	ResetTargetLocation();
	ResetTargetAgent();
}

void UAIBlackboardContext::OnRefresh()
{
	if (GetTargetAgent() && !IsValid(GetTargetAgent()))
	{
		ResetTargetAgent();
		ResetTargetLocation();
		ResetIsLostTarget();
	}
}

void UAIBlackboardContext::OnValueReset(FName InValueName)
{
	if (InValueName == NAME_IsLostTarget)
	{
		SetIsLostTarget(false);
	}
	else if (InValueName == NAME_TargetLocation)
	{
		UBlackboardComponent* WriteComponent = Component;
		OnValuePreChange(InValueName);
		if (Component != WriteComponent)
		{
			return;
		}
		Component->ClearValue(InValueName);
		OnValueChanged(InValueName);
	}
	else if (InValueName == NAME_TargetAgent)
	{
		SetTargetAgent(nullptr);
	}
	else
	{
		UBlackboardComponent* WriteComponent = Component;
		OnValuePreChange(InValueName);
		if (Component != WriteComponent)
		{
			return;
		}
		Component->ClearValue(InValueName);
		OnValueChanged(InValueName);
	}
}

void UAIBlackboardContext::OnValuePreChange(FName InValueName)
{
	OnBlackboardValuePreChange.Broadcast(InValueName);
}

void UAIBlackboardContext::OnValueChanged(FName InValueName)
{
	OnBlackboardValueChanged.Broadcast(InValueName);
}

APawn* UAIBlackboardContext::GetAgent(TSubclassOf<APawn> InClass) const
{
	APawn* Pawn = Agent.Get();
	return Pawn && (!InClass || Pawn->IsA(InClass)) ? Pawn : nullptr;
}
