#include "Dialogue/Conditions/DialogueCondition_And.h"

bool UDialogueCondition_And::IsConditionMet_Implementation(APlayerController* ConsideringPlayer, AActor* NPCActor)
	{
		for (auto & cond : AndConditions)
		{
			if (!cond || !cond->IsConditionMet(ConsideringPlayer, NPCActor))
				return false;
		}
		return true;
	}
