#include "SaveGame/SaveGameAsyncExecutor.h"
#include "SaveGame/SaveGameStorage.h"
#include "Misc/Paths.h"
FSaveGameAsyncExecutor::~FSaveGameAsyncExecutor()
{
	if (State)
	{
		State->Cancel.store(true);
		Task.Wait();
	}
}
bool FSaveGameAsyncExecutor::Start(FSaveGenerationPlan&& P)
{
	check(IsInGameThread());
	if (State || !P.SaveId.IsValid())
		return false;
	State = MakeShared<FState, ESPMode::ThreadSafe>();
	auto S = State;
	Task = UE::Tasks::Launch(UE_SOURCE_LOCATION,
	                         [S, P = MoveTemp(P)]() mutable
	                         {
		                         S->Result = Execute(P, S->Cancel);
	                         });
	return true;
}
bool FSaveGameAsyncExecutor::Poll(FSaveOperationResult& O)
{
	check(IsInGameThread());
	if (!State || !Task.IsCompleted())
		return false;
	O = State->Result;
	State.Reset();
	Task = UE::Tasks::FTask();
	return true;
}
FSaveOperationResult FSaveGameAsyncExecutor::Finish()
{
	check(IsInGameThread());
	if (!State)
		return FSaveOperationResult::Success();
	Task.Wait();
	FSaveOperationResult R = State->Result;
	State.Reset();
	Task = UE::Tasks::FTask();
	return R;
}
void FSaveGameAsyncExecutor::RequestCancel()
{
	if (State)
		State->Cancel.store(true);
}
FSaveOperationResult FSaveGameAsyncExecutor::Execute(const FSaveGenerationPlan& P, const std::atomic_bool& Cancel)
{
	auto Fail = [](ESaveResultCode C, const TCHAR* T)
	{
		return FSaveOperationResult::Failed(C, FText::FromString(T));
	};
	FSaveGameStorage Storage(P.UserIndex);
	FSaveManifest Current;
	if (!Storage.ReadManifest(P.SaveId, Current) || Current.CurrentGeneration != P.Manifest.CurrentGeneration)
		return Fail(ESaveResultCode::CommitFailed, TEXT("Save slot generation changed before write"));
	if (Cancel.load())
		return Fail(ESaveResultCode::Busy, TEXT("Save canceled before write"));
	if (Current.CurrentGeneration == MAX_int32)
		return Fail(ESaveResultCode::CommitFailed, TEXT("Generation counter exhausted"));
	const int32 Next = Current.CurrentGeneration + 1;
	Storage.CleanupUncommittedGenerations(P.SaveId, Current.CurrentGeneration);
	if (!Storage.PrepareTempGeneration(P.SaveId, Current.CurrentGeneration, Next))
		return Fail(ESaveResultCode::WriteFailed, TEXT("Prepare generation failed"));
	for (const auto& M : P.Modules)
	{
		if (Cancel.load())
			return Fail(ESaveResultCode::Busy, TEXT("Save canceled before commit"));
		if (!Storage.WriteBinary(Storage.GetTempModuleFilePath(P.SaveId, Next, M.ModuleName), M.Bytes))
			return Fail(ESaveResultCode::WriteFailed, TEXT("Module file write failed"));
	}
	if (P.Voxel.IsSet())
	{
		FString E;
		if (!UVoxelModule::WriteSaveCapture(P.Voxel.GetValue(), Storage.GetTempGenerationDir(P.SaveId, Next), E))
			return FSaveOperationResult::Failed(ESaveResultCode::WriteFailed, FText::FromString(E));
	}
	if (Cancel.load())
		return Fail(ESaveResultCode::Busy, TEXT("Save canceled before commit"));
	if (!Storage.CommitGeneration(P.SaveId, Next))
		return Fail(ESaveResultCode::CommitFailed, TEXT("Generation commit failed"));
	// Past this point finish publishing the manifest even if cancel was requested. Never acknowledge a directory rename alone.
	FSaveManifest Manifest = P.Manifest;
	Manifest.CurrentGeneration = Next;
	Manifest.CurrentMap = P.CurrentMap;
	Manifest.UpdatedAt = FDateTime::UtcNow();
	for (const auto& M : P.Modules)
		Manifest.ModuleVersions.Add(M.ModuleName, M.Version);
	if (!Storage.WriteManifestAtomic(P.SaveId, Manifest))
		return Fail(ESaveResultCode::CommitFailed, TEXT("Manifest commit failed"));
	return FSaveOperationResult::Success();
}
