// Fill out your copyright notice in the Description page of Project Settings.

#include "Platform/Windows/WindowsPlatformManager.h"

#include "GeneralProjectSettings.h"
#include "WHFrameworkCoreTypes.h"
#include "Main/MainManager.h"
#include "Platform/Windows/WindowsMessageHelpers.h"
#if PLATFORM_WINDOWS
#include <activation.h>
#include <shellapi.h>
#include "Windows/MinWindows.h"
#endif
#include<windows.h>
#include <string>
#include <shellapi.h>
#include <shlobj.h>
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
					GEngine->GameViewport->GetWindow()->ShowWindow();
					GEngine->GameViewport->GetWindow()->BringToFront();
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
	UE_LOG(LogTemp, Warning, TEXT(">>> FPATHS : %s"), *iconPath);
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

	UE_LOG(LogTemp, Warning, TEXT(">>> Shell_NotifyIcon : "));

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
		UE_LOG(LogTemp, Warning, TEXT(">>> WM_SHOWWINDOW : "));

		GEngine->GameViewport->GetWindow()->ShowWindow();
		GEngine->GameViewport->GetWindow()->BringToFront();
	}
	else if (MenuID == WM_DESTROY)
	{
		UE_LOG(LogTemp, Warning, TEXT(">>> WM_DESTROY : "));
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

BOOL CALLBACK lpEnumFunc(HWND hwnd, LPARAM lParam)
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
