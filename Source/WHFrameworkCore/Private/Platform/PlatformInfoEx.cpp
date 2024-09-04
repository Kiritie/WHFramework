// Copyright Epic Games, Inc. All Rights Reserved.

#include "Platform/PlatformInfoEx.h"

#include "Debug/DebugTypes.h"
#include "Misc/DataDrivenPlatformInfoRegistry.h"
#include "HAL/FileManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"

#define LOCTEXT_NAMESPACE "PlatformInfoEx"

namespace PlatformInfoEx
{
	TArray<FName> AllPlatformGroupNames;
	TArray<FName> AllVanillaPlatformNames;

namespace
{

TArray<FPlatformInfo> AllPlatformInfoArray;

// we don't need any of this without the editor, although we would ideally not even compile this outside of the editor
// @todo platplug: Figure out why this is compiled on target devices
#if WITH_EDITOR || IS_PROGRAM

void BuildPlatformInfo(const FName& InPlatformInfoName, const FName& InTargetPlatformName, const FText& InDisplayName, const EBuildTargetType InPlatformType, const EPlatformFlags::Flags InPlatformFlags, const FPlatformIconPaths& InIconPaths, const FString& InUATCommandLine, const FString& InAutoSDKPath, EPlatformSDKStatus InStatus, const FString& InTutorial, bool InEnabled, FString InBinaryFolderName, FString InIniPlatformName, bool InUsesHostCompiler, bool InUATClosesAfterLaunch, bool InIsConfidential, const FName& InUBTTargetId, const FName& InPlatformGroupName, const FName& InPlatformSubMenu, bool InTargetPlatformCanUseCrashReporter)
{
	FPlatformInfo& PlatformInfoEx = AllPlatformInfoArray.Emplace_GetRef();

	PlatformInfoEx.PlatformInfoName = InPlatformInfoName;
	PlatformInfoEx.TargetPlatformName = InTargetPlatformName;

	// See if this name also contains a flavor
	const FString InPlatformInfoNameString = InPlatformInfoName.ToString();
	{
		int32 UnderscoreLoc;
		if(InPlatformInfoNameString.FindChar(TEXT('_'), UnderscoreLoc))
		{
			PlatformInfoEx.VanillaPlatformName = *InPlatformInfoNameString.Mid(0, UnderscoreLoc);
			PlatformInfoEx.PlatformFlavor = *InPlatformInfoNameString.Mid(UnderscoreLoc + 1);
		}
		else
		{
			PlatformInfoEx.VanillaPlatformName = InPlatformInfoName;
		}
	}

	if (PlatformInfoEx.VanillaPlatformName != NAME_None)
	{
		PlatformInfoEx::AllVanillaPlatformNames.AddUnique(PlatformInfoEx.VanillaPlatformName);
	}

	PlatformInfoEx.DisplayName = InDisplayName;
	PlatformInfoEx.PlatformType = InPlatformType;
	PlatformInfoEx.PlatformFlags = InPlatformFlags;
	PlatformInfoEx.IconPaths = InIconPaths;
	PlatformInfoEx.UATCommandLine = InUATCommandLine;
	PlatformInfoEx.AutoSDKPath = InAutoSDKPath;
	PlatformInfoEx.BinaryFolderName = InBinaryFolderName;
	PlatformInfoEx.IniPlatformName = InIniPlatformName;
	PlatformInfoEx.UBTTargetId = InUBTTargetId;
	PlatformInfoEx.PlatformGroupName = InPlatformGroupName;
	PlatformInfoEx.PlatformSubMenu = InPlatformSubMenu;

	if (InPlatformGroupName != NAME_None)
	{
		PlatformInfoEx::AllPlatformGroupNames.AddUnique(InPlatformGroupName);
	}

	// Generate the icon style names for FEditorStyle
	PlatformInfoEx.IconPaths.NormalStyleName = *FString::Printf(TEXT("Launcher.Platform_%s"), *InPlatformInfoNameString);
	PlatformInfoEx.IconPaths.LargeStyleName  = *FString::Printf(TEXT("Launcher.Platform_%s.Large"), *InPlatformInfoNameString);
	PlatformInfoEx.IconPaths.XLargeStyleName = *FString::Printf(TEXT("Launcher.Platform_%s.XLarge"), *InPlatformInfoNameString);

	// SDK data
	PlatformInfoEx.SDKStatus = InStatus;
	PlatformInfoEx.SDKTutorial = InTutorial;

	// Distribution data
	PlatformInfoEx.bEnabledForUse = InEnabled;
	PlatformInfoEx.bUsesHostCompiler = InUsesHostCompiler;
	PlatformInfoEx.bUATClosesAfterLaunch = InUATClosesAfterLaunch;
	PlatformInfoEx.bIsConfidential = InIsConfidential;
	PlatformInfoEx.bTargetPlatformCanUseCrashReporter = InTargetPlatformCanUseCrashReporter;
}


void BuildHardcodedPlatforms()
{
	// PlatformInfoName									TargetPlatformName			DisplayName														PlatformType			PlatformFlags					IconPaths																																		UATCommandLine										AutoSDKPath			SDKStatus						SDKTutorial																								bEnabledForUse										BinaryFolderName	IniPlatformName		FbUsesHostCompiler		bUATClosesAfterLaunch	bIsConfidential	UBTTargetId (match UBT's UnrealTargetPlatform enum)		PlatformSubMenu		bTargetPlatformCanUseCrashReporter
	BuildPlatformInfo(TEXT("AllDesktop"),				TEXT("AllDesktop"),			LOCTEXT("DesktopTargetPlatDisplay", "Desktop (Win+Mac+Linux)"),	EBuildTargetType::Game,	EPlatformFlags::None,			FPlatformIconPaths(TEXT("Launcher/Desktop/Platform_Desktop_24x"), TEXT("Launcher/Desktop/Platform_Desktop_128x")),					TEXT(""),											TEXT(""),			EPlatformSDKStatus::Unknown,	TEXT(""),																								PLATFORM_WINDOWS /* see note below */,						TEXT(""),			TEXT(""),			false,					true,					false,			TEXT("AllDesktop"),	TEXT("Desktop"),	TEXT(""),	true);
	// Note: For "AllDesktop" bEnabledForUse value, see SProjectTargetPlatformSettings::Construct !!!! IsAvailableOnWindows || IsAvailableOnMac || IsAvailableOnLinux
}

// Gets a string from a section, or empty string if it didn't exist
FString GetSectionString(const FConfigSection& Section, FName Key)
{
	// look for a value prefixed with host:
	FName HostKey = *FString::Printf(TEXT("%s:%s"), ANSI_TO_TCHAR(FPlatformProperties::IniPlatformName()), *Key.ToString());
	const FConfigValue* HostValue = Section.Find(HostKey);
	return HostValue ? HostValue->GetValue() : Section.FindRef(Key).GetValue();
}

// Gets a bool from a section, or false if it didn't exist
bool GetSectionBool(const FConfigSection& Section, FName Key)
{
	return FCString::ToBool(*GetSectionString(Section, Key));
}

EPlatformFlags::Flags ConvertPlatformFlags(const FString& String)
{
	if (String == TEXT("") == 0 || String == TEXT("None")) { return EPlatformFlags::None; }
	if (String == TEXT("CookFlavor")) { return EPlatformFlags::CookFlavor; }
	if (String == TEXT("BuildFlavor")) { return EPlatformFlags::BuildFlavor; }

	UE_LOG(LogInit, Fatal, TEXT("Unknown platform flag %s in PlatformInfoEx"), *String);
	return EPlatformFlags::None;
}

namespace EPlatformSection
{
	const FName TargetPlatformName = FName(TEXT("TargetPlatformName"));
	const FName DisplayName = FName(TEXT("DisplayName"));
	const FName PlatformType = FName(TEXT("PlatformType"));
	const FName PlatformFlags = FName(TEXT("PlatformFlags"));
	const FName NormalIconPath = FName(TEXT("NormalIconPath"));
	const FName LargeIconPath = FName(TEXT("LargeIconPath"));
	const FName XLargeIconPath = FName(TEXT("XLargeIconPath"));
	const FName UATCommandLine = FName(TEXT("UATCommandLine"));
	const FName AutoSDKPath = FName(TEXT("AutoSDKPath"));
	const FName TutorialPath = FName(TEXT("TutorialPath"));
	const FName bIsEnabled = FName(TEXT("bIsEnabled"));
	const FName BinariesDirectoryName = FName(TEXT("BinariesDirectoryName"));
	const FName IniPlatformName = FName(TEXT("IniPlatformName"));
	const FName bUsesHostCompiler = FName(TEXT("bUsesHostCompiler"));
	const FName bUATClosesAfterLaunch = FName(TEXT("bUATClosesAfterLaunch"));
	const FName bIsConfidential = FName(TEXT("bIsConfidential"));
	const FName UBTTargetID = FName(TEXT("UBTTargetID"));
	const FName PlatformGroupName = FName(TEXT("PlatformGroupName"));
	const FName PlatformSubMenu = FName(TEXT("PlatformSubMenu"));
}

void ParseDataDrivenPlatformInfo(const TCHAR* Name, const FConfigSection& Section)
{
	FName TargetPlatformName = *GetSectionString(Section, EPlatformSection::TargetPlatformName);
	FString DisplayName = GetSectionString(Section, EPlatformSection::DisplayName);
	FString PlatformType = GetSectionString(Section, EPlatformSection::PlatformType);
	FString PlatformFlags = GetSectionString(Section, EPlatformSection::PlatformFlags);
	FString NormalIconPath = GetSectionString(Section, EPlatformSection::NormalIconPath);
	FString LargeIconPath = GetSectionString(Section, EPlatformSection::LargeIconPath);
	FString XLargeIconPath = GetSectionString(Section, EPlatformSection::XLargeIconPath);
	// no one has an XLarge path yet, but in case they add one, this will use it
	if (XLargeIconPath == TEXT(""))
	{
		XLargeIconPath = LargeIconPath;
	}
	FString UATCommandLine = GetSectionString(Section, EPlatformSection::UATCommandLine);
	FString AutoSDKPath = GetSectionString(Section, EPlatformSection::AutoSDKPath);
	FString TutorialPath = GetSectionString(Section, EPlatformSection::TutorialPath);
	bool bIsEnabled = GetSectionBool(Section, EPlatformSection::bIsEnabled);
	FString BinariesDirectoryName = GetSectionString(Section, EPlatformSection::BinariesDirectoryName);
	FString IniPlatformName = GetSectionString(Section, EPlatformSection::IniPlatformName);
	bool bUsesHostCompiler = GetSectionBool(Section, EPlatformSection::bUsesHostCompiler);
	bool bUATClosesAfterLaunch = GetSectionBool(Section, EPlatformSection::bUATClosesAfterLaunch);
	bool bIsConfidential = GetSectionBool(Section, EPlatformSection::bIsConfidential);
	FName UBTTargetID = *GetSectionString(Section, EPlatformSection::UBTTargetID);
	FName PlatformGroupName = *GetSectionString(Section, EPlatformSection::PlatformGroupName);
	FName PlatformSubMenu = *GetSectionString(Section, EPlatformSection::PlatformSubMenu);
	FString TargetPlatformCanUseCrashReporterString = GetSectionString(Section, TEXT("bTargetPlatformCanUseCrashReporter"));
	bool bTargetPlatformCanUseCrashReporter = TargetPlatformCanUseCrashReporterString.IsEmpty() ? true : GetSectionBool(Section, TEXT("bTargetPlatformCanUseCrashReporter"));

	EBuildTargetType TargetType;
	if (!LexTryParseString(TargetType, *PlatformType))
	{
		TargetType = EBuildTargetType::Unknown;
	}

	BuildPlatformInfo(Name, TargetPlatformName, FText::FromString(DisplayName), TargetType, ConvertPlatformFlags(PlatformFlags), FPlatformIconPaths(NormalIconPath, LargeIconPath, XLargeIconPath), UATCommandLine,
		AutoSDKPath, PlatformInfoEx::EPlatformSDKStatus::Unknown, TutorialPath, bIsEnabled, BinariesDirectoryName, IniPlatformName, bUsesHostCompiler, bUATClosesAfterLaunch, bIsConfidential, UBTTargetID, PlatformGroupName, PlatformSubMenu, bTargetPlatformCanUseCrashReporter);
}

void LoadDataDrivenPlatforms()
{
	// look for the standard DataDriven ini files
	int32 NumDDInfoFiles = FDataDrivenPlatformInfoRegistry::GetNumDataDrivenIniFiles();
	for (int32 Index = 0; Index < NumDDInfoFiles; Index++)
	{
		FConfigFile IniFile;
		FString PlatformName;

		FDataDrivenPlatformInfoRegistry::LoadDataDrivenIniFile(Index, IniFile, PlatformName);

		// now walk over the file, looking for ShaderPlatformInfo sections
		for (auto Section : IniFile)
		{
			if (Section.Key.StartsWith(TEXT("PlatformInfoEx ")))
			{
				const FString& SectionName = Section.Key;
				ParseDataDrivenPlatformInfo(*SectionName.Mid(13), Section.Value);
			}
		}
	}
}

struct FPlatformInfoAutoInit
{
	FPlatformInfoAutoInit()
	{
		// FCoreDelegates::ConfigReadyForUse.AddLambda([]
		// {
		// 	BuildHardcodedPlatforms();
		// 	LoadDataDrivenPlatforms();
		// });
	}

} GPlatformInfoAutoInit;
#endif

} // anonymous namespace

const FPlatformInfo* FindPlatformInfo(const FName& InPlatformName)
{
	for(const FPlatformInfo& PlatformInfoEx : AllPlatformInfoArray)
	{
		if(PlatformInfoEx.PlatformInfoName == InPlatformName)
		{
			return &PlatformInfoEx;
		}
	}

	WHLog(FString::Printf(TEXT("Unable to find platform info for '%s'"), *InPlatformName.ToString()), EDC_Platform, EDV_Warning);

	return nullptr;
}

const FPlatformInfo* FindVanillaPlatformInfo(const FName& InPlatformName)
{
	const FPlatformInfo* const FoundInfo = FindPlatformInfo(InPlatformName);
	return (FoundInfo) ? (FoundInfo->IsVanilla()) ? FoundInfo : FindPlatformInfo(FoundInfo->VanillaPlatformName) : nullptr;
}

void UpdatePlatformSDKStatus(FString InPlatformName, EPlatformSDKStatus InStatus)
{
	for(const FPlatformInfo& PlatformInfoEx : AllPlatformInfoArray)
	{
		if(PlatformInfoEx.VanillaPlatformName == FName(*InPlatformName))
		{
			const_cast<FPlatformInfo&>(PlatformInfoEx).SDKStatus = InStatus;
		}
	}
}

void UpdatePlatformDisplayName(FString InPlatformName, FText InDisplayName)
{
	for (const FPlatformInfo& PlatformInfoEx : AllPlatformInfoArray)
	{
		if (PlatformInfoEx.TargetPlatformName == FName(*InPlatformName))
		{
			const_cast<FPlatformInfo&>(PlatformInfoEx).DisplayName = InDisplayName;
		}
	}
}

const TArray<FPlatformInfo>& GetPlatformInfoArray()
{
	return AllPlatformInfoArray;
}

TArray<FVanillaPlatformEntryEx> BuildPlatformHierarchy(const EPlatformFilter InFilter)
{
	TArray<FVanillaPlatformEntryEx> VanillaPlatforms;

	// Build up a tree from the platforms we support (vanilla outers, with a list of flavors)
	// PlatformInfoArray should be ordered in such a way that the vanilla platforms always appear before their flavors
	for (const PlatformInfoEx::FPlatformInfo& PlatformInfoEx : AllPlatformInfoArray)
	{
		if(PlatformInfoEx.IsVanilla())
		{
			VanillaPlatforms.Add(FVanillaPlatformEntryEx(&PlatformInfoEx));
		}
		else
		{
			const bool bHasBuildFlavor = !!(PlatformInfoEx.PlatformFlags & EPlatformFlags::BuildFlavor);
			const bool bHasCookFlavor  = !!(PlatformInfoEx.PlatformFlags & EPlatformFlags::CookFlavor);
			
			const bool bValidFlavor = 
				InFilter == EPlatformFilter::All || 
				(InFilter == EPlatformFilter::BuildFlavor && bHasBuildFlavor) || 
				(InFilter == EPlatformFilter::CookFlavor && bHasCookFlavor);

			if(bValidFlavor)
			{
				const FName VanillaPlatformName = PlatformInfoEx.VanillaPlatformName;
				FVanillaPlatformEntryEx* const VanillaEntry = VanillaPlatforms.FindByPredicate([VanillaPlatformName](const FVanillaPlatformEntryEx& Item) -> bool
				{
					return Item.PlatformInfoEx->PlatformInfoName == VanillaPlatformName;
				});
				check(VanillaEntry);
				VanillaEntry->PlatformFlavors.Add(&PlatformInfoEx);
			}
		}
	}

	return VanillaPlatforms;
}

FVanillaPlatformEntryEx BuildPlatformHierarchy(const FName& InPlatformName, const EPlatformFilter InFilter)
{
	FVanillaPlatformEntryEx VanillaPlatformEntry;
	const FPlatformInfo* VanillaPlatformInfo = FindVanillaPlatformInfo(InPlatformName);
	
	if (VanillaPlatformInfo)
	{
		VanillaPlatformEntry.PlatformInfoEx = VanillaPlatformInfo;
		
		for (const PlatformInfoEx::FPlatformInfo& PlatformInfoEx : AllPlatformInfoArray)
		{
			if (!PlatformInfoEx.IsVanilla() && PlatformInfoEx.VanillaPlatformName == VanillaPlatformInfo->PlatformInfoName)
			{
				const bool bHasBuildFlavor = !!(PlatformInfoEx.PlatformFlags & EPlatformFlags::BuildFlavor);
				const bool bHasCookFlavor = !!(PlatformInfoEx.PlatformFlags & EPlatformFlags::CookFlavor);

				const bool bValidFlavor =
					InFilter == EPlatformFilter::All ||
					(InFilter == EPlatformFilter::BuildFlavor && bHasBuildFlavor) ||
					(InFilter == EPlatformFilter::CookFlavor && bHasCookFlavor);

				if (bValidFlavor)
				{
					VanillaPlatformEntry.PlatformFlavors.Add(&PlatformInfoEx);
				}
			}
		}
	}
	
	return VanillaPlatformEntry;
}

const TArray<FName>& GetAllPlatformGroupNames()
{
	return PlatformInfoEx::AllPlatformGroupNames;
}

const TArray<FName>& GetAllVanillaPlatformNames()
{
	return PlatformInfoEx::AllVanillaPlatformNames;
}

} // namespace PlatformInfoEx

#undef LOCTEXT_NAMESPACE
