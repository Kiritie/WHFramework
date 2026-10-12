#include "Step/StepModuleStatics.h"

#include "Step/StepModule.h"
#include "Step/Base/StepBase.h"

UStepModule* UStepModuleStatics::ResolveModule(const UObject* WorldContextObject)
{
	return WorldContextObject ? UStepModule::GetPtr(WorldContextObject->GetWorld()) : UStepModule::GetPtr();
}

TArray<UStepAsset*> UStepModuleStatics::GetStepAssets(const UObject* WorldContextObject)
{
	UStepModule* Module = ResolveModule(WorldContextObject);
	return Module ? Module->GetAssets() : TArray<UStepAsset*>();
}

UStepAsset* UStepModuleStatics::GetCurrentStepAsset(const UObject* WorldContextObject)
{
	UStepModule* Module = ResolveModule(WorldContextObject);
	return Module ? Module->GetCurrentAsset() : nullptr;
}

UStepBase* UStepModuleStatics::GetCurrentStep(const UObject* WorldContextObject)
{
	UStepModule* Module = ResolveModule(WorldContextObject);
	return Module ? Module->GetCurrentStep() : nullptr;
}

UStepBase* UStepModuleStatics::GetCurrentRootStep(const UObject* WorldContextObject)
{
	UStepModule* Module = ResolveModule(WorldContextObject);
	return Module ? Module->GetCurrentRootStep() : nullptr;
}

UStepBase* UStepModuleStatics::GetStepByGUID(const FString& InStepGUID, const UObject* WorldContextObject)
{
	UStepModule* Module = ResolveModule(WorldContextObject);
	return Module ? Module->GetStepByGUID(InStepGUID) : nullptr;
}

UStepAsset* UStepModuleStatics::GetStepAsset(UStepAsset* InAsset, const UObject* WorldContextObject)
{
	UStepModule* Module = ResolveModule(WorldContextObject);
	return Module ? Module->GetAsset(InAsset) : nullptr;
}

void UStepModuleStatics::AddStepAsset(UStepAsset* InAsset, const UObject* WorldContextObject)
{
	UStepModule* Module = ResolveModule(WorldContextObject);
	if(Module)
	{
		Module->AddAsset(InAsset);
	}
}

void UStepModuleStatics::RemoveStepAsset(UStepAsset* InAsset, const UObject* WorldContextObject)
{
	UStepModule* Module = ResolveModule(WorldContextObject);
	if(Module)
	{
		Module->RemoveAsset(InAsset);
	}
}

void UStepModuleStatics::SwitchStepAsset(UStepAsset* InAsset, const UObject* WorldContextObject)
{
	UStepModule* Module = ResolveModule(WorldContextObject);
	if(Module)
	{
		Module->SwitchAsset(InAsset);
	}
}

void UStepModuleStatics::StartStep(int32 InRootStepIndex, bool bSkipSteps, const UObject* WorldContextObject)
{
	UStepModule* Module = ResolveModule(WorldContextObject);
	if(Module)
	{
		Module->StartStep(InRootStepIndex, bSkipSteps);
	}
}

void UStepModuleStatics::EndStep(bool bRestoreSteps, const UObject* WorldContextObject)
{
	UStepModule* Module = ResolveModule(WorldContextObject);
	if(Module)
	{
		Module->EndStep(bRestoreSteps);
	}
}

void UStepModuleStatics::RestoreStep(UStepBase* InStep)
{
	if(InStep)
	{
		InStep->Restore();
	}
}

void UStepModuleStatics::EnterStep(UStepBase* InStep)
{
	if(InStep)
	{
		InStep->Enter();
	}
}

void UStepModuleStatics::RefreshStep(UStepBase* InStep)
{
	if(InStep)
	{
		InStep->Refresh();
	}
}

void UStepModuleStatics::GuideStep(UStepBase* InStep)
{
	if(InStep)
	{
		InStep->Guide();
	}
}

void UStepModuleStatics::ExecuteStep(UStepBase* InStep)
{
	if(InStep)
	{
		InStep->Execute();
	}
}

void UStepModuleStatics::LeaveStep(UStepBase* InStep)
{
	if(InStep)
	{
		InStep->Leave();
	}
}

void UStepModuleStatics::CompleteStep(UStepBase* InStep, EStepExecuteResult InStepExecuteResult)
{
	if(InStep)
	{
		InStep->Complete(InStepExecuteResult);
	}
}
