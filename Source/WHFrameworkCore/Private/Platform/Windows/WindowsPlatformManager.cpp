// Fill out your copyright notice in the Description page of Project Settings.

#include "Platform/Windows/WindowsPlatformManager.h"

#include "GeneralProjectSettings.h"
#include "WHFrameworkCoreTypes.h"
#include "Main/MainManager.h"
#include "WHFrameworkCoreStatics.h"
#if PLATFORM_WINDOWS
#include <activation.h>
#include <shellapi.h>
#include "Windows/MinWindows.h"
#endif
#include<windows.h>
#include <string>
#include <shellapi.h>
#include <shlobj.h>

#include "Debug/DebugTypes.h"
#pragma  comment(lib, "shell32.lib")
#ifdef _UNICODE
typedef wstring tstring;
#else
typedef string tstring;
#endif

#include <iostream>
#include <string>
#include <tlhelp32.h>
#include <vector>
#include <tchar.h>
#include "Misc/FeedbackContextMarkup.h"
#include "WindowsNativeFeedbackContextEx.h"
#include "Windows/COMPointer.h"
#include "Misc/Paths.h"
#include "Misc/Guid.h"
#include "HAL/FileManager.h"

#include "Windows/AllowWindowsPlatformTypes.h"
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>
#include <Winver.h>
#include <LM.h>
#include <tlhelp32.h>
#include <Psapi.h>
#include "Windows/HideWindowsPlatformTypes.h"

//////////////////////////////////////////////////////////////////////////
// FExampleHandler
NOTIFYICONDATA nid;

#define NOTIFY_SHOW WM_USER+2500
#define IDR_MAINFRAME WM_USER +2501

FExampleHandler::FExampleHandler()
{
}

FExampleHandler::~FExampleHandler()
{
}

bool FExampleHandler::ProcessMessage(HWND Hwnd, uint32 Message, WPARAM WParam, LPARAM LParam, int32& OutResult)
{
	// log out some details for the received message
	// GLog->Logf(TEXT("WindowsMessageHandlerExampleModule: hwnd = %i, msg = %s, wParam = %i, lParam = %i"), Hwnd, *GetMessageName(Message), WParam, LParam);

	switch (Message)
	{
		case NOTIFY_SHOW:
		{
			switch (LParam)
			{
				case WM_RBUTTONUP:
				{
					AddTrayMenu(Hwnd);
					break;
				}
				case WM_LBUTTONDBLCLK:
				{
#if WITH_ENGINE
					GEngine->GameViewport->GetWindow()->ShowWindow();
					GEngine->GameViewport->GetWindow()->BringToFront();
#endif
					break;
				}							
			}
			break;
		}
		case WM_COPYDATA:
		{
			COPYDATASTRUCT* CopyData = (COPYDATASTRUCT*)LParam;
			if (CopyData->dwData == (WM_USER + 100))//Sender发来的消息
			{
				FString CustomMessage = (char*)CopyData->lpData;
				if(OnProcessMessage.IsBound())
				{
					OnProcessMessage.Broadcast(CustomMessage);
				}
			}
			break;
		}
		//...
	}
	
	// we did not handle this message, so make sure it gets passed on to other handlers
	return false;
}

bool FExampleHandler::CreateTrayIcon()
{
	HWND hWnd = GetActiveWindow();
	nid.hWnd = hWnd;

	// 图标路径是package之后,主目录
	const FString iconPath = FString(FPlatformProcess::ExecutablePath()).Replace(TEXT(".exe"), TEXT(".ico"));
	WHLog(FString::Printf(TEXT(">>> FPATHS : %s"), *iconPath), EDC_Platform, EDV_Warning);
	HINSTANCE hInst = NULL;
	HICON hIcon = (HICON)LoadImage(hInst, *(iconPath), IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR | LR_CREATEDIBSECTION | LR_LOADFROMFILE);

	const uint32 Error = GetLastError();
	GLog->Logf(TEXT("%d"), Error);

	nid.cbSize = sizeof(NOTIFYICONDATA); 
	nid.uID = IDR_MAINFRAME;
	nid.hIcon = hIcon;
	nid.hWnd = hWnd;
	nid.uCallbackMessage = NOTIFY_SHOW;
	nid.uVersion = NOTIFYICON_VERSION;
	nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_INFO;

	const UGeneralProjectSettings* ProjectSettings = GetDefault<UGeneralProjectSettings>();

	lstrcpy(nid.szTip, *FString::Printf(TEXT("%s %s"), *ProjectSettings->ProjectName, *ProjectSettings->ProjectVersion)); //szAppClassName

	WHLog(TEXT(">>> Shell_NotifyIcon : "), EDC_Platform, EDV_Warning);

	return Shell_NotifyIcon(NIM_ADD, &nid);
}

bool FExampleHandler::DeleteTrayIcon()
{
	return Shell_NotifyIcon(NIM_DELETE, &nid);
}

void FExampleHandler::AddTrayMenu(HWND Hwnd)
{
	POINT pt;

	GetCursorPos(&pt);
	HMENU menu = CreatePopupMenu();

	// show window
	AppendMenu(menu, MF_STRING, WM_SHOWWINDOW, L"显示主窗口");

	// quit
	AppendMenu(menu, MF_STRING, WM_DESTROY, L"退出");

	::SetForegroundWindow(Hwnd);
	int32 MenuID = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_LEFTBUTTON, pt.x, pt.y, NULL, Hwnd, NULL);

	GLog->Logf(TEXT("%d"), MenuID);

	if (MenuID == WM_SHOWWINDOW)
	{
		WHLog(TEXT(">>> WM_SHOWWINDOW : "), EDC_Platform, EDV_Warning);

#if WITH_ENGINE
		GEngine->GameViewport->GetWindow()->ShowWindow();
		GEngine->GameViewport->GetWindow()->BringToFront();
#endif
	}
	else if (MenuID == WM_DESTROY)
	{
		WHLog(TEXT(">>> WM_DESTROY : "), EDC_Platform, EDV_Warning);
		// 退出
		FGenericPlatformMisc::RequestExit(false);
	}

	DestroyMenu(menu);
}

