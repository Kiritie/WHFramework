#include "Step/StepModule.h"

#include "Step/Base/StepAsset.h"
#include "Step/Base/StepBase.h"
#include "Step/Base/StepRuntime.h"
#include "Step/StepModuleNetworkComponent.h"
#include "Event/EventModuleStatics.h"
#include "Event/Events/Step/Event_StepEnded.h"
#include "Event/Events/Step/Event_StepStarted.h"
#include "UObject/StrongObjectPtr.h"
#include "Misc/ScopeExit.h"

struct FStepCallbackScope
{
	explicit FStepCallbackScope(UStepModule* InModule)
		: Module(InModule)
	{
		++Module->CallbackDepth;
	}

	~FStepCallbackScope()
	{
		--Module->CallbackDepth;
		Module->DrainDeferred();
	}

	UStepModule* Module;
};

IMPLEMENTATION_MODULE(UStepModule)

UStepModule::UStepModule()
{
	ModuleName = FName("StepModule");
	ModuleDisplayName = FText::FromString(TEXT("Step Module"));
	ModuleNetworkComponent = UStepModuleNetworkComponent::StaticClass();
	SaveScope = ESaveScope::World;
	SaveDataVersion = 1;
}

UStepModule::~UStepModule()
{
	TERMINATION_MODULE(UStepModule)
}

#if WITH_EDITOR
void UStepModule::OnGenerate()
{
	Super::OnGenerate();
}

void UStepModule::OnDestroy()
{
	ClearRuntimes();
	Super::OnDestroy();
	TERMINATION_MODULE(UStepModule)
}
#endif

void UStepModule::OnInitialize()
{
	Super::OnInitialize();
	AddAsset(DefaultAsset);
}

void UStepModule::OnPreparatory(EPhase InPhase)
{
	Super::OnPreparatory(InPhase);
	if(PHASEC(InPhase, EPhase::Final) && !CurrentRuntime && !bLoadingSteps)
	{
		SwitchAsset(DefaultAsset);
	}
}

void UStepModule::OnRefresh(float DeltaSeconds, bool bInEditor)
{
	Super::OnRefresh(DeltaSeconds, bInEditor);
	DrainDeferred();
	if(bInEditor || IsShuttingDown() || bLoadingSteps || StepModuleState != EStepModuleState::Running || !CurrentRuntime)
	{
		return;
	}
	FStepCallbackScope BatchScope(this);
	TStrongObjectPtr<UStepRuntime> RuntimeGuard(CurrentRuntime);
	UStepBase* Root = GetCurrentRootStep();
	if(!Root)
	{
		EndStep(false);
		return;
	}
	{
		FStepCallbackScope Scope(this);
		Root->TickRuntime(DeltaSeconds);
	}
	if(CurrentRuntime != RuntimeGuard.Get() || !Owns(Root) || StepModuleState != EStepModuleState::Running)
	{
		return;
	}
	if(Root->StepState == EStepState::Leaved)
	{
		const int32 NextIndex = CurrentRootStepIndex + 1;
		if(CurrentRuntime->RootSteps.IsValidIndex(NextIndex))
		{
			UStepBase* Next = CurrentRuntime->RootSteps[NextIndex];
			CurrentRootStepIndex = NextIndex;
			if(Next->CheckStepCondition(Root))
			{
				EnterStep(Next);
			}
			else
			{
				SkipUnmatchedStep(Next);
			}
		}
		else
		{
			EndStep(false);
		}
	}
}

void UStepModule::OnPause()
{
	Super::OnPause();
	if(StepModuleState == EStepModuleState::Running)
	{
		StepModuleState = EStepModuleState::Paused;
	}
}

void UStepModule::OnUnPause()
{
	Super::OnUnPause();
	if(StepModuleState == EStepModuleState::Paused)
	{
		StepModuleState = EStepModuleState::Running;
	}
}

void UStepModule::OnTermination(EPhase InPhase)
{
	if(bNotifyingTermination)
	{
		return;
	}
	FStepCallbackScope Scope(this);
	TGuardValue<bool> Guard(bNotifyingTermination, true);
	if(PHASEC(InPhase, EPhase::Primary))
	{
		bTerminating = true;
		DeferredCommands.Reset();
		ClearRuntimes();
	}
	Super::OnTermination(InPhase);
}

