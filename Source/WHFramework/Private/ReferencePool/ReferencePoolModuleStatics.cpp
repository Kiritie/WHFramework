// Fill out your copyright notice in the Description page of Project Settings.

#include "ReferencePool/ReferencePoolModuleStatics.h"

bool UReferencePoolModuleStatics::HasReference(const UObject* WorldContextObject, TSubclassOf<UObject> InType)
{
	UReferencePoolModule* Module = UReferencePoolModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr);
	return Module ? Module->HasReference(InType.Get()) : false;
}

UObject* UReferencePoolModuleStatics::GetReference(const UObject* WorldContextObject, TSubclassOf<UObject> InType)
{
	UReferencePoolModule* Module = UReferencePoolModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr);
	return Module ? Module->GetReference(InType.Get()) : nullptr;
}

bool UReferencePoolModuleStatics::ResetReference(const UObject* WorldContextObject, TSubclassOf<UObject> InType)
{
	UReferencePoolModule* Module = UReferencePoolModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr);
	return Module ? Module->ResetReference(InType.Get()) : false;
}

bool UReferencePoolModuleStatics::ClearReference(const UObject* WorldContextObject, TSubclassOf<UObject> InType)
{
	UReferencePoolModule* Module = UReferencePoolModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr);
	return Module ? Module->ClearReference(InType.Get()) : false;
}

void UReferencePoolModuleStatics::ClearAllReference(const UObject* WorldContextObject)
{
	if(UReferencePoolModule* Module = UReferencePoolModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr))
	{
		Module->ClearAllReference();
	}
}

UObject* UReferencePoolModuleStatics::GetDefaultReference(TSubclassOf<UObject> InType)
{
	return InType ? InType->GetDefaultObject() : nullptr;
}
