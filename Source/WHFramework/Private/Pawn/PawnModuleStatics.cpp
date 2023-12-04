// Fill out your copyright notice in the Description page of Project Settings.


#include "Pawn/PawnModuleStatics.h"

#include "Pawn/PawnModule.h"

APawnBase* UPawnModuleStatics::GetCurrentPawn()
{
	return UPawnModule::Get().GetCurrentPawn();
}

APawnBase* UPawnModuleStatics::GetCurrentPawn(TSubclassOf<APawnBase> InClass)
{
	return UPawnModule::Get().GetCurrentPawn(InClass);
}

void UPawnModuleStatics::SwitchPawn(APawnBase* InPawn, bool bResetCamera, bool bInstant)
{
	UPawnModule::Get().SwitchPawn(InPawn, bResetCamera, bInstant);
}

void UPawnModuleStatics::SwitchPawnByClass(TSubclassOf<APawnBase> InClass, bool bResetCamera, bool bInstant)
{
	UPawnModule::Get().SwitchPawnByClass(InClass, bResetCamera, bInstant);
}

void UPawnModuleStatics::SwitchPawnByName(FName InName, bool bResetCamera, bool bInstant)
{
	UPawnModule::Get().SwitchPawnByName(InName, bResetCamera, bInstant);
}

bool UPawnModuleStatics::HasPawnByClass(TSubclassOf<APawnBase> InClass)
{
	return UPawnModule::Get().HasPawnByClass(InClass);
}

bool UPawnModuleStatics::HasPawnByName(FName InName)
{
	return UPawnModule::Get().HasPawnByName(InName);
}

APawnBase* UPawnModuleStatics::GetPawnByClass(TSubclassOf<APawnBase> InClass)
{
	return UPawnModule::Get().GetPawnByClass(InClass);
}

APawnBase* UPawnModuleStatics::GetPawnByName(FName InName)
{
	return UPawnModule::Get().GetPawnByName(InName);
}

void UPawnModuleStatics::AddPawnToList(APawnBase* InPawn)
{
	UPawnModule::Get().AddPawnToList(InPawn);
}

void UPawnModuleStatics::RemovePawnFromList(APawnBase* InPawn)
{
	UPawnModule::Get().RemovePawnFromList(InPawn);
}