FString UStepModule::GetModuleDebugMessage()
{
	return FString::Printf(TEXT("Step: %s | Reachable: %d"), CurrentStep ? *CurrentStep->StepDisplayName.ToString() : TEXT("None"), CurrentRuntime ? CurrentRuntime->StepMap.Num() : 0);
}

UStepRuntime* UStepModule::GetRuntime(UStepAsset* InAsset) const
{
	for(UStepRuntime* Runtime : Runtimes)
	{
		if(Runtime->Definition == InAsset)
		{
			return Runtime;
		}
	}
	return nullptr;
}

UStepAsset* UStepModule::GetAsset(UStepAsset* InAsset) const
{
	return GetRuntime(InAsset) ? InAsset : nullptr;
}

void UStepModule::AddAsset(UStepAsset* InAsset)
{
	if(bTerminating || bLoadingSteps || bClearingRuntimes)
	{
		return;
	}
	if(CallbackDepth > 0)
	{
		Defer(EStepDeferredAction::AddAsset, InAsset);
		return;
	}
	FStepCallbackScope Scope(this);
	AddAssetInternal(InAsset);
}

void UStepModule::AddAssetInternal(UStepAsset* InAsset)
{
	if(!InAsset || GetRuntime(InAsset) || IsShuttingDown() || bLoadingSteps)
	{
		return;
	}
	TStrongObjectPtr<UStepRuntime> Candidate(NewObject<UStepRuntime>(this));
	TArray<FText> Errors;
	const bool bBuilt = Candidate->Build(InAsset, Errors);
	if(IsShuttingDown())
	{
		Candidate->Shutdown();
		return;
	}
	if(!bBuilt)
	{
		for(const FText& Error : Errors)
		{
			UE_LOG(LogTemp, Error, TEXT("Step runtime: %s"), *Error.ToString());
		}
		return;
	}
	Runtimes.Add(Candidate.Get());
}

void UStepModule::RemoveAsset(UStepAsset* InAsset)
{
	if(bTerminating || bClearingRuntimes || bLoadingSteps)
	{
		return;
	}
	if(CallbackDepth > 0)
	{
		Defer(EStepDeferredAction::RemoveAsset, InAsset);
		return;
	}
	FStepCallbackScope Scope(this);
	UStepRuntime* Runtime = GetRuntime(InAsset);
	if(!Runtime)
	{
		return;
	}
	TStrongObjectPtr<UStepRuntime> RuntimeGuard(Runtime);
	if(CurrentRuntime == Runtime)
	{
		EndStepInternal(false);
		CurrentRuntime = nullptr;
		CurrentStep = nullptr;
		CurrentRootStepIndex = INDEX_NONE;
	}
	Runtimes.Remove(Runtime);
	Runtime->Shutdown();
}

void UStepModule::SwitchAsset(UStepAsset* InAsset)
{
	if(!InAsset || bTerminating || bLoadingSteps || bClearingRuntimes)
	{
		return;
	}
	if(CallbackDepth > 0)
	{
		Defer(EStepDeferredAction::SwitchAsset, InAsset);
		return;
	}
	FStepCallbackScope Scope(this);
	AddAssetInternal(InAsset);
	UStepRuntime* Runtime = GetRuntime(InAsset);
	if(!Runtime || Runtime == CurrentRuntime || IsShuttingDown())
	{
		return;
	}
	EndStepInternal(false);
	if(IsShuttingDown())
	{
		return;
	}
	CurrentRuntime = Runtime;
	CurrentStep = nullptr;
	CurrentRootStepIndex = INDEX_NONE;
	StepModuleState = EStepModuleState::None;
	if(InAsset->bAutoStartFirst)
	{
		StartStep();
	}
}

void UStepModule::StartStepByGUID(const FString& InRootGUID, bool bSkipSteps)
{
	if(!CurrentRuntime)
	{
		return;
	}
	const int32 Index = CurrentRuntime->RootSteps.IndexOfByPredicate([&InRootGUID](const UStepBase* Step){ return Step->StepGUID == InRootGUID; });
	if(Index != INDEX_NONE)
	{
		StartStep(Index, bSkipSteps);
	}
}

bool UStepModule::CanSkipTree(const UStepBase* InStep) const
{
	if(IsShuttingDown())
	{
		return false;
	}
	if(!InStep->IsCompleted() && !InStep->IsSkipAble())
	{
		return false;
	}
	for(const UStepBase* Child : InStep->SubSteps)
	{
		if(!CanSkipTree(Child))
		{
			return false;
		}
	}
	return true;
}

