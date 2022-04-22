// Copyright Epic Games, Inc. All Rights Reserved.

#include "Procedure/Base/ProcedureBlueprint.h"

//////////////////////////////////////////////////////////////////////////
// UProcedureBlueprint

UProcedureBlueprint::UProcedureBlueprint(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

#if WITH_EDITOR

/** Returns the most base gameplay ability blueprint for a given blueprint (if it is inherited from another ability blueprint, returning null if only native / non-ability BP classes are it's parent) */
UProcedureBlueprint* UProcedureBlueprint::FindRootProcedureBlueprint(UProcedureBlueprint* DerivedBlueprint)
{
	UProcedureBlueprint* ParentBP = NULL;

	// Determine if there is a gameplay ability blueprint in the ancestry of this class
	// for (UClass* ParentClass = DerivedBlueprint->ParentClass; ParentClass != UObject::StaticClass(); ParentClass = ParentClass->GetSuperClass())
	// {
	// 	if (UProcedureBlueprint* TestBP = Cast<UProcedureBlueprint>(ParentClass->ClassGeneratedBy))
	// 	{
	// 		ParentBP = TestBP;
	// 	}
	// }

	return ParentBP;
}

#endif
