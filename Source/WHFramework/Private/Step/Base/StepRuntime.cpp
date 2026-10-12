#include "Step/Base/StepRuntime.h"

#include "Step/Base/StepAsset.h"
#include "Step/Base/StepBase.h"
#include "Step/StepModule.h"

UStepRuntime::UStepRuntime()
{
}

bool UStepRuntime::Build(UStepAsset* InAsset, TArray<FText>& OutErrors)
{
	Shutdown();
	if(!InAsset || !InAsset->BuildCompiledGraph(Graph, OutErrors))
	{
		return false;
	}
	Definition = InAsset;
	for(const FStepCompiledNode& Node : Graph.Nodes)
	{
		UStepBase* Step = DuplicateObject<UStepBase>(Definition->FindDefinition(Node.StepGUID), this);
		Step->ClearFlags(RF_Public | RF_Standalone | RF_Transactional);
		Step->SetFlags(RF_Transient);
		Step->InitializeRuntime(this);
		Step->StepIndex = Node.Index;
		Step->StepHierarchy = Node.Hierarchy;
		StepMap.Add(Node.StepGUID, Step);
	}
	for(const FStepCompiledNode& Node : Graph.Nodes)
	{
		UStepBase* Step = StepMap.FindChecked(Node.StepGUID);
		if(!Node.ParentGUID.IsEmpty())
		{
			Step->ParentStep = StepMap.FindChecked(Node.ParentGUID);
			Step->RootStep = Step->ParentStep->RootStep;
		}
		else
		{
			Step->RootStep = Step;
			RootSteps.Add(Step);
		}
		for(const FString& ChildGUID : Node.ChildrenGUIDs)
		{
			Step->SubSteps.Add(StepMap.FindChecked(ChildGUID));
		}
	}
	bActive = true;
	for(const FStepCompiledNode& Node : Graph.Nodes)
	{
		if(!GetModule() || GetModule()->IsShuttingDown())
		{
			Shutdown();
			return false;
		}
		UStepBase* Step = StepMap.FindChecked(Node.StepGUID);
		Step->bRuntimeInitialized = true;
		Step->OnInitialize();
	}
	return GetModule() && !GetModule()->IsShuttingDown();
}

void UStepRuntime::Shutdown()
{
	bActive = false;
	for(const auto& Pair : StepMap)
	{
		Pair.Value->ShutdownRuntime();
	}
	StepMap.Reset();
	RootSteps.Reset();
	Graph = FStepCompiledGraph();
	Definition = nullptr;
}

UStepBase* UStepRuntime::FindStep(const FString& InGUID) const
{
	UStepBase* const* Step = StepMap.Find(InGUID);
	return Step ? *Step : nullptr;
}

UStepModule* UStepRuntime::GetModule() const
{
	return Cast<UStepModule>(GetOuter());
}

bool UStepRuntime::Contains(const UStepBase* InStep) const
{
	return bActive && InStep && InStep->Runtime == this && FindStep(InStep->StepGUID) == InStep;
}

FStepAssetSaveData UStepRuntime::CaptureData()
{
	FStepAssetSaveData Data;
	Data.Asset = Definition;
	Data.GraphSignature = Graph.Signature;
	for(const FStepCompiledNode& Node : Graph.Nodes)
	{
		Data.Steps.Add(StepMap.FindChecked(Node.StepGUID)->CaptureRuntimeData());
	}
	return Data;
}

bool UStepRuntime::RestoreData(const FStepAssetSaveData& InData, FText& OutError)
{
	if(InData.GraphSignature != Graph.Signature || InData.Steps.Num() != StepMap.Num())
	{
		OutError = FText::FromString(TEXT("步骤存档与当前 Start 可达图不一致；不载入旧结构。"));
		return false;
	}
	TSet<FString> Seen;
	for(const FStepRuntimeSaveData& Record : InData.Steps)
	{
		UStepBase* Step = FindStep(Record.StepGUID);
		if(!Step || Seen.Contains(Record.StepGUID))
		{
			OutError = FText::FromString(TEXT("步骤存档包含缺失或重复的节点身份。"));
			return false;
		}
		Seen.Add(Record.StepGUID);
		if(!Step->RestoreRuntimeData(Record, OutError))
		{
			return false;
		}
	}
	return true;
}

void UStepRuntime::Resume()
{
	for(const FStepCompiledNode& Node : Graph.Nodes)
	{
		if(!GetModule() || GetModule()->IsShuttingDown())
		{
			return;
		}
		StepMap.FindChecked(Node.StepGUID)->ResumeRuntime();
	}
}
