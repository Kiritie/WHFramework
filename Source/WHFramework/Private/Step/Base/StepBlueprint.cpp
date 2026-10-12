#include "Step/Base/StepBlueprint.h"

UStepBlueprint::UStepBlueprint(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

#if WITH_EDITOR
UStepBlueprint* UStepBlueprint::FindRootStepBlueprint(UStepBlueprint* DerivedBlueprint)
{
	if(!DerivedBlueprint)
	{
		return nullptr;
	}
	UStepBlueprint* Root = DerivedBlueprint;
	for(UClass* Parent = DerivedBlueprint->ParentClass; Parent; Parent = Parent->GetSuperClass())
	{
		if(UStepBlueprint* Blueprint = Cast<UStepBlueprint>(Parent->ClassGeneratedBy))
		{
			Root = Blueprint;
		}
	}
	return Root;
}
#endif
