#include "Procedure/ProcedureModuleStatics.h"

bool UProcedureModuleStatics::StartMain(UWorld* InWorld)
{
    if (UProcedureModule* Module = UProcedureModule::GetPtr(InWorld)) return Module->StartMain();
    return {};
}

void UProcedureModuleStatics::StopMain(UWorld* InWorld)
{
    if (UProcedureModule* Module = UProcedureModule::GetPtr(InWorld)) Module->StopMain();
}

bool UProcedureModuleStatics::SwitchProcedure(UWorld* InWorld, UProcedureBase* InProcedure)
{
    if (UProcedureModule* Module = UProcedureModule::GetPtr(InWorld)) return Module->SwitchProcedure(InProcedure);
    return {};
}

bool UProcedureModuleStatics::SwitchProcedureFrom(UWorld* InWorld, const FProcedureActivationToken& Token, UProcedureBase* InProcedure)
{
    if (UProcedureModule* Module = UProcedureModule::GetPtr(InWorld)) return Module->SwitchProcedureFrom(Token, InProcedure);
    return {};
}

bool UProcedureModuleStatics::CanSwitchProcedure(UWorld* InWorld, UProcedureBase* InProcedure)
{
    if (UProcedureModule* Module = UProcedureModule::GetPtr(InWorld)) return Module->CanSwitchProcedure(InProcedure);
    return {};
}

bool UProcedureModuleStatics::SwitchProcedureByIndex(UWorld* InWorld, int32 InIndex)
{
    if (UProcedureModule* Module = UProcedureModule::GetPtr(InWorld)) return Module->SwitchProcedureByIndex(InIndex);
    return {};
}

bool UProcedureModuleStatics::CanSwitchProcedureByIndex(UWorld* InWorld, int32 InIndex)
{
    if (UProcedureModule* Module = UProcedureModule::GetPtr(InWorld)) return Module->CanSwitchProcedureByIndex(InIndex);
    return {};
}

bool UProcedureModuleStatics::SwitchProcedureByClass(UWorld* InWorld, TSubclassOf<UProcedureBase> InClass)
{
    if (UProcedureModule* Module = UProcedureModule::GetPtr(InWorld)) return Module->SwitchProcedureByClass(InClass);
    return {};
}

bool UProcedureModuleStatics::CanSwitchProcedureByClass(UWorld* InWorld, TSubclassOf<UProcedureBase> InClass)
{
    if (UProcedureModule* Module = UProcedureModule::GetPtr(InWorld)) return Module->CanSwitchProcedureByClass(InClass);
    return {};
}

bool UProcedureModuleStatics::SwitchFirstProcedure(UWorld* InWorld)
{
    if (UProcedureModule* Module = UProcedureModule::GetPtr(InWorld)) return Module->SwitchFirstProcedure();
    return {};
}

bool UProcedureModuleStatics::SwitchLastProcedure(UWorld* InWorld)
{
    if (UProcedureModule* Module = UProcedureModule::GetPtr(InWorld)) return Module->SwitchLastProcedure();
    return {};
}

bool UProcedureModuleStatics::SwitchPreProcedure(UWorld* InWorld)
{
    if (UProcedureModule* Module = UProcedureModule::GetPtr(InWorld)) return Module->SwitchPreProcedure();
    return {};
}

bool UProcedureModuleStatics::SwitchNextProcedure(UWorld* InWorld)
{
    if (UProcedureModule* Module = UProcedureModule::GetPtr(InWorld)) return Module->SwitchNextProcedure();
    return {};
}

bool UProcedureModuleStatics::Advance(UWorld* InWorld, FName Output)
{
    if (UProcedureModule* Module = UProcedureModule::GetPtr(InWorld)) return Module->Advance(Output);
    return {};
}

bool UProcedureModuleStatics::CanAdvance(UWorld* InWorld, FName Output)
{
    if (UProcedureModule* Module = UProcedureModule::GetPtr(InWorld)) return Module->CanAdvance(Output);
    return {};
}

bool UProcedureModuleStatics::AdvanceFrom(UWorld* InWorld, const FProcedureActivationToken& Token, FName Output)
{
    if (UProcedureModule* Module = UProcedureModule::GetPtr(InWorld)) return Module->AdvanceFrom(Token, Output);
    return {};
}

void UProcedureModuleStatics::GuideCurrentProcedure(UWorld* InWorld)
{
    if (UProcedureModule* Module = UProcedureModule::GetPtr(InWorld)) Module->GuideCurrentProcedure();
}

UProcedureAsset* UProcedureModuleStatics::GetCurrentProcedureAsset(UWorld* InWorld)
{
    if (UProcedureModule* Module = UProcedureModule::GetPtr(InWorld)) return Module->GetCurrentAsset();
    return {};
}

UProcedureBase* UProcedureModuleStatics::GetCurrentProcedure(UWorld* InWorld, TSubclassOf<UProcedureBase> InClass)
{
    if (UProcedureModule* Module = UProcedureModule::GetPtr(InWorld)) return Module->GetCurrentProcedure(InClass);
    return {};
}

UProcedureBase* UProcedureModuleStatics::GetProcedureByClass(UWorld* InWorld, TSubclassOf<UProcedureBase> InClass)
{
    if (UProcedureModule* Module = UProcedureModule::GetPtr(InWorld)) return Module->GetProcedureByClass(InClass);
    return {};
}

bool UProcedureModuleStatics::HasProcedureByClass(UWorld* InWorld, TSubclassOf<UProcedureBase> InClass)
{
    if (UProcedureModule* Module = UProcedureModule::GetPtr(InWorld)) return Module->HasProcedureByClass(InClass);
    return {};
}

bool UProcedureModuleStatics::IsCurrentProcedure(UWorld* InWorld, UProcedureBase* InProcedure)
{
    if (UProcedureModule* Module = UProcedureModule::GetPtr(InWorld)) return Module->IsCurrentProcedure(InProcedure);
    return {};
}

bool UProcedureModuleStatics::IsCurrentProcedureClass(UWorld* InWorld, TSubclassOf<UProcedureBase> InClass)
{
    if (UProcedureModule* Module = UProcedureModule::GetPtr(InWorld)) return Module->IsCurrentProcedureClass(InClass);
    return {};
}

bool UProcedureModuleStatics::HasProcedureByIndex(UWorld* InWorld, int32 InIndex)
{
    if (UProcedureModule* Module = UProcedureModule::GetPtr(InWorld)) return Module->HasProcedureByIndex(InIndex);
    return {};
}

bool UProcedureModuleStatics::IsCurrentProcedureIndex(UWorld* InWorld, int32 InIndex)
{
    if (UProcedureModule* Module = UProcedureModule::GetPtr(InWorld)) return Module->IsCurrentProcedureIndex(InIndex);
    return {};
}

UProcedureBase* UProcedureModuleStatics::GetProcedureByIndex(UWorld* InWorld, int32 InIndex, TSubclassOf<UProcedureBase> InClass)
{
    if (UProcedureModule* Module = UProcedureModule::GetPtr(InWorld)) return Module->GetProcedureByIndex(InIndex, InClass);
    return {};
}
