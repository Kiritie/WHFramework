// Fill out your copyright notice in the Description page of Project Settings.

#include "ReferencePool/ReferencePoolModule.h"

IMPLEMENTATION_MODULE(UReferencePoolModule)

UReferencePoolModule::UReferencePoolModule()
{
	ModuleName = FName("ReferencePoolModule");
	ModuleDisplayName = FText::FromString(TEXT("Reference Pool Module"));
	bModuleRequired = true;
}

UReferencePoolModule::~UReferencePoolModule()
{
	TERMINATION_MODULE(UReferencePoolModule)
}

#if WITH_EDITOR
void UReferencePoolModule::OnGenerate()
{
	Super::OnGenerate();
}

void UReferencePoolModule::OnDestroy()
{
	Super::OnDestroy();
	TERMINATION_MODULE(UReferencePoolModule)
}
#endif

void UReferencePoolModule::OnInitialize()
{
	Super::OnInitialize();
}

void UReferencePoolModule::OnPreparatory(EPhase InPhase)
{
	Super::OnPreparatory(InPhase);
}

void UReferencePoolModule::OnRefresh(float DeltaSeconds, bool bInEditor)
{
	Super::OnRefresh(DeltaSeconds, bInEditor);
}

void UReferencePoolModule::OnPause()
{
	Super::OnPause();
}

void UReferencePoolModule::OnUnPause()
{
	Super::OnUnPause();
}

void UReferencePoolModule::OnTermination(EPhase InPhase)
{
	if(InPhase == EPhase::Final)
	{
		ClearAllReference();
	}

	Super::OnTermination(InPhase);
}

bool UReferencePoolModule::IsValidReferenceType(UClass* InType) const
{
	if(!InType)
	{
		return false;
	}

	if(InType->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
	{
		return false;
	}

	return InType->ImplementsInterface(UReferencePoolInterface::StaticClass());
}

bool UReferencePoolModule::HasReference(UClass* InType) const
{
	if(!InType)
	{
		return false;
	}

	const TSubclassOf<UObject> Type = InType;
	if(const TObjectPtr<UObject>* Reference = References.Find(Type))
	{
		return IsValid(Reference->Get());
	}
	return false;
}

UObject* UReferencePoolModule::GetReference(UClass* InType)
{
	checkf(IsValidReferenceType(InType), TEXT("Reference type %s is invalid or does not implement IReferencePoolInterface."), *GetNameSafe(InType));

	const TSubclassOf<UObject> Type = InType;
	if(TObjectPtr<UObject>* Reference = References.Find(Type))
	{
		if(IsValid(Reference->Get()))
		{
			return Reference->Get();
		}
		References.Remove(Type);
	}

	UObject* Reference = NewObject<UObject>(this, InType);
	checkf(Reference, TEXT("Failed to create reference for class %s."), *GetNameSafe(InType));
	References.Add(Type, Reference);
	return Reference;
}

bool UReferencePoolModule::ResetReference(UClass* InType)
{
	if(!InType || !InType->ImplementsInterface(UReferencePoolInterface::StaticClass()))
	{
		return false;
	}

	const TSubclassOf<UObject> Type = InType;
	TObjectPtr<UObject>* Reference = References.Find(Type);
	if(!Reference)
	{
		return false;
	}

	UObject* Object = Reference->Get();
	if(!IsValid(Object))
	{
		References.Remove(Type);
		return false;
	}

	IReferencePoolInterface::Execute_OnReset(Object);
	return true;
}

bool UReferencePoolModule::ClearReference(UClass* InType)
{
	if(!InType)
	{
		return false;
	}

	const TSubclassOf<UObject> Type = InType;
	return References.Remove(Type) > 0;
}

void UReferencePoolModule::ClearAllReference()
{
	References.Empty();
}

FString UReferencePoolModule::GetModuleDebugMessage()
{
	FString DebugMessage;
	for(const auto& Iter : References)
	{
		UObject* Object = Iter.Value.Get();
		if(!IsValid(Object))
		{
			continue;
		}
		DebugMessage.Appendf(TEXT("%s: %s\n"), *GetNameSafe(Iter.Key.Get()), *GetNameSafe(Object));
	}
	DebugMessage.RemoveFromEnd(TEXT("\n"));
	return !DebugMessage.IsEmpty() ? DebugMessage : Super::GetModuleDebugMessage();
}
