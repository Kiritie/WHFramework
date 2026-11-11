#pragma once
#include "CoreMinimal.h"
#include "Type/NativeTypeIdentity.h"
#include "WHFrameworkCoreTypes.h"
#include "Main/MainTypes.h"
#include "Main/Base/ManagerBase.h"
#include "Async/Future.h"
#include <atomic>

struct zip;

enum class EZipTaskStatus : uint8
{
	Queued,
	Running,
	Succeeded,
	Failed,
	Cancelled
};

// Cancellation is cooperative: a file or library call already in progress
// finishes before observing the request. A cancelled/failed extraction may
// have written some destination files; callers own reconciliation.
class WHFRAMEWORKCORE_API FZipTask
{
public:
	void Cancel() { bCancelRequested.store(true); }
	EZipTaskStatus GetStatus() const { return Status.load(); }
	bool IsCancellationRequested() const { return bCancelRequested.load(); }
	FString GetFailureMessage() const { return Status.load() == EZipTaskStatus::Failed ? FailureMessage : FString(); }

private:
	friend class FZipManager;
	std::atomic_bool bCancelRequested{false};
	std::atomic<EZipTaskStatus> Status{EZipTaskStatus::Queued};
	FString FailureMessage;
};

class WHFRAMEWORKCORE_API FZipManager : public FManagerBase
{
	GENERATED_MANAGER(FZipManager)

public:
	FZipManager();
	FZipManager(const FZipManager&) = delete;
	FZipManager& operator=(const FZipManager&) = delete;

	virtual ~FZipManager() override;
	virtual void OnTermination() override;
	
	static const FUniqueType Type;

public:
    // Starts work on the thread pool. Status becomes final only after native
    // file/archive handles are closed; manager shutdown cancels and joins work.
    TSharedPtr<FZipTask, ESPMode::ThreadSafe> StartPack(const FString& InZipFile, const FString& InOriginDirectory, const FString& InIgnoreFile = TEXT(""));

    TSharedPtr<FZipTask, ESPMode::ThreadSafe> StartUnpack(const FString& InZipFile, const FString& InDestDirectory);

private:
	struct FRunningTask
	{
		TSharedPtr<FZipTask, ESPMode::ThreadSafe> Handle;
		TFuture<void> Future;
		FString Destination;
	};
	TArray<FRunningTask> RunningTasks;
	bool bStopping = false;
	void RetireCompletedTasks();
	static bool Pack(const FString& InZipFile, const FString& InOriginDirectory, const FString& InIgnoreFile, FZipTask& InTask, bool& bOutCancelled);
	static bool Pack(zip* InArchive, const FString& InDirectory, const FString& InRelativePath, const FString& InIgnoreFile, FZipTask& InTask, bool& bOutCancelled);
	static bool Unpack(const FString& InZipFile, const FString& InDestDirectory, FZipTask& InTask, bool& bOutCancelled);

public:
	static bool IsZip(const FString& InZipFile);
	
	static bool IsUTF8(const char* InChars);

	static FString ANSIToUTF8ToFString(const char* InChars);
};
