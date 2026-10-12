#include "Procedure/ProcedureModuleStatics.h"

bool UProcedureModuleStatics::StartMain()
{
    return UProcedureModule::Get().StartMain();
}

void UProcedureModuleStatics::StopMain()
{
    UProcedureModule::Get().StopMain();
}

bool UProcedureModuleStatics::SwitchProcedure(UProcedureBase* InProcedure)
{
    return UProcedureModule::Get().SwitchProcedure(InProcedure);
}

bool UProcedureModuleStatics::SwitchProcedureFrom(const FProcedureActivationToken& Token, UProcedureBase* InProcedure)
{
    return UProcedureModule::Get().SwitchProcedureFrom(Token, InProcedure);
}

bool UProcedureModuleStatics::CanSwitchProcedure(UProcedureBase* InProcedure)
{
    return UProcedureModule::Get().CanSwitchProcedure(InProcedure);
}

bool UProcedureModuleStatics::SwitchProcedureByIndex(int32 InIndex)
{
    return UProcedureModule::Get().SwitchProcedureByIndex(InIndex);
}

bool UProcedureModuleStatics::CanSwitchProcedureByIndex(int32 InIndex)
{
    return UProcedureModule::Get().CanSwitchProcedureByIndex(InIndex);
}

bool UProcedureModuleStatics::SwitchProcedureByClass(TSubclassOf<UProcedureBase> InClass)
{
    return UProcedureModule::Get().SwitchProcedureByClass(InClass);
}

bool UProcedureModuleStatics::CanSwitchProcedureByClass(TSubclassOf<UProcedureBase> InClass)
{
    return UProcedureModule::Get().CanSwitchProcedureByClass(InClass);
}

bool UProcedureModuleStatics::SwitchFirstProcedure()
{
    return UProcedureModule::Get().SwitchFirstProcedure();
}

bool UProcedureModuleStatics::SwitchLastProcedure()
{
    return UProcedureModule::Get().SwitchLastProcedure();
}

bool UProcedureModuleStatics::SwitchPreProcedure()
{
    return UProcedureModule::Get().SwitchPreProcedure();
}

bool UProcedureModuleStatics::SwitchNextProcedure()
{
    return UProcedureModule::Get().SwitchNextProcedure();
}

bool UProcedureModuleStatics::Advance(FName Output)
{
    return UProcedureModule::Get().Advance(Output);
}

bool UProcedureModuleStatics::CanAdvance(FName Output)
{
    return UProcedureModule::Get().CanAdvance(Output);
}

bool UProcedureModuleStatics::AdvanceFrom(const FProcedureActivationToken& Token, FName Output)
{
    return UProcedureModule::Get().AdvanceFrom(Token, Output);
}

void UProcedureModuleStatics::GuideCurrentProcedure()
{
    UProcedureModule::Get().GuideCurrentProcedure();
}

UProcedureAsset* UProcedureModuleStatics::GetCurrentProcedureAsset()
{
    return UProcedureModule::Get().GetCurrentAsset();
}

UProcedureBase* UProcedureModuleStatics::GetCurrentProcedure(TSubclassOf<UProcedureBase> InClass)
{
    return UProcedureModule::Get().GetCurrentProcedure(InClass);
}

UProcedureBase* UProcedureModuleStatics::GetProcedureByClass(TSubclassOf<UProcedureBase> InClass)
{
    return UProcedureModule::Get().GetProcedureByClass(InClass);
}

bool UProcedureModuleStatics::HasProcedureByClass(TSubclassOf<UProcedureBase> InClass)
{
    return UProcedureModule::Get().HasProcedureByClass(InClass);
}

bool UProcedureModuleStatics::IsCurrentProcedure(UProcedureBase* InProcedure)
{
    return UProcedureModule::Get().IsCurrentProcedure(InProcedure);
}

bool UProcedureModuleStatics::IsCurrentProcedureClass(TSubclassOf<UProcedureBase> InClass)
{
    return UProcedureModule::Get().IsCurrentProcedureClass(InClass);
}

bool UProcedureModuleStatics::HasProcedureByIndex(int32 InIndex)
{
    return UProcedureModule::Get().HasProcedureByIndex(InIndex);
}

bool UProcedureModuleStatics::IsCurrentProcedureIndex(int32 InIndex)
{
    return UProcedureModule::Get().IsCurrentProcedureIndex(InIndex);
}

UProcedureBase* UProcedureModuleStatics::GetProcedureByIndex(int32 InIndex, TSubclassOf<UProcedureBase> InClass)
{
    return UProcedureModule::Get().GetProcedureByIndex(InIndex, InClass);
}
