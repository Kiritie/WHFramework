// Fill out your copyright notice in the Description page of Project Settings.


#include "Procedure/ProcedureModuleStatics.h"

#include "Procedure/Base/ProcedureBase.h"

TArray<UProcedureAsset*> UProcedureModuleStatics::GetProcedureAssets()
{
	return UProcedureModule::Get().GetAssets();
}

UProcedureAsset* UProcedureModuleStatics::GetCurrentProcedureAsset()
{
	return UProcedureModule::Get().GetCurrentAsset();
}

UProcedureBase* UProcedureModuleStatics::GetCurrentProcedure(TSubclassOf<UProcedureBase> InClass)
{
	return GetDeterminesOutputObject(UProcedureModule::Get().GetCurrentProcedure(), InClass);
}

void UProcedureModuleStatics::AddProcedureAsset(UProcedureAsset* InAsset)
{
	UProcedureModule::Get().AddAsset(InAsset);
}

void UProcedureModuleStatics::RemoveProcedureAsset(UProcedureAsset* InAsset)
{
	UProcedureModule::Get().RemoveAsset(InAsset);
}

void UProcedureModuleStatics::SwitchProcedureAsset(UProcedureAsset* InAsset)
{
	UProcedureModule::Get().SwitchAsset(InAsset);
}

bool UProcedureModuleStatics::HasProcedureByIndex(int32 InIndex)
{
	return UProcedureModule::Get().HasProcedureByIndex(InIndex);
}

UProcedureBase* UProcedureModuleStatics::GetProcedureByIndex(int32 InIndex, TSubclassOf<UProcedureBase> InClass)
{
	return UProcedureModule::Get().GetProcedureByIndex(InIndex, InClass);
}

bool UProcedureModuleStatics::HasProcedureByClass(TSubclassOf<UProcedureBase> InClass)
{
	return UProcedureModule::Get().HasProcedureByClass(InClass);
}

UProcedureBase* UProcedureModuleStatics::GetProcedureByClass(TSubclassOf<UProcedureBase> InClass)
{
	return UProcedureModule::Get().GetProcedureByClass(InClass);
}

bool UProcedureModuleStatics::IsCurrentProcedure(UProcedureBase* InProcedure)
{
	return UProcedureModule::Get().IsCurrentProcedure(InProcedure);
}

bool UProcedureModuleStatics::IsCurrentProcedureIndex(int32 InIndex)
{
	return UProcedureModule::Get().IsCurrentProcedureIndex(InIndex);
}

void UProcedureModuleStatics::SwitchProcedure(UProcedureBase* InProcedure)
{
	UProcedureModule::Get().SwitchProcedure(InProcedure);
}

void UProcedureModuleStatics::SwitchProcedureByIndex(int32 InIndex)
{
	UProcedureModule::Get().SwitchProcedureByIndex(InIndex);
}

void UProcedureModuleStatics::SwitchProcedureByClass(TSubclassOf<UProcedureBase> InClass)
{
	UProcedureModule::Get().SwitchProcedureByClass(InClass);
}

void UProcedureModuleStatics::SwitchFirstProcedure()
{
	UProcedureModule::Get().SwitchFirstProcedure();
}

void UProcedureModuleStatics::SwitchLastProcedure()
{
	UProcedureModule::Get().SwitchLastProcedure();
}

void UProcedureModuleStatics::SwitchNextProcedure()
{
	UProcedureModule::Get().SwitchNextProcedure();
}

void UProcedureModuleStatics::GuideCurrentProcedure()
{
	UProcedureModule::Get().GuideCurrentProcedure();
}