//////////////////////////////////////////////////////////////////////////
// FWindowsPlatformManager
const FUniqueType FWindowsPlatformManager::Type = FPlatformManager::Type;

IMPLEMENTATION_MANAGER(FWindowsPlatformManager)

// Sets default values
FWindowsPlatformManager::FWindowsPlatformManager()
{
}

FWindowsPlatformManager::~FWindowsPlatformManager()
{
}

void FWindowsPlatformManager::OnInitialize()
{
	FManagerBase::OnInitialize();

	// register our handler
	if (FWindowsApplication* Application = GetApplication())
	{
		Application->AddMessageHandler(Handler);
	}

	bool bAutoRun = false;
	if(FParse::Bool(FCommandLine::Get(), TEXT("SetAppAutoRun"), bAutoRun))
	{
		SetAppAutoRun(bAutoRun);
	}

	// if(FParse::Param(FCommandLine::Get(), TEXT("AutoExit")))
	// {
	// 	FGenericPlatformMisc::RequestExit(false);
	// }
}

void FWindowsPlatformManager::OnTermination()
{
	FManagerBase::OnTermination();

	// unregister our handler
	if (FWindowsApplication* Application = GetApplication())
	{
		Application->RemoveMessageHandler(Handler);
	}
}

#pragma comment( lib, "version.lib" )

#define LOCTEXT_NAMESPACE "PlatformManager"
#define MAX_FILETYPES_STR 4096
#define MAX_FILENAME_STR 65536 // This buffer has to be big enough to contain the names of all the selected files as well as the null characters between them and the null character at the end

static const TCHAR *InstallationsSubKey = TEXT("SOFTWARE\\Epic Games\\Unreal Engine\\Builds");

bool FWindowsPlatformManager::OpenFileDialog( const void* ParentWindowHandle, const FString& DialogTitle, const FString& DefaultPath, const FString& DefaultFile, const FString& FileTypes, uint32 Flags, TArray<FString>& OutFilenames, int32& OutFilterIndex )
{
	return FileDialogShared(false, ParentWindowHandle, DialogTitle, DefaultPath, DefaultFile, FileTypes, Flags, OutFilenames, OutFilterIndex );
}

bool FWindowsPlatformManager::OpenFileDialog( const void* ParentWindowHandle, const FString& DialogTitle, const FString& DefaultPath, const FString& DefaultFile, const FString& FileTypes, uint32 Flags, TArray<FString>& OutFilenames)
{
	int32 DummyFilterIndex;
	return FileDialogShared(false, ParentWindowHandle, DialogTitle, DefaultPath, DefaultFile, FileTypes, Flags, OutFilenames, DummyFilterIndex );
}

bool FWindowsPlatformManager::SaveFileDialog(const void* ParentWindowHandle, const FString& DialogTitle, const FString& DefaultPath, const FString& DefaultFile, const FString& FileTypes, uint32 Flags, TArray<FString>& OutFilenames)
{
	int32 DummyFilterIndex = 0;
	return FileDialogShared(true, ParentWindowHandle, DialogTitle, DefaultPath, DefaultFile, FileTypes, Flags, OutFilenames, DummyFilterIndex );
}

bool FWindowsPlatformManager::OpenDirectoryDialog(const void* ParentWindowHandle, const FString& DialogTitle, const FString& DefaultPath, FString& OutFolderName)
{
	FScopedSystemModalMode SystemModalScope;

	bool bSuccess = false;

	TComPtr<IFileOpenDialog> FileDialog;
	if (SUCCEEDED(::CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&FileDialog))))
	{
		// Set this up as a folder picker
		{
			DWORD dwFlags = 0;
			FileDialog->GetOptions(&dwFlags);
			FileDialog->SetOptions(dwFlags | FOS_PICKFOLDERS);
		}

		// Set up common settings
		FileDialog->SetTitle(*DialogTitle);
		if (!DefaultPath.IsEmpty())
		{
			// SHCreateItemFromParsingName requires the given path be absolute and use \ rather than / as our normalized paths do
			FString DefaultWindowsPath = FPaths::ConvertRelativePathToFull(DefaultPath);
			DefaultWindowsPath.ReplaceInline(TEXT("/"), TEXT("\\"), ESearchCase::CaseSensitive);

			TComPtr<IShellItem> DefaultPathItem;
			if (SUCCEEDED(::SHCreateItemFromParsingName(*DefaultWindowsPath, nullptr, IID_PPV_ARGS(&DefaultPathItem))))
			{
				FileDialog->SetFolder(DefaultPathItem);
			}
		}

		// Show the picker
		if (SUCCEEDED(FileDialog->Show((HWND)ParentWindowHandle)))
		{
			TComPtr<IShellItem> Result;
			if (SUCCEEDED(FileDialog->GetResult(&Result)))
			{
				PWSTR pFilePath = nullptr;
				if (SUCCEEDED(Result->GetDisplayName(SIGDN_FILESYSPATH, &pFilePath)))
				{
					bSuccess = true;

					OutFolderName = pFilePath;
					FPaths::NormalizeDirectoryName(OutFolderName);

					::CoTaskMemFree(pFilePath);
				}
			}
		}
	}
	
	return bSuccess;
}

