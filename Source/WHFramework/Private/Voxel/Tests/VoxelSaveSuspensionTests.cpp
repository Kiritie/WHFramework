#include "Voxel/VoxelModule.h"

#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelSaveSuspensionOwnershipTest,
	"WHFramework.Voxel.Save.SuspensionOwnership",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FVoxelSaveSuspensionOwnershipTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UVoxelModule> Module(NewObject<UVoxelModule>());
	FString Error;
	const FGuid Token = FGuid::NewGuid();
	TestFalse(TEXT("未准备世界不能接纳最终保存冻结"), Module->BeginWorldSaveSuspension(Token, Error));
	TestFalse(TEXT("失败接纳不残留冻结所有权"), Module->IsWorldSaveSuspended());
	Module->Epoch = 17;
	Module->WorldSaveSuspensionEpoch = 17;
	Module->WorldSaveSuspensionToken = Token;
	TestFalse(TEXT("新所有者不能覆盖已接纳冻结"), Module->BeginWorldSaveSuspension(FGuid::NewGuid(), Error));
	TestEqual(TEXT("冻结所有者保持原令牌"), Module->WorldSaveSuspensionToken, Token);
	TestFalse(TEXT("其他令牌不能释放原冻结"), Module->EndWorldSaveSuspension(FGuid::NewGuid(), Error));
	TestTrue(TEXT("拒绝错误释放后原冻结仍有效"), Module->IsWorldSaveSuspended());
	TestFalse(TEXT("其他令牌不能驱动存档预算"), Module->AdvanceSuspendedWorldSave(FGuid::NewGuid(), Error));
	TestFalse(TEXT("冻结期间不能打开项目修改窗口"), Module->BeginProjectMutation());
	++Module->Epoch;
	TestFalse(TEXT("旧代次令牌不能释放新世界"), Module->EndWorldSaveSuspension(Token, Error));
	Module->Epoch = 17;
	TestTrue(TEXT("原令牌与原代次共同释放冻结"), Module->EndWorldSaveSuspension(Token, Error));
	TestFalse(TEXT("释放后不再冻结"), Module->IsWorldSaveSuspended());
	TestFalse(TEXT("重复释放不是另一次成功"), Module->EndWorldSaveSuspension(Token, Error));
	AddInfo(TEXT("测试所有权与代次边界，不替代真实暂停世界中的完整存档提交验收。"));
	return true;
}

#endif
