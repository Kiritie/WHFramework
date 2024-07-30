// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "Windows/WindowsHWrapper.h"
#include "Templates/UniquePtr.h"

struct FRegistryValueEx
{
	uint32 Type;
	TArray<uint8> Data;

	FRegistryValueEx();

	void Set(const FString &NewValue);
	void Set(uint32 NewValue);

	bool Read(HKEY hKey, const FString &Name);
	bool Write(HKEY hKey, const FString &Name) const;
	bool IsUpToDate(HKEY hKey, const FString &Name) const;
};

struct FRegistryKeyEx : FNoncopyable
{
	TMap<FString, FRegistryKeyEx*> Keys;
	TMap<FString, FRegistryValueEx*> Values;

	FRegistryKeyEx();
	~FRegistryKeyEx();

	FRegistryKeyEx *FindOrAddKey(const FString &Name);
	FRegistryValueEx *FindOrAddValue(const FString &Name);

	void SetValue(const FString &Name, const FString &Value);
	void SetValue(const FString &Name, uint32 Value);

	bool Read(HKEY hKey);
	bool Read(HKEY hKey, const FString &Path);

	bool Write(HKEY hKey, bool bRemoveDifferences) const;
	bool Write(HKEY hKey, const FString &Path, bool bRemoveDifferences) const;

	bool IsUpToDate(HKEY hKey, bool bRemoveDifferences) const;
	bool IsUpToDate(HKEY hKey, const FString &Path, bool bRemoveDifferences) const;
};

struct FRegistryRootedKeyEx
{
	HKEY hRootKey;
	FString Path;
	TUniquePtr<FRegistryKeyEx> Key;

	FRegistryRootedKeyEx(HKEY hInKeyRoot, const FString &InPath);

	bool Exists() const;

	bool Read();
	bool Write(bool bRemoveDifferences) const;
	bool IsUpToDate(bool bRemoveDifferences) const;
};

bool EnumerateRegistryKeysEx(HKEY hKey, TArray<FString> &OutNames);
bool EnumerateRegistryValuesEx(HKEY hKey, TArray<FString> &OutNames);

