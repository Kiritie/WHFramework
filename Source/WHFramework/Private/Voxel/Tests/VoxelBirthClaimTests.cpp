#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Components/SceneComponent.h"
#include "Voxel/VoxelWorldReadinessRuntime.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelBirthClaimIdentityTest,
	"WHFramework.Voxel.FineRefactor.Readiness.BirthClaimIdentity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelBirthClaimIdentityTest::RunTest(const FString& Parameters)
{
	FVoxelWorldReadinessRuntime Runtime;
	FVoxelPlayerClaimState State;
	UObject* Pawn = NewObject<USceneComponent>();
	FVoxelPlayerClaimToken Identity;
	Identity.WorldEpoch = 7;
	Identity.SourceId = FGuid::NewGuid();
	Identity.SourceIncarnation = 11;
	Identity.CandidateRevision = 3;
	Identity.PawnIncarnation = FGuid::NewGuid();
	Identity.CandidateCell = FIntVector(-17, 32, 80);
	FVoxelPlayerClaimToken Token;
	TestTrue(TEXT("出生源准备成功"), Runtime.PreparePlayerClaim(State, Identity, Pawn, Token));
	TestTrue(TEXT("新候选必须请求首次验证"), State.bVerificationPending);
	State.bVerificationPending = false;
	FVoxelPlayerClaimToken Again;
	TestTrue(TEXT("重复准备保持同一个令牌"), Runtime.PreparePlayerClaim(State, Identity, Pawn, Again));
	TestTrue(TEXT("身份和序号不重建"), Token == Again);
	TestFalse(TEXT("相同令牌不会反复打断正在进行的验证"), State.bVerificationPending);
	TestFalse(TEXT("准备状态不能提前提交"), Runtime.CommitPlayerClaim(State, Token, Pawn));
	State.bSpawnCoreReady = true;
	TestFalse(TEXT("仅 Core 就绪仍不允许玩家接管"), Runtime.CommitPlayerClaim(State, Token, Pawn));
	State.bResidencyReady = true;
	const auto RejectChangedIdentity = [&](const TCHAR* Label, FVoxelPlayerClaimToken Stale)
	{
		TestFalse(Label, Runtime.CommitPlayerClaim(State, Stale, Pawn));
	};
	auto Stale = Token;
	++Stale.WorldEpoch;
	RejectChangedIdentity(TEXT("旧世界代不能提交"), Stale);
	Stale = Token;
	++Stale.SourceIncarnation;
	RejectChangedIdentity(TEXT("旧源生命期不能提交"), Stale);
	Stale = Token;
	++Stale.CandidateRevision;
	RejectChangedIdentity(TEXT("旧候选不能提交"), Stale);
	Stale = Token;
	++Stale.ControlRevision;
	RejectChangedIdentity(TEXT("旧控制权版本不能提交"), Stale);
	Stale = Token;
	Stale.PawnIncarnation = FGuid::NewGuid();
	RejectChangedIdentity(TEXT("同坐标的新角色不能使用旧角色令牌"), Stale);
	TestFalse(TEXT("另一角色不能抢占活跃准备租约"),
		Runtime.PreparePlayerClaim(State, Identity, NewObject<USceneComponent>(), Again));
	TestTrue(TEXT("完整合同允许接管"), Runtime.CommitPlayerClaim(State, Token, Pawn));
	TestTrue(TEXT("接管保留源身份"), State.Token.SourceId == Identity.SourceId);
	TestFalse(TEXT("接管后旧 Abort 不能撤销正式玩家租约"), Runtime.AbortPlayerClaim(State, Token));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelBirthClaimAbortTest,
	"WHFramework.Voxel.FineRefactor.Readiness.BirthClaimAbort",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelBirthClaimAbortTest::RunTest(const FString& Parameters)
{
	FVoxelWorldReadinessRuntime Runtime;
	FVoxelPlayerClaimState State;
	FVoxelPlayerClaimToken Identity;
	Identity.WorldEpoch = 4;
	Identity.SourceId = FGuid::NewGuid();
	Identity.SourceIncarnation = 9;
	Identity.CandidateRevision = 1;
	Identity.PawnIncarnation = FGuid::NewGuid();
	UObject* Pawn = NewObject<USceneComponent>();
	FVoxelPlayerClaimToken First;
	TestTrue(TEXT("第一角色准备"), Runtime.PreparePlayerClaim(State, Identity, Pawn, First));
	TestTrue(TEXT("撤销临时准备"), Runtime.AbortPlayerClaim(State, First));
	TestFalse(TEXT("撤销清理待验证标记"), State.bVerificationPending);
	TestFalse(TEXT("重复撤销不影响其他租约"), Runtime.AbortPlayerClaim(State, First));
	Identity.PawnIncarnation = FGuid::NewGuid();
	FVoxelPlayerClaimToken Second;
	UObject* Replacement = NewObject<USceneComponent>();
	TestTrue(TEXT("同出生源准备新角色"), Runtime.PreparePlayerClaim(State, Identity, Replacement, Second));
	TestTrue(TEXT("替代角色重新请求验证"), State.bVerificationPending);
	TestTrue(TEXT("保持源与候选身份"), First.SourceId == Second.SourceId && First.CandidateRevision == Second.CandidateRevision);
	TestTrue(TEXT("新角色具有新序号"), First.Serial != Second.Serial);
	State.bSpawnCoreReady = State.bResidencyReady = true;
	TestFalse(TEXT("迟到回调不能解锁替代角色"), Runtime.CommitPlayerClaim(State, First, Pawn));
	TestTrue(TEXT("替代角色用自己的完整合同接管"), Runtime.CommitPlayerClaim(State, Second, Replacement));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelBirthClaimCandidateWakeTest,
	"WHFramework.Voxel.FineRefactor.Readiness.BirthClaimCandidateWake",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelBirthClaimCandidateWakeTest::RunTest(const FString& Parameters)
{
	FVoxelWorldReadinessRuntime Runtime;
	FVoxelPlayerClaimState State;
	FVoxelPlayerClaimToken Identity;
	Identity.WorldEpoch = 3;
	Identity.SourceId = FGuid::NewGuid();
	Identity.SourceIncarnation = 1;
	Identity.CandidateRevision = 3;
	Identity.PawnIncarnation = FGuid::NewGuid();
	Identity.CandidateCell = FIntVector(-8188, -8192, 427);
	UObject* Pawn = NewObject<USceneComponent>();
	FVoxelPlayerClaimToken Third;
	TestTrue(TEXT("准备第三出生候选"), Runtime.PreparePlayerClaim(State, Identity, Pawn, Third));
	State.bVerificationPending = false;
	State.bSpawnCoreReady = State.bResidencyReady = true;
	TestTrue(TEXT("替换候选前撤销旧证明"), Runtime.AbortPlayerClaim(State, Third));
	++Identity.CandidateRevision;
	Identity.CandidateCell = FIntVector(-8192, -8188, 427);
	FVoxelPlayerClaimToken Fourth;
	TestTrue(TEXT("同一 Section 的第四候选可重新准备"), Runtime.PreparePlayerClaim(State, Identity, Pawn, Fourth));
	TestTrue(TEXT("即使世界和来源未变，新候选仍请求验证"), State.bVerificationPending);
	TestFalse(TEXT("新候选不继承旧核心证明"), State.bSpawnCoreReady);
	TestFalse(TEXT("新候选不继承旧驻留证明"), State.bResidencyReady);
	TestFalse(TEXT("旧候选令牌不能提交新位置"), Runtime.CommitPlayerClaim(State, Third, Pawn));
	TestFalse(TEXT("新候选未验证前不能接管"), Runtime.CommitPlayerClaim(State, Fourth, Pawn));
	return true;
}

#endif