bool FWindowsPlatformManager::OpenFontDialog(const void* ParentWindowHandle, FString& OutFontName, float& OutHeight, EFontImportFlags& OutFlags)
{	
	FScopedSystemModalMode SystemModalScope;

	CHOOSEFONT cf;
	ZeroMemory(&cf, sizeof(CHOOSEFONT));

	LOGFONT lf;
	ZeroMemory(&lf, sizeof(LOGFONT));

	cf.lStructSize = sizeof(CHOOSEFONT);
	cf.hwndOwner = (HWND)ParentWindowHandle;
	cf.lpLogFont = &lf;
	cf.Flags = CF_EFFECTS | CF_SCREENFONTS;
	bool bSuccess = !!::ChooseFont(&cf);
	if ( bSuccess )
	{
		HDC DC = ::GetDC( cf.hwndOwner ); 
		const float LogicalPixelsY = static_cast<float>(GetDeviceCaps(DC, LOGPIXELSY));
		const int32 PixelHeight = static_cast<int32>(-lf.lfHeight * ( 72.0f / LogicalPixelsY ));	// Always target 72 DPI
		auto FontFlags = EFontImportFlags::None;
		if ( lf.lfUnderline )
		{
			FontFlags |= EFontImportFlags::EnableUnderline;
		}
		if ( lf.lfItalic )
		{
			FontFlags |= EFontImportFlags::EnableItalic;
		}
		if ( lf.lfWeight == FW_BOLD )
		{
			FontFlags |= EFontImportFlags::EnableBold;
		}

		OutFontName = (const TCHAR*)lf.lfFaceName;
		OutHeight = PixelHeight;
		OutFlags = FontFlags;

		::ReleaseDC( cf.hwndOwner, DC ); 
	}
	else
	{
		WHLog(TEXT("Error reading results of font dialog."), EDC_Platform, EDV_Warning);
	}

	return bSuccess;
}

CA_SUPPRESS(6262)

bool FWindowsPlatformManager::FileDialogShared(bool bSave, const void* ParentWindowHandle, const FString& DialogTitle, const FString& DefaultPath, const FString& DefaultFile, const FString& FileTypes, uint32 Flags, TArray<FString>& OutFilenames, int32& OutFilterIndex)
{
	FScopedSystemModalMode SystemModalScope;

	bool bSuccess = false;

	TComPtr<IFileDialog> FileDialog;
	if (SUCCEEDED(::CoCreateInstance(bSave ? CLSID_FileSaveDialog : CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, bSave ? IID_IFileSaveDialog : IID_IFileOpenDialog, IID_PPV_ARGS_Helper(&FileDialog))))
	{
		if (bSave)
		{
			// Set the default "filename"
			if (!DefaultFile.IsEmpty())
			{
				FileDialog->SetFileName(*FPaths::GetCleanFilename(DefaultFile));
			}
		}
		else
		{
			// Set this up as a multi-select picker
			if (Flags & EFileDialogFlags::Multiple)
			{
				DWORD dwFlags = 0;
				FileDialog->GetOptions(&dwFlags);
				FileDialog->SetOptions(dwFlags | FOS_ALLOWMULTISELECT);
			}
		}

		// Set up common settings
		FileDialog->SetTitle(*DialogTitle);
		if (!DefaultPath.IsEmpty())
		{
			// SHCreateItemFromParsingName requires the given path be absolute and use \ rather than / as our normalized paths do
			FString DefaultWindowsPath = FPaths::ConvertRelativePathToFull(DefaultPath);
			DefaultWindowsPath.ReplaceInline(TEXT("/"), TEXT("\\"), ESearchCase::CaseSensitive);

			TComPtr<IShellItem> DefaultPathItem;
			if (SUCCEEDED(::SHCreateItemFromParsingName(*DefaultWindowsPath, nullptr, IID_PPV_ARGS(&DefaultPathItem))))
			{
				FileDialog->SetFolder(DefaultPathItem);
			}
		}

		// Set-up the file type filters
		TArray<FString> UnformattedExtensions;
		TArray<COMDLG_FILTERSPEC> FileDialogFilters;
		{
			// Split the given filter string (formatted as "Pair1String1|Pair1String2|Pair2String1|Pair2String2") into the Windows specific filter struct
			FileTypes.ParseIntoArray(UnformattedExtensions, TEXT("|"), true);

			if (UnformattedExtensions.Num() % 2 == 0)
			{
				FileDialogFilters.Reserve(UnformattedExtensions.Num() / 2);
				for (int32 ExtensionIndex = 0; ExtensionIndex < UnformattedExtensions.Num();)
				{
					COMDLG_FILTERSPEC& NewFilterSpec = FileDialogFilters[FileDialogFilters.AddDefaulted()];
					NewFilterSpec.pszName = *UnformattedExtensions[ExtensionIndex++];
					NewFilterSpec.pszSpec = *UnformattedExtensions[ExtensionIndex++];
				}
			}
		}
		FileDialog->SetFileTypes(FileDialogFilters.Num(), FileDialogFilters.GetData());

		// Show the picker
		if (SUCCEEDED(FileDialog->Show((HWND)ParentWindowHandle)))
		{
			OutFilterIndex = 0;
			if (SUCCEEDED(FileDialog->GetFileTypeIndex((UINT*)&OutFilterIndex)))
			{
				OutFilterIndex -= 1; // GetFileTypeIndex returns a 1-based index
			}

			auto AddOutFilename = [&OutFilenames](const FString& InFilename)
			{
				FString& OutFilename = OutFilenames[OutFilenames.Add(InFilename)];
				OutFilename = IFileManager::Get().ConvertToRelativePath(*OutFilename);
				FPaths::NormalizeFilename(OutFilename);
			};

			if (bSave)
			{
				TComPtr<IShellItem> Result;
				if (SUCCEEDED(FileDialog->GetResult(&Result)))
				{
					PWSTR pFilePath = nullptr;
					if (SUCCEEDED(Result->GetDisplayName(SIGDN_FILESYSPATH, &pFilePath)))
					{
						bSuccess = true;

						// Apply the selected extension if the given filename doesn't already have one
						FString SaveFilePath = pFilePath;
						if (FileDialogFilters.IsValidIndex(OutFilterIndex))
						{
							// May have multiple semi-colon separated extensions in the pattern
							const FString ExtensionPattern = FileDialogFilters[OutFilterIndex].pszSpec;
							TArray<FString> SaveExtensions;
							ExtensionPattern.ParseIntoArray(SaveExtensions, TEXT(";"));

							// Build a "clean" version of the selected extension (without the wildcard)
							FString CleanExtension = SaveExtensions[0];
							if (CleanExtension == TEXT("*.*"))
							{
								CleanExtension.Reset();
							}
							else
							{
								int32 WildCardIndex = INDEX_NONE;
								if (CleanExtension.FindChar(TEXT('*'), WildCardIndex))
								{
									CleanExtension.RightChopInline(WildCardIndex + 1, false);
								}
							}

							// We need to split these before testing the extension to avoid anything within the path being treated as a file extension
							FString SaveFileName = FPaths::GetCleanFilename(SaveFilePath);
							SaveFilePath = FPaths::GetPath(SaveFilePath);

							// Apply the extension if the file name doesn't already have one
							if (FPaths::GetExtension(SaveFileName).IsEmpty() && !CleanExtension.IsEmpty())
							{
								SaveFileName = FPaths::SetExtension(SaveFileName, CleanExtension);
							}

							SaveFilePath /= SaveFileName;
						}
						AddOutFilename(SaveFilePath);

						::CoTaskMemFree(pFilePath);
					}
				}
			}
			else
			{
				IFileOpenDialog* FileOpenDialog = static_cast<IFileOpenDialog*>(FileDialog.Get());

				TComPtr<IShellItemArray> Results;
				if (SUCCEEDED(FileOpenDialog->GetResults(&Results)))
				{
					DWORD NumResults = 0;
					Results->GetCount(&NumResults);
					for (DWORD ResultIndex = 0; ResultIndex < NumResults; ++ResultIndex)
					{
						TComPtr<IShellItem> Result;
						if (SUCCEEDED(Results->GetItemAt(ResultIndex, &Result)))
						{
							PWSTR pFilePath = nullptr;
							if (SUCCEEDED(Result->GetDisplayName(SIGDN_FILESYSPATH, &pFilePath)))
							{
								bSuccess = true;
								AddOutFilename(pFilePath);
								::CoTaskMemFree(pFilePath);
							}
						}
					}
				}
			}
		}
	}

	return bSuccess;
}