void UStepModule::StartStep(int32 InRootStepIndex, bool bSkipSteps)
{
	if(!CurrentRuntime || bTerminating || bLoadingSteps || bClearingRuntimes)
	{
		return;
	}
	if(CallbackDepth > 0)
	{
		Defer(EStepDeferredAction::Start, GetCurrentAsset(), FString(), InRootStepIndex, bSkipSteps);
		return;
	}
	FStepCallbackScope Scope(this);
	const int32 Index = InRootStepIndex == INDEX_NONE ? 0 : InRootStepIndex;
	if(!CurrentRuntime->RootSteps.IsValidIndex(Index))
	{
		return;
	}
	if(bSkipSteps)
	{
		for(int32 PreviousIndex = 0; PreviousIndex < Index; ++PreviousIndex)
		{
			if(!CanSkipTree(CurrentRuntime->RootSteps[PreviousIndex]))
			{
				return;
			}
		}
	}
	if(StepModuleState == EStepModuleState::Running || StepModuleState == EStepModuleState::Paused)
	{
		if(Index == CurrentRootStepIndex)
		{
			return;
		}
		EndStepInternal(false);
	}
	if(IsShuttingDown())
	{
		return;
	}
	UStepBase* Root = CurrentRuntime->RootSteps[Index];
	if(Root->StepState != EStepState::None)
	{
		TGuardValue<bool> RestoreGuard(bRestoringSteps, true);
		RestoreTree(Root);
	}
	if(IsShuttingDown())
	{
		return;
	}
	StepModuleState = EStepModuleState::Running;
	CurrentRootStepIndex = Index;
	TStrongObjectPtr<UStepRuntime> RuntimeGuard(CurrentRuntime);
	BroadcastStepStarted(Index);
	if(CurrentRuntime != RuntimeGuard.Get() || IsShuttingDown())
	{
		return;
	}
	if(bSkipSteps)
	{
		for(int32 PreviousIndex = 0; PreviousIndex < Index; ++PreviousIndex)
		{
			UStepBase* Previous = CurrentRuntime->RootSteps[PreviousIndex];
			CompleteStep(Previous, EStepExecuteResult::Skipped);
			LeaveStep(Previous);
		}
	}
	EnterStep(Root);
}

void UStepModule::StopTree(UStepBase* InStep)
{
	if(IsShuttingDown())
	{
		return;
	}
	for(UStepBase* Child : InStep->SubSteps)
	{
		StopTree(Child);
	}
	if(!IsShuttingDown() && InStep->StepState != EStepState::None && InStep->StepState != EStepState::Leaved)
	{
		FStepCallbackScope Scope(this);
		InStep->OnLeave();
	}
}

void UStepModule::EndStep(bool bRestoreSteps)
{
	if(bTerminating || bLoadingSteps || bClearingRuntimes)
	{
		return;
	}
	if(CallbackDepth > 0)
	{
		Defer(EStepDeferredAction::End, GetCurrentAsset(), FString(), INDEX_NONE, bRestoreSteps);
		return;
	}
	FStepCallbackScope Scope(this);
	EndStepInternal(bRestoreSteps);
}

void UStepModule::EndStepInternal(bool bRestoreSteps)
{
	if(bStoppingSteps || bLoadingSteps)
	{
		return;
	}
	TGuardValue<bool> Guard(bStoppingSteps, true);
	TStrongObjectPtr<UStepRuntime> RuntimeGuard(CurrentRuntime);
	const TArray<UStepBase*> Roots = CurrentRuntime ? CurrentRuntime->RootSteps : TArray<UStepBase*>();
	const bool bWasActive = StepModuleState == EStepModuleState::Running || StepModuleState == EStepModuleState::Paused;
	StepModuleState = EStepModuleState::Ended;
	if(CurrentRuntime)
	{
		for(UStepBase* Root : Roots)
		{
			StopTree(Root);
		}
		if(bRestoreSteps && !IsShuttingDown())
		{
			TGuardValue<bool> RestoreGuard(bRestoringSteps, true);
			for(UStepBase* Root : Roots)
			{
				RestoreTree(Root);
			}
		}
	}
	CurrentStep = nullptr;
	CurrentRootStepIndex = INDEX_NONE;
	if(bWasActive && !IsShuttingDown())
	{
		BroadcastStepEnded();
	}
}

