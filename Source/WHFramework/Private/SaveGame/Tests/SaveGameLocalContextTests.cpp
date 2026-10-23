#if WITH_DEV_AUTOMATION_TESTS

#include "SaveGame/SaveGameLocalContext.h"
#include "SaveGame/SaveGameAsyncExecutor.h"
#include "SaveGame/SaveGameStorage.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSaveGameLocalContextTest,
	"WHFramework.SaveGame.LocalInstanceIsolation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSaveGameLocalContextTest::RunTest(const FString&)
{
	const FString Root = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"),
		TEXT("LocalInstanceIsolation"), FGuid::NewGuid().ToString(EGuidFormats::Digits), TEXT("SaveGames")));
	FString Error;
	TSharedPtr<FSaveGameLocalContext, ESPMode::ThreadSafe> First = MakeShared<FSaveGameLocalContext, ESPMode::ThreadSafe>();
	TSharedPtr<FSaveGameLocalContext, ESPMode::ThreadSafe> Second = MakeShared<FSaveGameLocalContext, ESPMode::ThreadSafe>();
	if (!TestTrue(TEXT("首实例取得原有默认目录"), First->Acquire(Root, FString(), Error)) ||
		!TestTrue(TEXT("第二实例自动取得独立目录"), Second->Acquire(Root, FString(), Error))) return false;
	TestEqual(TEXT("首实例保留原存档布局"), First->GetStorageRoot(), Root);
	TestEqual(TEXT("默认实例身份稳定"), First->GetNamespace(), FString(TEXT("default")));
	TestFalse(TEXT("两个实例不共享世界目录"), First->GetStorageRoot() == Second->GetStorageRoot());
	TestFalse(TEXT("未保存世界的库存目录也隔离"), First->GetUnboundInventoryRoot() == Second->GetUnboundInventoryRoot());
	TestEqual(TEXT("凭据相对路径与世界存档根一致"), FPaths::Combine(Root, Second->GetSlotPrefix()), Second->GetStorageRoot());
	const FString Marker = FPaths::Combine(Second->GetStorageRoot(), TEXT("retained.txt"));
	TestTrue(TEXT("创建需重启保留的数据"), FFileHelper::SaveStringToFile(TEXT("persistent-instance"), *Marker));
	const FString SecondName = Second->GetNamespace();
	const FString SecondRoot = Second->GetStorageRoot();
	FSaveGameLocalContext Duplicate;
	TestFalse(TEXT("显式同名实例不能取得已有目录"), Duplicate.Acquire(Root, SecondName, Error));
	TestFalse(TEXT("目录别名不能绕过原所有权"), Duplicate.Acquire(Root + TEXT("/../SaveGames"), SecondName, Error));
	FSaveGenerationPlan PendingWorker;
	PendingWorker.LocalSaveContext = Second;
	Second.Reset();
	TestFalse(TEXT("实例退出但原生保存仍持有目录时不能重开"), Duplicate.Acquire(Root, SecondName, Error));
	PendingWorker.LocalSaveContext.Reset();
	if (!TestTrue(TEXT("最后保存引用释放后可重新取得同名目录"), Duplicate.Acquire(Root, SecondName, Error))) return false;
	TestEqual(TEXT("重开使用原目录而非随机目录"), Duplicate.GetStorageRoot(), SecondRoot);
	FString Retained;
	TestTrue(TEXT("重开仍可读取原数据"), FFileHelper::LoadFileToString(Retained, *Marker));
	TestEqual(TEXT("重开不覆盖旧数据"), Retained, FString(TEXT("persistent-instance")));
	FSaveGameLocalContext Invalid;
	TestFalse(TEXT("实例名称不能包含上级路径"), Invalid.Acquire(Root, TEXT("../other"), Error));
	TestFalse(TEXT("实例名称不能包含绝对路径"), Invalid.Acquire(Root, TEXT("C:/other"), Error));
	AddInfo(TEXT("原生目录租约不依赖PIE分支；独立打包进程的实际启动与退出仍须单独验收。"));
	return true;
}

#endif