bool FWindowsPlatformManager::RegisterEngineInstallation(const FString &RootDir, FString &OutIdentifier)
{
	bool bRes = false;
	if(IsValidRootDirectory(RootDir))
	{
		HKEY hRootKey;
		if(RegCreateKeyEx(HKEY_CURRENT_USER, InstallationsSubKey, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hRootKey, NULL) == ERROR_SUCCESS)
		{
			FString NewIdentifier = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensInBraces);
			LRESULT SetResult = RegSetValueEx(hRootKey, *NewIdentifier, 0, REG_SZ, (const BYTE*)*RootDir, (RootDir.Len() + 1) * sizeof(TCHAR));
			RegCloseKey(hRootKey);

			if(SetResult == ERROR_SUCCESS)
			{
				OutIdentifier = NewIdentifier;
				bRes = true;
			}
		}
	}
	return bRes;
}

void FWindowsPlatformManager::EnumerateEngineInstallations(TMap<FString, FString> &OutInstallations)
{
	// Enumerate the binary installations
	EnumerateLauncherEngineInstallations(OutInstallations);

	// Enumerate the per-user installations
	HKEY hKey;
	if (RegOpenKeyEx(HKEY_CURRENT_USER, InstallationsSubKey, 0, KEY_ALL_ACCESS, &hKey) == ERROR_SUCCESS)
	{
		// Get a list of all the directories
		TArray<FString> UniqueDirectories;
		OutInstallations.GenerateValueArray(UniqueDirectories);

		// Enumerate all the installations
		TArray<FString> InvalidKeys;
		for (::DWORD Index = 0;; Index++)
		{
			TCHAR ValueName[256];
			TCHAR ValueData[MAX_PATH];
			::DWORD ValueType = 0;
			::DWORD ValueNameLength = sizeof(ValueName) / sizeof(ValueName[0]);
			::DWORD ValueDataSize = sizeof(ValueData);

			LRESULT Result = RegEnumValue(hKey, Index, ValueName, &ValueNameLength, NULL, &ValueType, (BYTE*)&ValueData[0], &ValueDataSize);
			if(Result == ERROR_SUCCESS)
			{
				int32 ValueDataLength = ValueDataSize / sizeof(TCHAR);
				if(ValueDataLength > 0 && ValueData[ValueDataLength - 1] == 0) ValueDataLength--;

				FString NormalizedInstalledDirectory(ValueDataLength, ValueData);
				FPaths::NormalizeDirectoryName(NormalizedInstalledDirectory);
				FPaths::CollapseRelativeDirectories(NormalizedInstalledDirectory);

				if(IsValidRootDirectory(NormalizedInstalledDirectory) && !UniqueDirectories.Contains(NormalizedInstalledDirectory))
				{
					OutInstallations.Add(ValueName, NormalizedInstalledDirectory);
					UniqueDirectories.Add(NormalizedInstalledDirectory);
				}
				else
				{
					InvalidKeys.Add(ValueName);
				}
			}
			else if(Result == ERROR_NO_MORE_ITEMS)
			{
				break;
			}
		}

		// Remove all the keys which weren't valid
		for(const FString InvalidKey: InvalidKeys)
		{
			RegDeleteValue(hKey, *InvalidKey);
		}

		RegCloseKey(hKey);
	}
}

