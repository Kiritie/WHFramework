// Fill out your copyright notice in the Description page of Project Settings.

#include "ReferencePool/ReferencePoolModuleStatics.h"

bool UReferencePoolModuleStatics::HasReference(TSubclassOf<UObject> InType)
{
	UReferencePoolModule* Module = UReferencePoolModule::GetPtr();
	return Module ? Module->HasReference(InType.Get()) : false;
}

UObject* UReferencePoolModuleStatics::GetReference(TSubclassOf<UObject> InType)
{
	UReferencePoolModule* Module = UReferencePoolModule::GetPtr();
	return Module ? Module->GetReference(InType.Get()) : nullptr;
}

bool UReferencePoolModuleStatics::ResetReference(TSubclassOf<UObject> InType)
{
	UReferencePoolModule* Module = UReferencePoolModule::GetPtr();
	return Module ? Module->ResetReference(InType.Get()) : false;
}

bool UReferencePoolModuleStatics::ClearReference(TSubclassOf<UObject> InType)
{
	UReferencePoolModule* Module = UReferencePoolModule::GetPtr();
	return Module ? Module->ClearReference(InType.Get()) : false;
}

void UReferencePoolModuleStatics::ClearAllReference()
{
	if(UReferencePoolModule* Module = UReferencePoolModule::GetPtr())
	{
		Module->ClearAllReference();
	}
}

UObject* UReferencePoolModuleStatics::GetDefaultReference(TSubclassOf<UObject> InType)
{
	return InType ? InType->GetDefaultObject() : nullptr;
}
