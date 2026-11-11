#if WITH_DEV_AUTOMATION_TESTS

#include "SaveGame/SaveOperationTypes.h"
#include "SaveGame/SaveSlotTypes.h"
#include "SaveGame/SaveManifestTypes.h"
#include "SaveGame/SaveFileTypes.h"
#include "Common/LifecycleTypes.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Event/EventModule.h"
#include "Event/Events/Common/Game/Event_GameExited.h"
#include "Gameplay/WHGameInstance.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "JsonObjectConverter.h"
#include "Main/MainModule.h"
#include "Misc/AutomationTest.h"
#include "Misc/Char.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "SaveGame/SaveDataSerializer.h"
#include "SaveGame/SaveGameModule.h"
#include "SaveGame/Tests/SaveGameTestPathSafety.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
bool CanRemoveTestNamespace(const FString& InPath, const FString& InName, FString& OutPath, FString& OutError)
{
	const FString Prefix(TEXT("Automation_Lazy_"));
	if (!InName.StartsWith(Prefix, ESearchCase::CaseSensitive) || InName.Len() != Prefix.Len() + 12)
	{
		OutError = TEXT("测试目录名不是本轮随机名称。");
		return false;
	}
	for (int32 Index = Prefix.Len(); Index < InName.Len(); ++Index)
	{
		if (!FChar::IsHexDigit(InName[Index]))
		{
			OutError = TEXT("测试目录名的随机部分不是十六进制。");
			return false;
		}
	}
	const auto Canonicalize = [](const FString& In, FString& Out)
	{
		Out = FPaths::ConvertRelativePathToFull(In);
		FPaths::NormalizeDirectoryName(Out);
		return !FPaths::IsRelative(Out) && FPaths::CollapseRelativeDirectories(Out);
	};
	FString ProjectRoot, InstancesRoot;
	if (!Canonicalize(FPaths::ProjectDir(), ProjectRoot) ||
		!Canonicalize(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames"), TEXT("Instances")), InstancesRoot) ||
		!Canonicalize(InPath, OutPath) || !FPaths::IsUnderDirectory(InstancesRoot, ProjectRoot) ||
		!FPaths::IsUnderDirectory(OutPath, InstancesRoot) ||
		!FPaths::IsSamePath(FPaths::GetPath(OutPath), InstancesRoot) ||
		!FPaths::GetCleanFilename(OutPath).Equals(InName, ESearchCase::CaseSensitive))
	{
		OutError = TEXT("测试目录的绝对路径、直接父目录或末级名称不匹配。");
		return false;
	}
	IPlatformFile& Platform = FPlatformFileManager::Get().GetPlatformFile();
	// 禁止通过 Saved、Instances 或目标目录的链接进入别处。
	FString Ancestor = OutPath;
	for (;;)
	{
		if (Platform.IsSymlink(*Ancestor) != ESymlinkResult::NonSymlink)
		{
			OutError = TEXT("测试目录的父路径包含链接，或平台无法验证链接。");
			return false;
		}
		if (FPaths::IsSamePath(Ancestor, ProjectRoot)) break;
		const FString Parent = FPaths::GetPath(Ancestor);
		if (Parent.IsEmpty() || FPaths::IsSamePath(Parent, Ancestor) || !FPaths::IsUnderDirectory(Parent, ProjectRoot))
		{
			OutError = TEXT("无法验证测试目录的父路径。");
			return false;
		}
		Ancestor = Parent;
	}
	// UE 的递归删除会跟随目录链接；先逐层枚举，发现链接立即停止且不进入。
	TArray<FString> Directories{OutPath};
	while (!Directories.IsEmpty())
	{
		const FString Directory = Directories.Pop(EAllowShrinking::No);
		if (!Platform.IterateDirectory(*Directory, [&](const TCHAR* Child, bool bDirectory)
		{
			if (Platform.IsSymlink(Child) != ESymlinkResult::NonSymlink) return false;
			if (bDirectory) Directories.Add(Child);
			return true;
		}))
		{
			OutError = TEXT("测试目录树包含链接，或无法完整检查。");
			return false;
		}
	}
	return true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSaveGameTransientSessionTest,
	"WHFramework.SaveGame.TransientSession.IsolationAndTravel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSaveGameTransientSessionTest::RunTest(const FString&)
{
	using SaveGameTestPathSafety::EFixturePathKind;
	using SaveGameTestPathSafety::FixtureAbsolutePath;
	using SaveGameTestPathSafety::ProbeFixturePath;
	using SaveGameTestPathSafety::ValidateFixtureDirectoryChain;
	using SaveGameTestPathSafety::CreateExclusiveFixtureRoot;
	using SaveGameTestPathSafety::VisitFixtureDirectory;
	using SaveGameTestPathSafety::ValidateFixtureTree;
	using SaveGameTestPathSafety::FixtureDirectoryIsEmpty;

	TStrongObjectPtr<UWHGameInstance> Instance(NewObject<UWHGameInstance>());
	const FString TestNamespace = TEXT("Automation_Lazy_") + FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(12);
	Instance->RequestedLocalDataNamespace = TestNamespace;
	const FString NamespaceRoot = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames"),
		TEXT("Instances"), Instance->RequestedLocalDataNamespace));
	UWorld* Worlds[4] = {};
	USaveGameModule* Saves[4] = {};
	ON_SCOPE_EXIT
	{
		for (int32 Index = 3; Index >= 0; --Index)
		{
			if (Saves[Index])
			{
				Saves[Index]->Storage.Reset();
				Saves[Index]->LocalSaveContext.Reset();
			}
			if (Worlds[Index])
			{
				Worlds[Index]->DestroyWorld(false);
				GEngine->DestroyWorldContext(Worlds[Index]);
			}
		}
		AddInfo(FString::Printf(TEXT("隔离会话夹具目录保留用于检查：%s"), *NamespaceRoot));
		Instance->LocalSaveContext.Reset();
	};
	TestFalse(TEXT("新 GameInstance 尚未申请持久目录"), Instance->GetLocalSaveContext().IsValid());
	const FString InstancesRoot = FixtureAbsolutePath(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames"), TEXT("Instances")));
	const FString ProjectRoot = FixtureAbsolutePath(FPaths::ProjectDir());
	const FString SavedRoot = FixtureAbsolutePath(FPaths::ProjectSavedDir());
	if (!TestTrue(TEXT("临时初始化前确认候选目录不存在且父路径无链接"),
		!ProjectRoot.IsEmpty() && !SavedRoot.IsEmpty() && !InstancesRoot.IsEmpty() &&
		FPaths::IsUnderDirectory(SavedRoot, ProjectRoot) && FPaths::IsUnderDirectory(InstancesRoot, SavedRoot) &&
		FPaths::IsSamePath(FPaths::GetPath(NamespaceRoot), InstancesRoot) &&
		FPaths::IsUnderDirectory(NamespaceRoot, InstancesRoot) &&
		FPaths::GetCleanFilename(NamespaceRoot) == TestNamespace &&
		ValidateFixtureDirectoryChain(InstancesRoot, true) && ProbeFixturePath(NamespaceRoot) == EFixturePathKind::Missing)) return false;

	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Worlds); ++Index)
	{
		Worlds[Index] = UWorld::CreateWorld(EWorldType::Game, false);
		if (!TestNotNull(TEXT("创建独立关卡世界"), Worlds[Index])) return false;
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(Worlds[Index]);
		Worlds[Index]->SetGameInstance(Instance.Get());
		AMainModule* Main = Worlds[Index]->SpawnActor<AMainModule>();
		if (!TestNotNull(TEXT("创建关卡模块所有者"), Main)) return false;
		UEventModule* Events = NewObject<UEventModule>(Main);
		Saves[Index] = NewObject<USaveGameModule>(Main);
		Main->RegisterModule(Events);
		Main->RegisterModule(Saves[Index]);
	}

	// 实际跨图交接：进入临时目标时拒绝并释放原文件代租约。
	Saves[0]->SessionPolicy = ESaveSessionPolicy::Transient;
	if (!TestTrue(TEXT("初始化前可读取关卡配置的临时策略"), Saves[0]->IsTransientSession())) return false;
	FSaveGameLoadSnapshot Pending;
	Pending.Context.OperationId = FGuid::NewGuid();
	Pending.Context.SaveId = FGuid::NewGuid();
	Pending.Context.Generation = 1;
	Pending.Context.TargetMap = TEXT("/Game/TransientDestination");
	Pending.Context.LoadPhase = EPhase::Primary;
	const FGuid OperationId = Pending.Context.OperationId;
	const FString PinDirectory = FPaths::Combine(NamespaceRoot, TEXT("GenerationPin"));
	Pending.ReadLease = FSaveGameGenerationReadLease::Acquire(PinDirectory);
	if (!TestTrue(TEXT("原跨图操作接纳有效快照"), Instance->SetPendingSaveLoad(MoveTemp(Pending)))) return false;
	TestEqual(TEXT("原文件代被读取租约固定"), FSaveGameGenerationReadLease::GetPinCount(PinDirectory), uint64(1));
	{
		Saves[0]->OnInitialize();
	}
	const FSaveOperationResult Rejected = Saves[0]->GetLoadOperationResult(OperationId);
	TestTrue(TEXT("临时目标返回原操作身份的明确失败"), Rejected.Code == ESaveResultCode::Unsupported && Rejected.OperationId == OperationId);
	TestFalse(TEXT("已拒绝的快照不再留在 GameInstance"), Instance->ConsumePendingSaveLoad().IsValid());
	TestEqual(TEXT("临时目标释放文件代租约"), FSaveGameGenerationReadLease::GetPinCount(PinDirectory), uint64(0));
	if (!TestFalse(TEXT("临时启动不取得持久目录租约"), Instance->GetLocalSaveContext().IsValid()) ||
		!TestTrue(TEXT("临时启动未创建持久目录"), ProbeFixturePath(NamespaceRoot) == EFixturePathKind::Missing)) return false;
	TestFalse(TEXT("临时会话没有活动槽"), Saves[0]->HasActiveSave());
	TestTrue(TEXT("临时会话不暴露持久路径"), Saves[0]->GetStorageRoot().IsEmpty() && Saves[0]->GetUnboundInventoryRoot().IsEmpty());
	Saves[0]->SessionPolicy = ESaveSessionPolicy::Persistent;
	if (!TestTrue(TEXT("初始化后更改配置不能使临时会话变为持久会话"), Saves[0]->IsTransientSession())) return false;
	{
		Saves[0]->OnInitialize();
	}
	if (!TestFalse(TEXT("重复初始化不会让临时会话取得持久租约"), Instance->GetLocalSaveContext().IsValid()) ||
		!TestTrue(TEXT("重复初始化不会创建持久目录"), ProbeFixturePath(NamespaceRoot) == EFixturePathKind::Missing)) return false;

	// 临时会话已证明不会申请目录；现在独占创建一份模拟既有的有效持久存档。
	FString CreatedRoot;
	if (!TestTrue(TEXT("只在本轮随机名称下原子新建独立目录"),
		CreateExclusiveFixtureRoot(FPaths::GetPath(NamespaceRoot), TestNamespace, CreatedRoot) &&
		FPaths::IsSamePath(CreatedRoot, NamespaceRoot) && FixtureDirectoryIsEmpty(CreatedRoot))) return false;
	if (!TestFalse(TEXT("构造磁盘夹具不会预先申请持久目录租约"), Instance->GetLocalSaveContext().IsValid())) return false;
	FCreateSaveSlotParams Params;
	Params.DisplayName = TEXT("persistent-before-transient");
	FSaveGameStorage Existing(0, NamespaceRoot);
	FSaveManifest Manifest;
	Manifest.SaveId = FGuid::NewGuid();
	Manifest.DisplayName = Params.DisplayName;
	Manifest.CreatedAt = Manifest.UpdatedAt = FDateTime::UtcNow();
	FSaveData FixtureData;
	FixtureData.SetDatas(TArray<uint8>{1, 3, 5, 7});
	TArray<uint8> FixtureBytes;
	const FName FixtureModule(TEXT("FixtureModule"));
	if (!TestTrue(TEXT("构造完整模块文件"), FSaveDataSerializer::BuildModuleFile(FixtureModule, 1, FParameter(FixtureData), FixtureBytes))) return false;
	Manifest.CurrentGeneration = 1;
	Manifest.LastCommitOperationId = FGuid::NewGuid();
	Manifest.ModuleVersions.Add(FixtureModule, 1);
	const FSaveSlotSummary Summary = Manifest.ToSummary(Existing.GetWorldDir(Manifest.SaveId));
	const FString ManifestPath = Existing.GetManifestPath(Summary.SaveId);
	const FString ModulePath = Existing.GetModuleFilePath(Summary.SaveId, 1, FixtureModule);
	const FString LastActivePath = Existing.GetLastActiveSavePath();
	const FString ProfilePath = Existing.GetProfileModuleFilePath(FName(TEXT("Default")), FName(TEXT("SettingModule")));
	auto PrepareFixtureFile = [&](const FString& InPath)
	{
		const FString Path = FixtureAbsolutePath(InPath);
		const FString Parent = FPaths::GetPath(Path);
		return !Path.IsEmpty() && FPaths::IsUnderDirectory(Path, NamespaceRoot) &&
			ProbeFixturePath(Path) == EFixturePathKind::Missing && ValidateFixtureDirectoryChain(Parent, true) &&
			IFileManager::Get().MakeDirectory(*Parent, true) && ValidateFixtureDirectoryChain(Parent, false) &&
			ProbeFixturePath(Path) == EFixturePathKind::Missing;
	};
	FString ManifestJson;
	if (!TestTrue(TEXT("合成有效清单"), FJsonObjectConverter::UStructToJsonObjectString(Manifest, ManifestJson)) ||
		!TestTrue(TEXT("直接写入隔离槽清单"), PrepareFixtureFile(ManifestPath) && FFileHelper::SaveStringToFile(ManifestJson, *ManifestPath)) ||
		!TestTrue(TEXT("直接写入真实序列化模块"), PrepareFixtureFile(ModulePath) && FFileHelper::SaveArrayToFile(FixtureBytes, *ModulePath)) ||
		!TestTrue(TEXT("直接写入最近槽标记"), PrepareFixtureFile(LastActivePath) &&
			FFileHelper::SaveStringToFile(Summary.SaveId.ToString(EGuidFormats::DigitsWithHyphens), *LastActivePath)) ||
		!TestTrue(TEXT("直接写入 Profile 哨兵"), PrepareFixtureFile(ProfilePath) &&
			FFileHelper::SaveStringToFile(TEXT("existing-profile"), *ProfilePath))) return false;
	FSaveManifest ReadManifest;
	FGuid LastActive;
	if (!TestTrue(TEXT("正式读取器接受合成的完整持久槽"), Existing.ReadManifest(Summary.SaveId, ReadManifest) &&
		ReadManifest.CurrentGeneration == 1 && Existing.IsGenerationComplete(Summary.SaveId, 1, ReadManifest) &&
		Existing.ReadLastActiveSave(LastActive) && LastActive == Summary.SaveId)) return false;
	TArray<uint8> StoredModule;
	FModuleSaveFileHeader FixtureHeader;
	FParameter DecodedFixture;
	if (!TestTrue(TEXT("合成文件代可由正式模块反序列化器读取"), FFileHelper::LoadFileToArray(StoredModule, *ModulePath) &&
		StoredModule == FixtureBytes && FSaveDataSerializer::ReadModuleFile(StoredModule, FixtureHeader, DecodedFixture) &&
		FixtureHeader.ModuleName == FixtureModule && FixtureHeader.ModuleVersion == 1)) return false;
	TMap<FString, TArray<uint8>> FixtureBefore;
	for (const FString& Path : {ManifestPath, ModulePath, LastActivePath, ProfilePath})
	{
		TArray<uint8>& Bytes = FixtureBefore.Add(Path);
		if (!TestTrue(TEXT("读取夹具原始字节"), FFileHelper::LoadFileToArray(Bytes, *Path))) return false;
	}
	// 两次持久初始化都会执行恢复和退役清理。只允许一份已发布代，且没有待恢复或待删除对象。
	auto CanInitializePersistent = [&]()
	{
		if (!FPaths::IsSamePath(FPaths::GetPath(NamespaceRoot), InstancesRoot) ||
			FPaths::GetCleanFilename(NamespaceRoot) != TestNamespace || !ValidateFixtureTree(NamespaceRoot) ||
			!Existing.ReadManifest(Summary.SaveId, ReadManifest) ||
			ReadManifest.CurrentGeneration != 1 || !Existing.IsGenerationComplete(Summary.SaveId, 1, ReadManifest) ||
			!Existing.ReadLastActiveSave(LastActive) || LastActive != Summary.SaveId) return false;
		auto HasOnlyDirectory = [](const FString& Directory, const FString& Name)
		{
			int32 Count = 0;
			return VisitFixtureDirectory(Directory, [&](const FString& Child, const bool bDirectory)
			{
				++Count;
				return bDirectory && FPaths::GetCleanFilename(Child) == Name;
			}) && Count == 1;
		};
		if (!HasOnlyDirectory(Existing.GetWorldsDir(), Summary.SaveId.ToString(EGuidFormats::Digits)) ||
			!HasOnlyDirectory(Existing.GetGenerationsDir(Summary.SaveId), TEXT("00000001")) ||
			ProbeFixturePath(FPaths::Combine(NamespaceRoot, TEXT("RetiredWorlds"))) != EFixturePathKind::Missing) return false;
		const FString Operations = FPaths::Combine(NamespaceRoot, TEXT("WorldOperations"));
		const EFixturePathKind OperationsKind = ProbeFixturePath(Operations);
		if (OperationsKind == EFixturePathKind::Missing) return true;
		if (OperationsKind != EFixturePathKind::Directory) return false;
		const FString AllowedLock = Summary.SaveId.ToString(EGuidFormats::Digits) + TEXT(".intent.lock");
		return VisitFixtureDirectory(Operations, [&](const FString& Child, const bool bDirectory)
		{
			return !bDirectory && FPaths::GetCleanFilename(Child) == AllowedLock;
		});
	};
	if (!TestTrue(TEXT("正式初始化前确认没有可恢复或退役的目录"), CanInitializePersistent())) return false;
	if (!TestFalse(TEXT("默认关卡仍是持久会话"), Saves[1]->IsTransientSession())) return false;
	{
		Saves[1]->OnInitialize();
	}
	const auto Context = Instance->GetLocalSaveContext();
	if (!TestTrue(TEXT("正式关卡按需取得该实例的持久目录租约"), Context && Context->GetNamespace() == TestNamespace &&
		FPaths::IsSamePath(Context->GetStorageRoot(), NamespaceRoot)) ||
		!TestTrue(TEXT("正式关卡恢复已有的有效活动槽"), FPaths::IsSamePath(Saves[1]->GetStorageRoot(), NamespaceRoot) &&
			Saves[1]->GetActiveSaveId() == Summary.SaveId)) return false;
	Saves[1]->SessionPolicy = ESaveSessionPolicy::Transient;
	TestFalse(TEXT("初始化后更改配置不能使持久会话变为临时会话"), Saves[1]->IsTransientSession());
	TestTrue(TEXT("持久会话仍保有同一存档槽和目录所有权"), Saves[1]->GetActiveSaveId() == Summary.SaveId &&
		FPaths::IsSamePath(Saves[1]->GetStorageRoot(), NamespaceRoot));

	// GameInstance 已持有真实租约时，后续临时图仍不能访问旧槽或 Profile。
	Saves[2]->SessionPolicy = ESaveSessionPolicy::Transient;
	if (!TestTrue(TEXT("后续临时图初始化前仍采用临时策略"), Saves[2]->IsTransientSession())) return false;
	{
		Saves[2]->OnInitialize();
	}
	if (!TestTrue(TEXT("临时图不接管先前持久目录"), Saves[2]->IsTransientSession() && !Saves[2]->Storage &&
		Saves[2]->GetStorageRoot().IsEmpty() && Saves[2]->GetUnboundInventoryRoot().IsEmpty())) return false;
	TestFalse(TEXT("临时图不恢复最近槽"), Saves[2]->HasActiveSave());
	FSaveSlotSummary Unused;
	const auto Unsupported = [this](const TCHAR* Label, const FSaveOperationResult& Result)
	{
		return TestEqual(Label, static_cast<uint8>(Result.Code), static_cast<uint8>(ESaveResultCode::Unsupported));
	};
	if (!Unsupported(TEXT("临时会话拒绝创建"), Saves[2]->CreateSaveSlot(Params, Unused)) ||
		!Unsupported(TEXT("临时会话拒绝保存"), Saves[2]->SaveSlot(Summary.SaveId)) ||
		!Unsupported(TEXT("临时会话拒绝保存当前槽"), Saves[2]->SaveCurrentSlot()) ||
		!Unsupported(TEXT("临时会话拒绝加载"), Saves[2]->LoadSlot(Summary.SaveId)) ||
		!Unsupported(TEXT("临时会话拒绝删除"), Saves[2]->DeleteSaveSlot(Summary.SaveId)) ||
		!Unsupported(TEXT("临时会话拒绝重命名"), Saves[2]->RenameSaveSlot(Summary.SaveId, TEXT("forbidden"))) ||
		!Unsupported(TEXT("临时会话拒绝复制"), Saves[2]->DuplicateSaveSlot(Summary.SaveId, TEXT("forbidden"), Unused)) ||
		!Unsupported(TEXT("临时会话拒绝保存设置 Profile"), Saves[2]->SaveProfile()) ||
		!Unsupported(TEXT("临时会话拒绝加载设置 Profile"), Saves[2]->LoadProfile())) return false;
	Saves[2]->BeginPendingSaveSlot(Params);
	TestFalse(TEXT("临时会话不接受待创建槽"), Saves[2]->HasPendingSaveSlot());
	TestTrue(TEXT("临时会话不枚举旧槽"), Saves[2]->GetSaveSlotSummaries().IsEmpty());
	Saves[2]->SetUserIndex(12);
	Saves[2]->ClearActiveSave();
	Saves[2]->OnGameExited(nullptr, FEventGameExited(false));
	for (const TPair<FString, TArray<uint8>>& Entry : FixtureBefore)
	{
		TArray<uint8> After;
		if (!TestTrue(FString::Printf(TEXT("临时操作和退出保留原文件字节：%s"), *Entry.Key),
			FFileHelper::LoadFileToArray(After, *Entry.Key) && Entry.Value == After)) return false;
	}
	TestTrue(TEXT("最近存档标记仍指向原槽"), Existing.ReadLastActiveSave(LastActive) && LastActive == Summary.SaveId);

	if (!TestTrue(TEXT("再次持久初始化前仍持有原实例租约"), Instance->GetLocalSaveContext() == Context) ||
		!TestTrue(TEXT("再次持久初始化前确认没有可恢复或退役的目录"), CanInitializePersistent())) return false;
	{
		Saves[3]->OnInitialize();
	}
	TestTrue(TEXT("回到正式关卡仍恢复原活动槽"), Saves[3]->GetActiveSaveId() == Summary.SaveId);
	TestTrue(TEXT("回到正式关卡复用原实例所有权"), FPaths::IsSamePath(Saves[3]->GetStorageRoot(), NamespaceRoot));
	AddInfo(TEXT("本回归使用合成的有效既有槽，不覆盖正式创建和主动加载；实际异步提交由协调器测试另行验证。"));
	AddInfo(TEXT("此回归不实际退出编辑器；用户退出的引擎窗口关闭仍需运行期验证。"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSaveGameTransientCleanupBoundaryTest,
	"WHFramework.SaveGame.TransientSession.CleanupBoundary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSaveGameTransientCleanupBoundaryTest::RunTest(const FString&)
{
	const FString Name(TEXT("Automation_Lazy_0123456789ab"));
	const FString Instances = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames"), TEXT("Instances"));
	const FString Target = FPaths::Combine(Instances, Name);
	FString Checked, Error;
	TestFalse(TEXT("禁止删除实例根目录本身"), CanRemoveTestNamespace(Instances, Name, Checked, Error));
	TestFalse(TEXT("禁止删除类似前缀的兄弟目录"), CanRemoveTestNamespace(FPaths::Combine(Instances + TEXT("Other"), Name), Name, Checked, Error));
	TestFalse(TEXT("禁止末级不同于当前随机名"), CanRemoveTestNamespace(FPaths::Combine(Instances, TEXT("Automation_Lazy_0123456789ac")), Name, Checked, Error));
	TestFalse(TEXT("禁止清理随机目录的子目录"), CanRemoveTestNamespace(FPaths::Combine(Target, TEXT("Child")), Name, Checked, Error));
	TestFalse(TEXT("禁止清理向上跳出的目录"), CanRemoveTestNamespace(FPaths::Combine(Instances, TEXT(".."), Name), Name, Checked, Error));
	TestFalse(TEXT("禁止非随机十六进制名称"), CanRemoveTestNamespace(Target, TEXT("Automation_Lazy_invalid"), Checked, Error));
	return true;
}

#endif