bool FWindowsPlatformManager::IsSourceDistribution(const FString &RootDir)
{
	// Check for the existence of a GenerateProjectFiles.bat file. This allows compatibility with the GitHub 4.0 release. Guard it against a check for Build.version to skip it in newer engine versions.
	FString BuildVersionPath = RootDir / TEXT("Engine/Build/Build.version");
	if (!IFileManager::Get().FileExists(*BuildVersionPath))
	{
		FString GenerateProjectFilesPath = RootDir / TEXT("GenerateProjectFiles.bat");
		if (IFileManager::Get().FileSize(*GenerateProjectFilesPath) >= 0)
		{
			return true;
		}
	}

	// Otherwise use the default test
	return FWindowsPlatformManager::IsSourceDistribution(RootDir);
}

bool FWindowsPlatformManager::VerifyFileAssociations()
{
	TIndirectArray<FRegistryRootedKeyEx> Keys;
	GetRequiredRegistrySettings(Keys);

	for (int32 Idx = 0; Idx < Keys.Num(); Idx++)
	{
		if (!Keys[Idx].IsUpToDate(true))
		{
			return false;
		}
	}

	return true;
}

bool FWindowsPlatformManager::UpdateFileAssociations()
{
	TIndirectArray<FRegistryRootedKeyEx> Keys;
	GetRequiredRegistrySettings(Keys);

	for (int32 Idx = 0; Idx < Keys.Num(); Idx++)
	{
		if (!Keys[Idx].Write(true))
		{
			return false;
		}
	}

	return true;
}

bool FWindowsPlatformManager::OpenProject(const FString &ProjectFileName)
{
	// Get the project filename in a native format
	FString PlatformProjectFileName = ProjectFileName;
	FPaths::MakePlatformFilename(PlatformProjectFileName);

	// Always treat projects as an Unreal.ProjectFile, don't allow the user overriding the file association to take effect
	SHELLEXECUTEINFO Info;
	ZeroMemory(&Info, sizeof(Info));
	Info.cbSize = sizeof(Info);
	Info.fMask = SEE_MASK_CLASSNAME;
	Info.lpVerb = TEXT("open");
	Info.nShow = SW_SHOWNORMAL;
	Info.lpFile = *PlatformProjectFileName;
	Info.lpClass = TEXT("Unreal.ProjectFile");
	return ::ShellExecuteExW(&Info) != 0;
}

bool FWindowsPlatformManager::RunUnrealBuildTool(const FText& Description, const FString& RootDir, const FString& Arguments, FFeedbackContext* Warn, int32& OutExitCode)
{
	OutExitCode = 1;

	// Get the path to UBT
	FString UnrealBuildToolPath = RootDir / TEXT("Engine/Binaries/DotNET/UnrealBuildTool.exe");
	if(IFileManager::Get().FileSize(*UnrealBuildToolPath) < 0)
	{
		Warn->Logf(ELogVerbosity::Error, TEXT("Couldn't find UnrealBuildTool at '%s'"), *UnrealBuildToolPath);
		return false;
	}

	// Write the output
	Warn->Logf(TEXT("Running %s %s"), *UnrealBuildToolPath, *Arguments);

	// Spawn UBT
	return FFeedbackContextMarkup::PipeProcessOutput(Description, UnrealBuildToolPath, Arguments, Warn, &OutExitCode) && OutExitCode == 0;
}

bool FWindowsPlatformManager::IsUnrealBuildToolRunning()
{
	FString UBTPath = GetUnrealBuildToolExecutableFilename(FPaths::RootDir());
	FPaths::MakePlatformFilename(UBTPath);

	HANDLE SnapShot = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (SnapShot != INVALID_HANDLE_VALUE)
	{
		PROCESSENTRY32 Entry;
		Entry.dwSize = sizeof(PROCESSENTRY32);

		if (::Process32First(SnapShot, &Entry))
		{
			do
			{
				const FString EntryFullPath = FPlatformProcess::GetApplicationName(Entry.th32ProcessID);
				if (EntryFullPath == UBTPath)
				{
					::CloseHandle(SnapShot);
					return true;
				}
			} while (::Process32Next(SnapShot, &Entry));
		}
	}
	::CloseHandle(SnapShot);
	return false;
}

FFeedbackContext* FWindowsPlatformManager::GetNativeFeedbackContext()
{
	static FWindowsNativeFeedbackContextEx FeedbackContext;
	return &FeedbackContext;
}

FString FWindowsPlatformManager::GetUserTempPath()
{
	return FString(FPlatformProcess::UserTempDir());
}

