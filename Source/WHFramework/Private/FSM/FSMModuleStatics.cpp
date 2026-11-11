#include "FSM/FSMModuleStatics.h"

#include "FSM/FSMModule.h"
#include "FSM/Components/FSMComponent.h"

void UFSMModuleStatics::RegisterFSM(UFSMComponent* FSM)
{
	if (UFSMModule* Module = FSM ? UFSMModule::GetPtr(FSM->GetWorld()) : nullptr) Module->RegisterFSM(FSM);
}

void UFSMModuleStatics::UnregisterFSM(UFSMComponent* FSM)
{
	if (UFSMModule* Module = FSM ? UFSMModule::GetPtr(FSM->GetWorld()) : nullptr) Module->UnregisterFSM(FSM);
}

bool UFSMModuleStatics::HasFSMGroup(UWorld* World, FName Name)
{
	const UFSMModule* Module = UFSMModule::GetPtr(World);
	return Module && Module->HasGroup(Name);
}

FFSMGroup UFSMModuleStatics::GetFSMGroup(UWorld* World, FName Name)
{
	const UFSMModule* Module = UFSMModule::GetPtr(World);
	return Module ? Module->GetGroup(Name) : FFSMGroup();
}

void UFSMModuleStatics::SwitchFSMGroupStateByIndex(UWorld* World, FName Name, int32 Index, const TArray<FParameter>& Params)
{
	if (UFSMModule* Module = UFSMModule::GetPtr(World)) Module->SwitchGroupStateByIndex(Name, Index, Params);
}

void UFSMModuleStatics::SwitchFSMGroupStateByName(UWorld* World, FName Name, FName State, const TArray<FParameter>& Params)
{
	if (UFSMModule* Module = UFSMModule::GetPtr(World)) Module->SwitchGroupStateByName(Name, State, Params);
}

void UFSMModuleStatics::SwitchFSMGroupStateByClass(UWorld* World, FName Name, TSubclassOf<UFiniteStateBase> State, const TArray<FParameter>& Params)
{
	if (UFSMModule* Module = UFSMModule::GetPtr(World)) Module->SwitchGroupStateByClass(Name, State, Params);
}

void UFSMModuleStatics::SwitchFSMGroupDefaultState(UWorld* World, FName Name, const TArray<FParameter>& Params)
{
	if (UFSMModule* Module = UFSMModule::GetPtr(World)) Module->SwitchGroupDefaultState(Name, Params);
}

void UFSMModuleStatics::SwitchFSMGroupFinalState(UWorld* World, FName Name, const TArray<FParameter>& Params)
{
	if (UFSMModule* Module = UFSMModule::GetPtr(World)) Module->SwitchGroupFinalState(Name, Params);
}

void UFSMModuleStatics::SwitchFSMGroupLastState(UWorld* World, FName Name, const TArray<FParameter>& Params)
{
	if (UFSMModule* Module = UFSMModule::GetPtr(World)) Module->SwitchGroupLastState(Name, Params);
}

void UFSMModuleStatics::SwitchFSMGroupNextState(UWorld* World, FName Name, const TArray<FParameter>& Params)
{
	if (UFSMModule* Module = UFSMModule::GetPtr(World)) Module->SwitchGroupNextState(Name, Params);
}
