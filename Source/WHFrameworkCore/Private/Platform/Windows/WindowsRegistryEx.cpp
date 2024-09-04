// Copyright Epic Games, Inc. All Rights Reserved.

#include "Platform/Windows/WindowsRegistryEx.h"

#include "Windows/AllowWindowsPlatformTypes.h"

#if WINVER == 0x0502
	// Some registry functions are not available on windows XP, so we need the XP compat alternative
	#include <Shlwapi.h>
#endif

////////////////////////////////////////////////////////////////////

FRegistryValueEx::FRegistryValueEx()
{
	Type = 0;
}

void FRegistryValueEx::Set(const FString &NewValue)
{
	Type = REG_SZ;
	Data.Reset();
	Data.Append((const uint8*)*NewValue, (NewValue.Len() + 1) * sizeof(TCHAR));
}

void FRegistryValueEx::Set(uint32 NewValue)
{
	Type = REG_DWORD;
	Data.Reset();
	Data.Append((const uint8*)&NewValue, sizeof(uint32));
}

bool FRegistryValueEx::Read(HKEY hKey, const FString &Name)
{
	// Read the size and type
	DWORD ValueType;
	DWORD ValueDataLength = 0;
	if (RegQueryValueEx(hKey, *Name, NULL, &ValueType, NULL, &ValueDataLength) != ERROR_SUCCESS)
	{
		return false;
	}
	Type = (uint32)ValueType;

	// Read the value data
	Data.SetNum(ValueDataLength);
	if (RegQueryValueEx(hKey, *Name, NULL, NULL, Data.GetData(), &ValueDataLength) != ERROR_SUCCESS || ValueDataLength != Data.Num())
	{
		return false;
	}

	// Otherwise success
	return true;
}

bool FRegistryValueEx::Write(HKEY hKey, const FString &Name) const
{
	return (RegSetValueEx(hKey, *Name, 0, Type, Data.GetData(), (DWORD)Data.Num()) == ERROR_SUCCESS);
}

bool FRegistryValueEx::IsUpToDate(HKEY hKey, const FString &Name) const
{
	FRegistryValueEx Other;
	return Other.Read(hKey, Name) && Other.Type == Type && Other.Data == Data;
}

////////////////////////////////////////////////////////////////////

FRegistryKeyEx::FRegistryKeyEx()
{
}

FRegistryKeyEx::~FRegistryKeyEx()
{
	for (TMap<FString, FRegistryKeyEx*>::TIterator Iter(Keys); Iter; ++Iter)
	{
		delete Iter.Value();
	}
	for (TMap<FString, FRegistryValueEx*>::TIterator Iter(Values); Iter; ++Iter)
	{
		delete Iter.Value();
	}
}

FRegistryKeyEx *FRegistryKeyEx::FindOrAddKey(const FString &Name)
{
	FRegistryKeyEx *&Key = Keys.FindOrAdd(Name);
	if (Key == NULL)
	{
		Key = new FRegistryKeyEx();
	}
	return Key;
}

FRegistryValueEx *FRegistryKeyEx::FindOrAddValue(const FString &Name)
{
	FRegistryValueEx *&Value = Values.FindOrAdd(Name);
	if (Value == NULL)
	{
		Value = new FRegistryValueEx();
	}
	return Value;
}

void FRegistryKeyEx::SetValue(const FString &Name, const FString &NewValue)
{
	FindOrAddValue(Name)->Set(NewValue);
}

void FRegistryKeyEx::SetValue(const FString &Name, uint32 NewValue)
{
	FindOrAddValue(Name)->Set(NewValue);
}

bool FRegistryKeyEx::Read(HKEY hKey)
{
	// Enumerate all the child key names
	TArray<FString> KeyNames;
	EnumerateRegistryKeysEx(hKey, KeyNames);

	// Read all the child keys
	for (int32 Idx = 0; Idx < KeyNames.Num(); Idx++)
	{
		FRegistryKeyEx *Key = FindOrAddKey(KeyNames[Idx]);
		if (!Key->Read(hKey, KeyNames[Idx]))
		{
			return false;
		}
	}

	// Enumerate all the child values
	TArray<FString> ValueNames;
	EnumerateRegistryValuesEx(hKey, ValueNames);

	// Read all the values
	for (int32 Idx = 0; Idx < ValueNames.Num(); Idx++)
	{
		FRegistryValueEx *Value = FindOrAddValue(ValueNames[Idx]);
		if (!Value->Read(hKey, ValueNames[Idx]))
		{
			return false;
		}
	}

	return true;
}