void FWindowsPlatformManager::GetRequiredRegistrySettings(TIndirectArray<FRegistryRootedKeyEx> &RootedKeys)
{
	// Get the path to VersionSelector.exe. If we're running from UnrealVersionSelector itself, try to stick with the current configuration.
	FString DefaultVersionSelectorName = FPlatformProcess::ExecutableName(false);
	if (!DefaultVersionSelectorName.StartsWith(TEXT("UnrealVersionSelector")))
	{
		DefaultVersionSelectorName = TEXT("UnrealVersionSelector.exe");
	}
	FString ExecutableFileName = FPaths::ConvertRelativePathToFull(FPaths::EngineDir()) / TEXT("Binaries/Win64") / DefaultVersionSelectorName;

	// Defer to UnrealVersionSelector.exe in a launcher installation if it's got the same version number or greater.
	FString InstallDir;
	if (FWindowsPlatformMisc::QueryRegKey(HKEY_LOCAL_MACHINE, TEXT("SOFTWARE\\EpicGames\\Unreal Engine"), TEXT("INSTALLDIR"), InstallDir) && (InstallDir.Len() > 0))
	{
		FString NormalizedInstallDir = InstallDir;
		FPaths::NormalizeDirectoryName(NormalizedInstallDir);

		FString InstalledExecutableFileName = NormalizedInstallDir / FString("Launcher/Engine/Binaries/Win64/UnrealVersionSelector.exe");
		if(GetShellIntegrationVersion(InstalledExecutableFileName) == GetShellIntegrationVersion(ExecutableFileName))
		{
			ExecutableFileName = InstalledExecutableFileName;
		}
	}

	// Get the path to the executable
	FPaths::MakePlatformFilename(ExecutableFileName);
	FString QuotedExecutableFileName = FString(TEXT("\"")) + ExecutableFileName + FString(TEXT("\""));

	// HKCU\SOFTWARE\Classes\.uproject
	FRegistryRootedKeyEx *UserRootExtensionKey = new FRegistryRootedKeyEx(HKEY_CURRENT_USER, TEXT("SOFTWARE\\Classes\\.uproject"));
	RootedKeys.Add(UserRootExtensionKey);

	// HKLM\SOFTWARE\Classes\.uproject
	FRegistryRootedKeyEx *RootExtensionKey = new FRegistryRootedKeyEx(HKEY_LOCAL_MACHINE, TEXT("SOFTWARE\\Classes\\.uproject"));
	RootExtensionKey->Key = MakeUnique<FRegistryKeyEx>();
	RootExtensionKey->Key->SetValue(TEXT(""), TEXT("Unreal.ProjectFile"));
	RootedKeys.Add(RootExtensionKey);

	// HKLM\SOFTWARE\Classes\Unreal.ProjectFile
	FRegistryRootedKeyEx *RootFileTypeKey = new FRegistryRootedKeyEx(HKEY_LOCAL_MACHINE, TEXT("SOFTWARE\\Classes\\Unreal.ProjectFile"));
	RootFileTypeKey->Key = MakeUnique<FRegistryKeyEx>();
	RootFileTypeKey->Key->SetValue(TEXT(""), TEXT("Unreal Engine Project File"));
	RootFileTypeKey->Key->FindOrAddKey(L"DefaultIcon")->SetValue(TEXT(""), QuotedExecutableFileName);
	RootedKeys.Add(RootFileTypeKey);

	// HKLM\SOFTWARE\Classes\Unreal.ProjectFile\shell
	FRegistryKeyEx *ShellKey = RootFileTypeKey->Key->FindOrAddKey(TEXT("shell"));

	// HKLM\SOFTWARE\Classes\Unreal.ProjectFile\shell\open
	FRegistryKeyEx *ShellOpenKey = ShellKey->FindOrAddKey(TEXT("open"));
	ShellOpenKey->SetValue(L"", L"Open");
	ShellOpenKey->FindOrAddKey(L"command")->SetValue(L"", QuotedExecutableFileName + FString(TEXT(" /editor \"%1\"")));

	// HKLM\SOFTWARE\Classes\Unreal.ProjectFile\shell\run
	FRegistryKeyEx *ShellRunKey = ShellKey->FindOrAddKey(TEXT("run"));
	ShellRunKey->SetValue(TEXT(""), TEXT("Launch game"));
	ShellRunKey->SetValue(TEXT("Icon"), QuotedExecutableFileName);
	ShellRunKey->FindOrAddKey(L"command")->SetValue(TEXT(""), QuotedExecutableFileName + FString(TEXT(" /game \"%1\"")));

	// HKLM\SOFTWARE\Classes\Unreal.ProjectFile\shell\rungenproj
	FRegistryKeyEx *ShellProjectKey = ShellKey->FindOrAddKey(TEXT("rungenproj"));
	ShellProjectKey->SetValue(L"", L"Generate Visual Studio project files");
	ShellProjectKey->SetValue(L"Icon", QuotedExecutableFileName);
	ShellProjectKey->FindOrAddKey(L"command")->SetValue(TEXT(""), QuotedExecutableFileName + FString(TEXT(" /projectfiles \"%1\"")));

	// HKLM\SOFTWARE\Classes\Unreal.ProjectFile\shell\switchversion
	FRegistryKeyEx *ShellVersionKey = ShellKey->FindOrAddKey(TEXT("switchversion"));
	ShellVersionKey->SetValue(TEXT(""), TEXT("Switch Unreal Engine version..."));
	ShellVersionKey->SetValue(TEXT("Icon"), QuotedExecutableFileName);
	ShellVersionKey->FindOrAddKey(L"command")->SetValue(TEXT(""), QuotedExecutableFileName + FString(TEXT(" /switchversion \"%1\"")));

	// If the user has manually selected something other than our extension, we need to delete it. Explorer explicitly disables set access on that keys in that folder, but we can delete the whole thing.
	const TCHAR* UserChoicePath = TEXT("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\.uproject\\UserChoice");

	// Figure out whether we need to delete it. If it's already set to our own ProgId, leave it alone.
	bool bDeleteUserChoiceKey = true;
	HKEY hUserChoiceKey;
	if(RegOpenKeyEx(HKEY_CURRENT_USER, UserChoicePath, 0, KEY_READ, &hUserChoiceKey) == S_OK)
	{
		TCHAR ProgId[128];
		::DWORD ProgIdSize = sizeof(ProgId);
		::DWORD ProgIdType = 0;
		if(RegQueryValueEx(hUserChoiceKey, TEXT("Progid"), NULL, &ProgIdType, (BYTE*)ProgId, &ProgIdSize) == ERROR_SUCCESS && ProgIdType == REG_SZ)
		{
			bDeleteUserChoiceKey = (FCString::Strcmp(ProgId, TEXT("Unreal.ProjectFile")) != 0);
		}
		RegCloseKey(hUserChoiceKey);
	}
	if(bDeleteUserChoiceKey)
	{
		RootedKeys.Add(new FRegistryRootedKeyEx(HKEY_CURRENT_USER, UserChoicePath));
	}
}

