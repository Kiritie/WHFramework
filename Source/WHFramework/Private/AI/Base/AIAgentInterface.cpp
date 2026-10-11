#include "AI/Base/AIAgentInterface.h"
#include "AI/Base/AIControllerBase.h"

UAIBlackboardContext* IAIAgentInterface::GetBlackboard() const
{
	AAIControllerBase* Controller = GetAIController();
	return Controller ? Controller->GetBlackboard() : nullptr;
}
