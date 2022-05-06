// Copyright Epic Games, Inc. All Rights Reserved.

#include "Step/Base/StepBlueprint.h"

//////////////////////////////////////////////////////////////////////////
// UStepBlueprint

UStepBlueprint::UStepBlueprint(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

#if WITH_EDITOR

/** Returns the most base gameplay ability blueprint for a given blueprint (if it is inherited from another ability blueprint, returning null if only native / non-ability BP classes are it's parent) */
UStepBlueprint* UStepBlueprint::FindRootStepBlueprint(UStepBlueprint* DerivedBlueprint)
{
	UStepBlueprint* ParentBP = NULL;

	// Determine if there is a gameplay ability blueprint in the ancestry of this class
	// for (UClass* ParentClass = DerivedBlueprint->ParentClass; ParentClass != UObject::StaticClass(); ParentClass = ParentClass->GetSuperClass())
	// {
	// 	if (UStepBlueprint* TestBP = Cast<UStepBlueprint>(ParentClass->ClassGeneratedBy))
	// 	{
	// 		ParentBP = TestBP;
	// 	}
	// }

	return ParentBP;
}

#endif