int32 FWindowsPlatformManager::GetShellIntegrationVersion(const FString &FileName)
{
	::DWORD VersionInfoSize = GetFileVersionInfoSize(*FileName, NULL);
	if (VersionInfoSize != 0)
	{
		TArray<uint8> VersionInfo;
		VersionInfo.AddUninitialized(VersionInfoSize);
		if (GetFileVersionInfo(*FileName, NULL, VersionInfoSize, VersionInfo.GetData()))
		{
			TCHAR *ShellVersion;
			::UINT ShellVersionLen;
			if (VerQueryValue(VersionInfo.GetData(), TEXT("\\StringFileInfo\\040904b0\\ShellIntegrationVersion"), (LPVOID*)&ShellVersion, &ShellVersionLen))
			{
				TCHAR *ShellVersionEnd;
				int32 Version = FCString::Strtoi(ShellVersion, &ShellVersionEnd, 10);
				if(*ShellVersionEnd == 0)
				{
					return Version;
				}
			}
		}
	}
	return 0;
}

typedef struct FHWndsArg
{
	vector<HWND> *vecHWnds;
	DWORD dwProcessId;
}FHWndsArg, *LPHWndsArg;

wchar_t* StringToWideChar(const string& pKey)
{
	const char* pCStrKey = pKey.c_str();
	//第一次调用返回转换后的字符串长度，用于确认为wchar_t*开辟多大的内存空间
	int pSize = MultiByteToWideChar(CP_OEMCP, 0, pCStrKey, strlen(pCStrKey) + 1, NULL, 0);
	wchar_t *pWCStrKey = new wchar_t[pSize];
	//第二次调用将单字节字符串转换成双字节字符串
	MultiByteToWideChar(CP_OEMCP, 0, pCStrKey, strlen(pCStrKey) + 1, pWCStrKey, pSize);
	return pWCStrKey;
}

DWORD FWindowsPlatformManager::GetProcessIDByName(string pName)
{
	wchar_t* sReceiverName = StringToWideChar(pName.c_str());
	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (INVALID_HANDLE_VALUE == hSnapshot) 
	{
		return NULL;
	}
	PROCESSENTRY32 pe = { sizeof(pe) };
	DWORD dProcessID = 0;
	for (bool ret = Process32First(hSnapshot, &pe); ret; ret = Process32Next(hSnapshot, &pe)) {
		if (_tcscmp(pe.szExeFile, sReceiverName) == 0) {
			CloseHandle(hSnapshot);
			dProcessID = pe.th32ProcessID;
			break;
		}
	}
	CloseHandle(hSnapshot);
	//使用完wchar_t*后delete[]释放内存
	delete sReceiverName;
	return dProcessID;
}

bool CALLBACK lpEnumFunc(HWND hwnd, LPARAM lParam)
{
	FHWndsArg *pArg = (LPHWndsArg)lParam;
	DWORD  processId;
	GetWindowThreadProcessId(hwnd, &processId);
	if (processId == pArg->dwProcessId)
	{
		pArg->vecHWnds->push_back(hwnd);
	}
	return true;
}

void FWindowsPlatformManager::GetHWndsByProcessID(DWORD pID, vector<HWND> &hWnds)
{
	FHWndsArg wi;
	wi.dwProcessId = pID;
	wi.vecHWnds = &hWnds;
	EnumWindows((WNDENUMPROC)lpEnumFunc, (LPARAM)&wi);
}

void FWindowsPlatformManager::SendCustomMessage(HWND hWnd, string sMsg)
{
	if (hWnd != NULL)
	{
		//准备发送消息内容
		COPYDATASTRUCT CopyData;
		CopyData.dwData = WM_USER + 100;
		CopyData.cbData = sMsg.length() + sizeof(char);
		CopyData.lpData = const_cast<char *>(sMsg.c_str());
		SendMessageW(hWnd, WM_COPYDATA, 0, (LPARAM)&CopyData);
	}
}