bool UStepModule::Owns(const UStepBase* InStep) const
{
	return !IsShuttingDown() && !bStoppingSteps && !bLoadingSteps && !bRestoringSteps && CurrentRuntime && CurrentRuntime->Contains(InStep);
}

void UStepModule::RestoreTree(UStepBase* InStep)
{
	if(IsShuttingDown())
	{
		return;
	}
	for(int32 Index = InStep->SubSteps.Num() - 1; Index >= 0; --Index)
	{
		RestoreTree(InStep->SubSteps[Index]);
	}
	if(!IsShuttingDown())
	{
		InStep->OnRestore();
	}
}

void UStepModule::RestoreStep(UStepBase* InStep)
{
	if(IsShuttingDown() || bLoadingSteps || !CurrentRuntime || !CurrentRuntime->Contains(InStep))
	{
		return;
	}
	if(CallbackDepth > 0)
	{
		Defer(EStepDeferredAction::Restore, GetCurrentAsset(), InStep->StepGUID);
		return;
	}
	if(bStoppingSteps || bRestoringSteps)
	{
		return;
	}
	FStepCallbackScope Scope(this);
	TGuardValue<bool> Guard(bRestoringSteps, true);
	RestoreTree(InStep);
	if(IsShuttingDown())
	{
		return;
	}
	if(CurrentStep == InStep || InStep->IsParentOf(CurrentStep))
	{
		CurrentStep = InStep->ParentStep;
	}
	if(InStep->ParentStep && InStep->ParentStep->StepState == EStepState::Executing)
	{
		InStep->ParentStep->CurrentSubStepIndex = InStep->StepIndex - 1;
	}
	else if(InStep->IsRootStep() && CurrentRootStepIndex == InStep->StepIndex)
	{
		CurrentRootStepIndex = INDEX_NONE;
		StepModuleState = EStepModuleState::None;
	}
}

void UStepModule::EnterStep(UStepBase* InStep)
{
	if(!Owns(InStep) || bStoppingSteps || StepModuleState != EStepModuleState::Running || InStep->StepState != EStepState::None)
	{
		return;
	}
	if(InStep->ParentStep && InStep->ParentStep->StepState != EStepState::Executing)
	{
		return;
	}
	UStepBase* Last = CurrentStep;
	CurrentStep = InStep;
	CurrentRootStepIndex = InStep->RootStep->StepIndex;
	FStepCallbackScope Scope(this);
	InStep->OnEnter(Last);
}

void UStepModule::RefreshStep(UStepBase* InStep)
{
	if(Owns(InStep) && !bStoppingSteps && StepModuleState == EStepModuleState::Running && InStep->StepState == EStepState::Executing)
	{
		FStepCallbackScope Scope(this);
		InStep->OnRefresh();
	}
}

void UStepModule::GuideStep(UStepBase* InStep)
{
	if(Owns(InStep) && !bStoppingSteps && StepModuleState == EStepModuleState::Running && InStep->IsEntered())
	{
		FStepCallbackScope Scope(this);
		InStep->OnGuide();
	}
}

void UStepModule::ExecuteStep(UStepBase* InStep)
{
	if(Owns(InStep) && !bStoppingSteps && StepModuleState == EStepModuleState::Running && InStep->StepState == EStepState::Entered)
	{
		FStepCallbackScope Scope(this);
		InStep->OnExecute();
	}
}

void UStepModule::CompleteStep(UStepBase* InStep, EStepExecuteResult InStepExecuteResult)
{
	FStepCallbackScope BatchScope(this);
	if(!Owns(InStep) || bStoppingSteps || InStep->IsCompleted() || InStepExecuteResult == EStepExecuteResult::None)
	{
		return;
	}
	if(InStepExecuteResult == EStepExecuteResult::Skipped && !CanSkipTree(InStep))
	{
		return;
	}
	if(InStep->StepState == EStepState::None && InStepExecuteResult != EStepExecuteResult::Skipped)
	{
		return;
	}
	for(const UStepBase* Child : InStep->SubSteps)
	{
		if(!Child->IsCompleted(true) && !CanSkipTree(Child))
		{
			return;
		}
	}
	if(!Owns(InStep))
	{
		return;
	}
	const FString Key = InStep->StepGUID + TEXT(":complete");
	if(Transitioning.Contains(Key))
	{
		return;
	}
	Transitioning.Add(Key);
	ON_SCOPE_EXIT { Transitioning.Remove(Key); };
	for(UStepBase* Child : InStep->SubSteps)
	{
		if(!Child->IsCompleted())
		{
			CompleteStep(Child, EStepExecuteResult::Skipped);
		}
		LeaveStep(Child);
	}
	if(Owns(InStep))
	{
		FStepCallbackScope Scope(this);
		InStep->OnComplete(InStepExecuteResult);
	}
}

