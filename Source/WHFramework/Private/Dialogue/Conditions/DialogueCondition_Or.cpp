#include "Dialogue/Conditions/DialogueCondition_Or.h"

bool UDialogueCondition_Or::IsConditionMet_Implementation(APlayerController* ConsideringPlayer, AActor* NPCActor)
	{
		for (auto & cond : OrConditions)
		{
			if (cond && cond->IsConditionMet(ConsideringPlayer, NPCActor))
				return true;
		}
		return false;
	}