bool FRegistryKeyEx::Read(HKEY hRootKey, const FString &Path)
{
	bool bRes = false;

	HKEY hKey;
	if (RegOpenKeyEx(hRootKey, *Path, 0, KEY_READ, &hKey) == ERROR_SUCCESS)
	{
		bRes = Read(hKey);
		RegCloseKey(hKey);
	}

	return bRes;
}

bool FRegistryKeyEx::Write(HKEY hKey, bool bRemoveDifferences) const
{
	// Remove all the differences from the current content
	if (bRemoveDifferences)
	{
		// Get all the existing value names
		TArray<FString> ValueNames;
		EnumerateRegistryValuesEx(hKey, ValueNames);

		// Remove any values that don't exist any more
		for (int32 Idx = 0; Idx < ValueNames.Num(); Idx++)
		{
			if (!Values.Contains(ValueNames[Idx]) && RegDeleteValue(hKey, *ValueNames[Idx]) != ERROR_SUCCESS)
			{
				return false;
			}
		}

		// Get all the existing key names
		TArray<FString> KeyNames;
		EnumerateRegistryKeysEx(hKey, KeyNames);

		// Remove any keys that don't exist any more
		for (int32 Idx = 0; Idx < KeyNames.Num(); Idx++)
		{
#if WINVER == 0x0502
			if (!Keys.Contains(KeyNames[Idx]) && SHDeleteKey(hKey, *KeyNames[Idx]) != ERROR_SUCCESS)
#else

			if (!Keys.Contains(KeyNames[Idx]) && RegDeleteTree(hKey, *KeyNames[Idx]) != ERROR_SUCCESS)
#endif // WINVER == 0x0502
			{
				return false;
			}
		}
	}

	// Write all the child keys
	for (TMap<FString, FRegistryKeyEx*>::TConstIterator ChildKeyIter(Keys); ChildKeyIter; ++ChildKeyIter)
	{
		if (!ChildKeyIter.Value()->Write(hKey, ChildKeyIter.Key(), bRemoveDifferences))
		{
			return false;
		}
	}

	// Write all the child values
	for (TMap<FString, FRegistryValueEx*>::TConstIterator ChildValueIter(Values); ChildValueIter; ++ChildValueIter)
	{
		if (RegSetValueEx(hKey, *ChildValueIter.Key(), 0, ChildValueIter.Value()->Type, ChildValueIter.Value()->Data.GetData(), (DWORD)ChildValueIter.Value()->Data.Num()) != ERROR_SUCCESS)
		{
			return false;
		}
	}
	return true;
}