void UStepModule::LeaveStep(UStepBase* InStep)
{
	FStepCallbackScope BatchScope(this);
	if(!Owns(InStep) || bStoppingSteps || InStep->StepState == EStepState::Leaved)
	{
		return;
	}
	if(!InStep->IsCompleted())
	{
		CompleteStep(InStep, EStepExecuteResult::Skipped);
	}
	if(Owns(InStep) && InStep->StepState == EStepState::Completed)
	{
		FStepCallbackScope Scope(this);
		InStep->OnLeave();
	}
}

bool UStepModule::IsAllStepCompleted() const
{
	if(!CurrentRuntime)
	{
		return true;
	}
	for(const UStepBase* Root : CurrentRuntime->RootSteps)
	{
		if(!Root->IsCompleted(true))
		{
			return false;
		}
	}
	return true;
}

TArray<UStepAsset*> UStepModule::GetAssets() const
{
	TArray<UStepAsset*> Result;
	for(const UStepRuntime* Runtime : Runtimes)
	{
		Result.Add(Runtime->Definition);
	}
	return Result;
}

UStepAsset* UStepModule::GetCurrentAsset() const
{
	return CurrentRuntime ? CurrentRuntime->Definition : nullptr;
}

UStepBase* UStepModule::GetFirstStep() const
{
	return CurrentRuntime && !CurrentRuntime->RootSteps.IsEmpty() ? CurrentRuntime->RootSteps[0] : nullptr;
}

UStepBase* UStepModule::GetCurrentRootStep() const
{
	return CurrentRuntime && CurrentRuntime->RootSteps.IsValidIndex(CurrentRootStepIndex) ? CurrentRuntime->RootSteps[CurrentRootStepIndex] : nullptr;
}

TArray<UStepBase*> UStepModule::GetRootSteps() const
{
	return CurrentRuntime ? CurrentRuntime->RootSteps : TArray<UStepBase*>();
}

TMap<FString, UStepBase*> UStepModule::GetStepMap() const
{
	return CurrentRuntime ? CurrentRuntime->StepMap : TMap<FString, UStepBase*>();
}

UStepBase* UStepModule::GetStepByGUID(const FString& InStepGUID) const
{
	return CurrentRuntime ? CurrentRuntime->FindStep(InStepGUID) : nullptr;
}

void UStepModule::RefreshAutomaticTimers()
{
	for(UStepRuntime* Runtime : Runtimes)
	{
		for(const auto& Pair : Runtime->StepMap)
		{
			Pair.Value->RefreshAutomaticTimers(true);
		}
	}
}

void UStepModule::SetGlobalStepExecuteType(EStepExecuteType InType)
{
	if(GlobalStepExecuteType != InType)
	{
		GlobalStepExecuteType = InType;
		RefreshAutomaticTimers();
	}
}

void UStepModule::SetGlobalStepCompleteType(EStepCompleteType InType)
{
	if(GlobalStepCompleteType != InType)
	{
		GlobalStepCompleteType = InType;
		RefreshAutomaticTimers();
	}
}

void UStepModule::SetGlobalStepLeaveType(EStepLeaveType InType)
{
	if(GlobalStepLeaveType != InType)
	{
		GlobalStepLeaveType = InType;
		RefreshAutomaticTimers();
	}
}

void UStepModule::ClearRuntimes()
{
	if(CallbackDepth > 0 || bClearingRuntimes)
	{
		bPendingClear = true;
		bStoppingSteps = true;
		DeferredCommands.Reset();
		return;
	}
	ClearRuntimesImmediate();
}

