
#pragma once
#include "SceneTypes.h"
#include "WHFrameworkCoreTypes.h"
#include "Main/MainTypes.h"
#include "Platform/PlatformManager.h"
#include "Windows/WindowsApplication.h"
#include "Containers/IndirectArray.h"
#include "WindowsRegistryEx.h"

using namespace std;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnProcessMessage, const FString&)

/**
 * Example Windows message handler.
 */
class WHFRAMEWORKCORE_API FExampleHandler : public IWindowsMessageHandler
{
public:
	FExampleHandler();
	
	virtual ~FExampleHandler();
	
public:
	//~ IWindowsMessageHandler interface
	virtual bool ProcessMessage(HWND Hwnd, uint32 Message, WPARAM WParam, LPARAM LParam, int32& OutResult) override;

public:
	virtual bool CreateTrayIcon();

	virtual bool DeleteTrayIcon();

	virtual void AddTrayMenu(HWND Hwnd);

public:
	FOnProcessMessage OnProcessMessage;
};

class WHFRAMEWORKCORE_API FWindowsPlatformManager : public FPlatformManager
{
	GENERATED_MANAGER(FWindowsPlatformManager)

public:
	// ParamSets default values for this actor's properties
	FWindowsPlatformManager();

	virtual ~FWindowsPlatformManager() override;

	static const FUniqueType Type;

public:
	virtual void OnInitialize() override;

	virtual void OnTermination() override;
	
public:
	// IPlatformManager Implementation
	virtual bool OpenFileDialog(const void* ParentWindowHandle, const FString& DialogTitle, const FString& DefaultPath, const FString& DefaultFile, const FString& FileTypes, uint32 Flags, TArray<FString>& OutFilenames) override;
	virtual bool OpenFileDialog(const void* ParentWindowHandle, const FString& DialogTitle, const FString& DefaultPath, const FString& DefaultFile, const FString& FileTypes, uint32 Flags, TArray<FString>& OutFilenames, int32& OutFilterIndex) override;
	virtual bool SaveFileDialog(const void* ParentWindowHandle, const FString& DialogTitle, const FString& DefaultPath, const FString& DefaultFile, const FString& FileTypes, uint32 Flags, TArray<FString>& OutFilenames) override;
	virtual bool OpenDirectoryDialog(const void* ParentWindowHandle, const FString& DialogTitle, const FString& DefaultPath, FString& OutFolderName) override;
	virtual bool OpenFontDialog(const void* ParentWindowHandle, FString& OutFontName, float& OutHeight, EFontImportFlags& OutFlags) override;

	virtual bool RegisterEngineInstallation(const FString &RootDir, FString &OutIdentifier) override;
	virtual void EnumerateEngineInstallations(TMap<FString, FString> &OutInstallations) override;

	virtual bool IsSourceDistribution(const FString &RootDir) override;

	virtual bool VerifyFileAssociations() override;
	virtual bool UpdateFileAssociations() override;

	virtual bool OpenProject(const FString &ProjectFileName) override;

	using FWindowsPlatformManager::RunUnrealBuildTool;
	virtual bool RunUnrealBuildTool(const FText& Description, const FString& RootDir, const FString& Arguments, FFeedbackContext* Warn, int32& OutExitCode) override;
	virtual bool IsUnrealBuildToolRunning() override;

	virtual FFeedbackContext* GetNativeFeedbackContext() override;

	virtual FString GetUserTempPath() override;

private:
	bool FileDialogShared(bool bSave, const void* ParentWindowHandle, const FString& DialogTitle, const FString& DefaultPath, const FString& DefaultFile, const FString& FileTypes, uint32 Flags, TArray<FString>& OutFilenames, int32& OutFilterIndex);
	void GetRequiredRegistrySettings(TIndirectArray<FRegistryRootedKeyEx> &RootedKeys);
	int32 GetShellIntegrationVersion(const FString &FileName);

public:
	virtual DWORD GetProcessIDByName(string pName);

	virtual void GetHWndsByProcessID(DWORD pID, vector<HWND>& hWnds);

	virtual void SendCustomMessage(HWND hWnd, string sMsg);

	virtual bool CreateLinkFile(LPCTSTR szStartAppPath, LPCTSTR szAddCmdLine, LPCOLESTR szDestLnkPath, LPCTSTR szIconPath);

	virtual bool DeleteLinkFile(LPCOLESTR szDestLnkPath);

	virtual void SetAppAutoRun(bool bAutoRun);

	virtual bool IsAdministrator();
	
	virtual void AsAdministrator();

	virtual bool ExecElevatedProcess(const TCHAR* URL, const TCHAR* Params);

protected:
	FWindowsApplication* GetApplication() const
	{
		if (!FSlateApplication::IsInitialized())
		{
			return nullptr;
		}

		return (FWindowsApplication*)FSlateApplication::Get().GetPlatformApplication().Get();
	}
	
private:
	FExampleHandler Handler;

public:
	FExampleHandler& GetHandler() { return Handler; }
};