bool FRegistryKeyEx::Write(HKEY hRootKey, const FString &Path, bool bRemoveDifferences) const
{
	bool bRes = false;

	HKEY hKey;
	if (RegCreateKeyEx(hRootKey, *Path, 0, NULL, 0, KEY_READ | KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS)
	{
		bRes = Write(hKey, bRemoveDifferences);
		RegCloseKey(hKey);
	}

	return bRes;
}

bool FRegistryKeyEx::IsUpToDate(HKEY hKey, bool bRemoveDifferences) const
{
	// Remove all the differences from the current content
	if (bRemoveDifferences)
	{
		// Get all the existing value names
		TArray<FString> ValueNames;
		EnumerateRegistryValuesEx(hKey, ValueNames);

		// Check there aren't any extra values
		for (int32 Idx = 0; Idx < ValueNames.Num(); Idx++)
		{
			if (!Values.Contains(ValueNames[Idx]))
			{
				return false;
			}
		}

		// Get all the existing key names
		TArray<FString> KeyNames;
		EnumerateRegistryKeysEx(hKey, KeyNames);

		// Check there aren't any extra keys
		for (int32 Idx = 0; Idx < KeyNames.Num(); Idx++)
		{
			if (!Keys.Contains(KeyNames[Idx]))
			{
				return false;
			}
		}
	}

	// Write all the child keys
	for (TMap<FString, FRegistryKeyEx*>::TConstIterator Iter(Keys); Iter; ++Iter)
	{
		if (!Iter.Value()->IsUpToDate(hKey, Iter.Key(), bRemoveDifferences))
		{
			return false;
		}
	}

	// Write all the child values
	for (TMap<FString, FRegistryValueEx*>::TConstIterator Iter(Values); Iter; ++Iter)
	{
		if (!Iter.Value()->IsUpToDate(hKey, Iter.Key()))
		{
			return false;
		}
	}
	return true;
}

bool FRegistryKeyEx::IsUpToDate(HKEY hRootKey, const FString &Path, bool bRemoveDifferences) const
{
	bool bRes = false;

	HKEY hKey;
	if (RegOpenKeyEx(hRootKey, *Path, 0, KEY_READ, &hKey) == ERROR_SUCCESS)
	{
		bRes = IsUpToDate(hKey, bRemoveDifferences);
		RegCloseKey(hKey);
	}

	return bRes;
}

////////////////////////////////////////////////////////////////////

FRegistryRootedKeyEx::FRegistryRootedKeyEx(HKEY hInRootKey, const FString &InPath)
{
	hRootKey = hInRootKey;
	Path = InPath;
}

bool FRegistryRootedKeyEx::Exists() const
{
	bool bRes = false;

	HKEY hKey;
	if (RegOpenKeyEx(hRootKey, *Path, 0, KEY_READ, &hKey) == ERROR_SUCCESS)
	{
		RegCloseKey(hKey);
		bRes = true;
	}

	return bRes;
}

bool FRegistryRootedKeyEx::Write(bool bRemoveDifferences) const
{
	bool bRes = false;
	if (Key)
	{
		bRes = Key->Write(hRootKey, Path, bRemoveDifferences);
	}
	else
	{
		bRes = (!Exists() || RegDeleteKeyEx(hRootKey, *Path, 0, 0) == ERROR_SUCCESS);
	}
	return bRes;
}

bool FRegistryRootedKeyEx::IsUpToDate(bool bRemoveDifferences) const
{
	bool bRes = false;
	if (Key)
	{
		bRes = Key->IsUpToDate(hRootKey, Path, bRemoveDifferences);
	}
	else
	{
		bRes = !Exists();
	}
	return bRes;
}

////////////////////////////////////////////////////////////////////

bool EnumerateRegistryKeysEx(HKEY hKey, TArray<FString> &OutNames)
{
	for (DWORD Index = 0;; Index++)
	{
		// Query the next key name
		TCHAR KeyName[256];
		DWORD KeyNameLength = sizeof(KeyName) / sizeof(KeyName[0]);

		LONG Result = RegEnumKeyEx(hKey, Index, KeyName, &KeyNameLength, NULL, NULL, NULL, NULL);
		if (Result == ERROR_NO_MORE_ITEMS)
		{
			break;
		}
		else if (Result != ERROR_SUCCESS)
		{
			return false;
		}

		// Add it to the array
		OutNames.Add(KeyName);
	}
	return true;
}

bool EnumerateRegistryValuesEx(HKEY hKey, TArray<FString> &OutNames)
{
	for (DWORD Index = 0;; Index++)
	{
		// Query the value
		wchar_t ValueName[256];
		DWORD ValueNameLength = sizeof(ValueName) / sizeof(ValueName[0]);

		LONG Result = RegEnumValue(hKey, Index, ValueName, &ValueNameLength, NULL, NULL, NULL, NULL);
		if (Result == ERROR_NO_MORE_ITEMS)
		{
			break;
		}
		else if (Result != ERROR_SUCCESS)
		{
			return false;
		}

		// Add it to the array
		OutNames.Add(ValueName);
	}
	return true;
}

#include "Windows/HideWindowsPlatformTypes.h"
