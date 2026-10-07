#pragma once

#include "CoreMinimal.h"
#include "Containers/ArrayView.h"
#include "SaveGame/SaveGameGenerationLease.h"

struct FSaveManifest;

class WHFRAMEWORK_API FSaveGameStorage
{
public:
	explicit FSaveGameStorage(int32 InUserIndex = 0, const FString& InRootDirectory = FString(),
		const FGuid& InTransactionId = FGuid());
	ESaveWorldWriteResult BeginWorldWrite(const FGuid& SaveId,
		const TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe>& InResources,
		const FVoxelGenerationConsumerKey& InConsumer);
	void EndWorldWrite();

	const FString& GetRootDir() const;
	FString GetProfilesDir() const;
	FString GetProfileDir(FName ProfileName) const;
	FString GetProfileModuleFilePath(FName ProfileName, FName ModuleName) const;
	FString GetWorldsDir() const;
	FString GetWorldDir(const FGuid& SaveId) const;
	FString GetManifestPath(const FGuid& SaveId) const;
	FString GetGenerationsDir(const FGuid& SaveId) const;
	FString GetGenerationDir(const FGuid& SaveId, int32 Generation) const;
	FString GetTempGenerationDir(const FGuid& SaveId, int32 Generation) const;
	FString GetModuleFilePath(const FGuid& SaveId, int32 Generation, FName ModuleName) const;
	FString GetTempModuleFilePath(const FGuid& SaveId, int32 Generation, FName ModuleName) const;
	FString GetLastActiveSavePath() const;

	bool EnsureRoot();
	bool CreateWorldDirectory(const FGuid& SaveId);
	bool DeleteWorldDirectory(const FGuid& SaveId);
	bool ReadManifest(const FGuid& SaveId, FSaveManifest& OutManifest) const;
	bool WriteManifestAtomic(const FGuid& SaveId, const FSaveManifest& Manifest);
	bool EnumerateManifests(TArray<FSaveManifest>& OutManifests) const;
	bool ReadLastActiveSave(FGuid& OutSaveId) const;
	bool WriteLastActiveSave(const FGuid& SaveId);
	void ClearLastActiveSave();
	bool WriteBinary(const FString& Path, const TArray<uint8>& Bytes);
	static bool WriteBinaryDurable(const FString& Path, TConstArrayView<uint8> Bytes);
	bool WriteBinaryAtomic(const FString& Path, const TArray<uint8>& Bytes);
	bool ReadBinary(const FString& Path, TArray<uint8>& OutBytes) const;
	bool WriteTextAtomic(const FString& Path, const FString& Text);
	bool PrepareTempGeneration(const FGuid& SaveId, int32 PreviousGeneration, int32 NewGeneration,
		const FString* InCapturedVoxelGeneration = nullptr,
		const TSharedPtr<const FSaveGameGenerationReadLease, ESPMode::ThreadSafe>* InCapturedSourceLease = nullptr);
	bool FlushTempGeneration(const FGuid& SaveId, int32 Generation);
	bool CommitGeneration(const FGuid& SaveId, int32 Generation);
	bool CleanupUncommittedGenerations(const FGuid& SaveId, int32 CurrentGeneration);
	void CleanupAllTempGenerations();
	bool CopyCurrentGeneration(const FGuid& Source, const FGuid& Target, int32 SourceGeneration, int32 TargetGeneration);
	bool CleanupTempGenerations(const FGuid& SaveId);
	bool IsGenerationComplete(const FGuid& SaveId, int32 Generation, const FSaveManifest& Manifest) const;
	int32 RecoverLatestCompleteGeneration(const FGuid& SaveId, const FSaveManifest& Manifest) const;

private:
	ESaveWorldWriteResult AcquireWorldWrite(const FGuid& SaveId,
		TSharedPtr<const FSaveGameGenerationWriteLease, ESPMode::ThreadSafe>& OutWriter);
	TSharedPtr<const FSaveGameGenerationWriteLease, ESPMode::ThreadSafe> ActiveWorldWrite;
	int32 UserIndex = 0;
	FString RootDirectory;
	FGuid WriteTransactionId;
};