void UStepModule::ClearRuntimesImmediate()
{
	if(bClearingRuntimes)
	{
		return;
	}
	TGuardValue<bool> Guard(bClearingRuntimes, true);
	bPendingClear = false;
	bStoppingSteps = true;
	DeferredCommands.Reset();
	CurrentStep = nullptr;
	CurrentRuntime = nullptr;
	CurrentRootStepIndex = INDEX_NONE;
	StepModuleState = EStepModuleState::None;
	for(UStepRuntime* Runtime : Runtimes)
	{
		Runtime->Shutdown();
	}
	Runtimes.Reset();
	bStoppingSteps = bTerminating || bPendingClear;
}

FParameter UStepModule::ToData()
{
	FStepCallbackScope Scope(this);
	FStepModuleSaveData Data;
	for(UStepRuntime* Runtime : Runtimes)
	{
		Data.Assets.Add(Runtime->CaptureData());
	}
	Data.CurrentAsset = GetCurrentAsset();
	Data.CurrentStepGUID = CurrentStep ? CurrentStep->StepGUID : FString();
	Data.CurrentRootGUID = GetCurrentRootStep() ? GetCurrentRootStep()->StepGUID : FString();
	Data.State = StepModuleState;
	return FParameter(MoveTemp(Data));
}

void UStepModule::LoadData(const FParameter& InSaveData, EPhase InPhase)
{
	if(CallbackDepth > 0 || bClearingRuntimes || bTerminating || (PHASEC(InPhase, EPhase::Primary) && bLoadingSteps))
	{
		UE_LOG(LogTemp, Warning, TEXT("拒绝重入或终止期间的 Step 存档加载。"));
		return;
	}
	FStepCallbackScope Scope(this);
	const FStepModuleSaveData& Data = InSaveData.GetRef<FStepModuleSaveData>();
	if(!Data.IsSaved())
	{
		return;
	}
	if(PHASEC(InPhase, EPhase::Primary))
	{
		bLoadingSteps = true;
		TArray<TStrongObjectPtr<UStepRuntime>> Candidates;
		TSet<FSoftObjectPath> SeenAssets;
		bool bValid = Data.State <= EStepModuleState::Ended;
		for(const FStepAssetSaveData& Record : Data.Assets)
		{
			UStepAsset* Asset = Record.Asset.LoadSynchronous();
			TStrongObjectPtr<UStepRuntime> Candidate(NewObject<UStepRuntime>(this));
			TArray<FText> Errors;
			FText Error;
			if(!Asset || SeenAssets.Contains(Record.Asset.ToSoftObjectPath()) || !Candidate->Build(Asset, Errors) || !Candidate->RestoreData(Record, Error))
			{
				UE_LOG(LogTemp, Error, TEXT("Step save restore failed: %s %s"), *Record.Asset.ToString(), *Error.ToString());
				Candidate->Shutdown();
				bValid = false;
				break;
			}
			SeenAssets.Add(Record.Asset.ToSoftObjectPath());
			Candidates.Add(MoveTemp(Candidate));
		}
		UStepRuntime* Selected = nullptr;
		for(const TStrongObjectPtr<UStepRuntime>& Candidate : Candidates)
		{
			if(Candidate->Definition == Data.CurrentAsset.Get())
			{
				Selected = Candidate.Get();
			}
		}
		UStepBase* SavedCurrent = Selected ? Selected->FindStep(Data.CurrentStepGUID) : nullptr;
		UStepBase* SavedRoot = Selected ? Selected->FindStep(Data.CurrentRootGUID) : nullptr;
		if((!Data.CurrentAsset.IsNull() && !Selected) || (!Data.CurrentStepGUID.IsEmpty() && !SavedCurrent) || (!Data.CurrentRootGUID.IsEmpty() && (!SavedRoot || !SavedRoot->IsRootStep())))
		{
			bValid = false;
		}
		if((Data.State == EStepModuleState::Running || Data.State == EStepModuleState::Paused) && (!SavedRoot || (SavedCurrent && SavedCurrent->RootStep != SavedRoot)))
		{
			bValid = false;
		}
		if(!bValid || bTerminating || bPendingClear)
		{
			for(const TStrongObjectPtr<UStepRuntime>& Candidate : Candidates)
			{
				Candidate->Shutdown();
			}
			bLoadingSteps = false;
			return;
		}
		ClearRuntimesImmediate();
		if(bTerminating || bPendingClear)
		{
			for(const TStrongObjectPtr<UStepRuntime>& Candidate : Candidates)
			{
				Candidate->Shutdown();
			}
			bLoadingSteps = false;
			return;
		}
		for(const TStrongObjectPtr<UStepRuntime>& Candidate : Candidates)
		{
			Runtimes.Add(Candidate.Get());
		}
		CurrentRuntime = Selected;
		CurrentStep = SavedCurrent;
		CurrentRootStepIndex = SavedRoot ? SavedRoot->StepIndex : INDEX_NONE;
		StepModuleState = Data.State;
	}
	if(PHASEC(InPhase, EPhase::Final) && bLoadingSteps && !bTerminating && !bPendingClear)
	{
		for(UStepRuntime* Runtime : Runtimes)
		{
			Runtime->Resume();
			if(IsShuttingDown())
			{
				break;
			}
		}
		bLoadingSteps = false;
	}
}

