#pragma once

#include "CoreMinimal.h"

struct FSaveManifest;

class WHFRAMEWORK_API FSaveGameStorage
{
public:
	explicit FSaveGameStorage(int32 InUserIndex = 0);

	FString GetRootDir() const;
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
	bool WriteBinaryAtomic(const FString& Path, const TArray<uint8>& Bytes);
	bool ReadBinary(const FString& Path, TArray<uint8>& OutBytes) const;
	bool WriteTextAtomic(const FString& Path, const FString& Text);
	bool PrepareTempGeneration(const FGuid& SaveId, int32 PreviousGeneration, int32 NewGeneration);
	bool CommitGeneration(const FGuid& SaveId, int32 Generation);
	void CleanupUncommittedGenerations(const FGuid& SaveId, int32 CurrentGeneration);
	void CleanupAllTempGenerations();
	bool CopyCurrentGeneration(const FGuid& Source, const FGuid& Target, int32 SourceGeneration, int32 TargetGeneration);
	void CleanupTempGenerations(const FGuid& SaveId);
	bool IsGenerationComplete(const FGuid& SaveId, int32 Generation, const FSaveManifest& Manifest) const;
	int32 RecoverLatestCompleteGeneration(const FGuid& SaveId, const FSaveManifest& Manifest) const;

private:
	int32 UserIndex = 0;
};
