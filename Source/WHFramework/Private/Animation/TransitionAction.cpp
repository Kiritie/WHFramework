#include "Animation/TransitionAction.h"

void FTransitionAction::UpdateOperation(FLatentResponse& Response)
{
	Remaining -= Response.ElapsedTime();
	Response.FinishAndTriggerIf(!Owner.IsValid() || Remaining <= 0.f, ExecutionFunction, Link, Target);
}