void UStepModule::UnloadData(EPhase InPhase)
{
	if(PHASEC(InPhase, EPhase::Primary))
	{
		ClearRuntimes();
		bLoadingSteps = false;
		DrainDeferred();
	}
}

void UStepModule::Defer(EStepDeferredAction Action, UStepAsset* Asset, const FString& GUID, int32 Index, bool bOption)
{
	if(IsShuttingDown() || bLoadingSteps)
	{
		return;
	}
	FStepDeferredCommand Command;
	Command.Action = Action;
	Command.Asset = Asset;
	Command.StepGUID = GUID;
	Command.RootIndex = Index;
	Command.bOption = bOption;
	DeferredCommands.Add(Command);
}

void UStepModule::DrainDeferred()
{
	if(CallbackDepth != 0 || bDrainingCommands || bClearingRuntimes)
	{
		return;
	}
	while(bPendingClear)
	{
		bPendingClear = false;
		ClearRuntimesImmediate();
		bLoadingSteps = false;
	}
	if(bLoadingSteps || bTerminating)
	{
		return;
	}
	TGuardValue<bool> Guard(bDrainingCommands, true);
	int32 Count = 0;
	while(!DeferredCommands.IsEmpty() && !IsShuttingDown() && !bLoadingSteps && Count++ < 128)
	{
		const FStepDeferredCommand Command = DeferredCommands[0];
		DeferredCommands.RemoveAt(0, 1, EAllowShrinking::No);
		switch(Command.Action)
		{
			case EStepDeferredAction::AddAsset:
			{
				AddAsset(Command.Asset);
				break;
			}
			case EStepDeferredAction::RemoveAsset:
			{
				RemoveAsset(Command.Asset);
				break;
			}
			case EStepDeferredAction::SwitchAsset:
			{
				SwitchAsset(Command.Asset);
				break;
			}
			case EStepDeferredAction::Start:
			{
				if(Command.Asset == GetCurrentAsset())
				{
					StartStep(Command.RootIndex, Command.bOption);
				}
				break;
			}
			case EStepDeferredAction::End:
			{
				if(Command.Asset == GetCurrentAsset())
				{
					EndStep(Command.bOption);
				}
				break;
			}
			case EStepDeferredAction::Restore:
			{
				if(Command.Asset == GetCurrentAsset())
				{
					RestoreStep(GetStepByGUID(Command.StepGUID));
				}
				break;
			}
			default:
			{
				break;
			}
		}
	}
	while(bPendingClear)
	{
		ClearRuntimesImmediate();
		bLoadingSteps = false;
	}
}

void UStepModule::SkipUnmatchedStep(UStepBase* InStep)
{
	if(!Owns(InStep))
	{
		return;
	}
	FStepCallbackScope Scope(this);
	for(UStepBase* Child : InStep->SubSteps)
	{
		SkipUnmatchedStep(Child);
	}
	if(!Owns(InStep))
	{
		return;
	}
	if(!InStep->IsCompleted())
	{
		InStep->OnComplete(EStepExecuteResult::Skipped);
	}
	if(Owns(InStep) && InStep->StepState == EStepState::Completed)
	{
		InStep->OnLeave();
	}
}

void UStepModule::BroadcastStepStarted(int32 RootIndex)
{
	UEventModuleStatics::BroadcastEvent<FEventStepStarted>(this, {RootIndex});
}

void UStepModule::BroadcastStepEnded()
{
	UEventModuleStatics::BroadcastEvent<FEventStepEnded>(this);
}