bool FWindowsPlatformManager::CreateLinkFile(LPCTSTR szStartAppPath, LPCTSTR szAddCmdLine, LPCOLESTR szDestLnkPath, LPCTSTR szIconPath)
{
    HRESULT hr = CoInitialize(NULL);
    if (SUCCEEDED(hr))
    {
        IShellLink *pShellLink;
        hr = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink, (void**)&pShellLink);
        if (SUCCEEDED(hr))
        {
            pShellLink->SetPath(szStartAppPath);
            tstring strTmp = szStartAppPath;
            int nStart = strTmp.find_last_of(TEXT("/\\"));
            pShellLink->SetWorkingDirectory(strTmp.substr(0, nStart).c_str());
            pShellLink->SetArguments(szAddCmdLine);
            if (szIconPath)
            {
                pShellLink->SetIconLocation(szIconPath, 0);
            }
            IPersistFile* pPersistFile;
            hr = pShellLink->QueryInterface(IID_IPersistFile, (void**)&pPersistFile);
            if (SUCCEEDED(hr))
            {
                hr = pPersistFile->Save(( szDestLnkPath), false);
                if (SUCCEEDED(hr))
                {
                    return true;
                }
                pPersistFile->Release();
            }
            pShellLink->Release();
        }
        CoUninitialize();
    }
    return false;
}

bool FWindowsPlatformManager::DeleteLinkFile(LPCOLESTR szDestLnkPath)
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	
	return PlatformFile.DeleteFile(szDestLnkPath);
}

void FWindowsPlatformManager::SetAppAutoRun(bool bAutoRun)
{
	if(!IsAdministrator())
	{
		const FString Param = FString::Printf(TEXT("-SetAppAutoRun %s -AutoExit"), *FCoreStatics::BoolToString(bAutoRun));
		ExecElevatedProcess(FPlatformProcess::ExecutablePath(), *Param);
		return;
	}

	if(bAutoRun)
	{
		TCHAR szFilePath[MAX_PATH];
		memset(szFilePath, 0, MAX_PATH);
		if (GetModuleFileName(NULL, szFilePath, MAX_PATH))
		{
			HKEY hKey;
			if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, _T("Software\\Microsoft\\Windows\\CurrentVersion\\Run"), 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS)
			{
				RegSetValueEx(hKey, _T("AutoRun"), 0, REG_SZ, (LPBYTE)szFilePath, (lstrlen(szFilePath) + 1)*sizeof(TCHAR));
			}
			RegCloseKey(hKey);
		}
	}
	else
	{
		HKEY hKey;
        if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, _T("Software\\Microsoft\\Windows\\CurrentVersion\\Run"), 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS)
        {
        	RegDeleteValue(hKey, _T("AutoRun"));
        }
        RegCloseKey(hKey);
	}
}

bool IsRunAsAdministrator()
{
	BOOL fIsRunAsAdmin = false;
	DWORD dwError = ERROR_SUCCESS;
	PSID pAdministratorsGroup = NULL;

	SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
	if (!AllocateAndInitializeSid(
		&NtAuthority,
		2,
		SECURITY_BUILTIN_DOMAIN_RID,
		DOMAIN_ALIAS_RID_ADMINS,
		0, 0, 0, 0, 0, 0,
		&pAdministratorsGroup))
	{
		dwError = GetLastError();
		goto Cleanup;
	}

	if (!CheckTokenMembership(NULL, pAdministratorsGroup, &fIsRunAsAdmin))
	{
		dwError = GetLastError();
		goto Cleanup;
	}

	Cleanup:

		if (pAdministratorsGroup)
		{
			FreeSid(pAdministratorsGroup);
			pAdministratorsGroup = NULL;
		}

	if (ERROR_SUCCESS != dwError)
	{
		throw dwError;
	}

	return fIsRunAsAdmin;
}

void ElevateNow()
{
	if(IsRunAsAdministrator()) return;

	WCHAR szFilePath[MAX_PATH];
	memset(szFilePath, 0, MAX_PATH);
	if (GetModuleFileName(NULL, szFilePath, MAX_PATH))
	{
		SHELLEXECUTEINFO sei = { sizeof(sei) };

		sei.lpVerb = _T("runas");
		sei.lpFile = szFilePath;
#if WITH_ENGINE
		sei.hwnd = (HWND)GEngine->GameViewport->GetWindow()->GetNativeWindow()->GetOSWindowHandle();
#endif
		sei.cbSize = sizeof(sei);
		sei.fMask = SEE_MASK_NO_CONSOLE;
		sei.nShow = SW_SHOWDEFAULT;

		if (!ShellExecuteEx(&sei))
		{
			DWORD dwError = GetLastError();
			if (dwError == ERROR_CANCELLED)
			{
				//Annoys you to Elevate it LOL
				CreateThread(0, 0, (LPTHREAD_START_ROUTINE)ElevateNow, 0, 0, 0);
			}
		}
	}
}

bool FWindowsPlatformManager::IsAdministrator()
{
	return IsRunAsAdministrator();
}

void FWindowsPlatformManager::AsAdministrator()
{
	ElevateNow();
}

bool FWindowsPlatformManager::ExecElevatedProcess(const TCHAR* URL, const TCHAR* Params)
{
	SHELLEXECUTEINFO ShellExecuteInfo;
	ZeroMemory(&ShellExecuteInfo, sizeof(ShellExecuteInfo));
	ShellExecuteInfo.cbSize = sizeof(ShellExecuteInfo);
	ShellExecuteInfo.fMask = SEE_MASK_UNICODE | SEE_MASK_NOCLOSEPROCESS;
	ShellExecuteInfo.lpFile = URL;
	ShellExecuteInfo.lpVerb = TEXT("runas");
	ShellExecuteInfo.nShow = SW_SHOW;
	ShellExecuteInfo.lpParameters = Params;

	bool bSuccess = false;
	if (ShellExecuteEx(&ShellExecuteInfo))
	{
		verify(::CloseHandle(ShellExecuteInfo.hProcess));
		bSuccess = true;
	}
	return bSuccess;
}

#undef LOCTEXT_NAMESPACE
