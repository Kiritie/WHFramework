#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Ability/Inventory/InventoryNetworkComponent.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FInventoryConnectionLifecycleTest,
    "WHFramework.Inventory.ConnectionResetLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInventoryConnectionLifecycleTest::RunTest(const FString& InParameters)
{
    (void)InParameters;
    TStrongObjectPtr<UInventoryNetworkComponent> Network(NewObject<UInventoryNetworkComponent>());
    const FGuid OldConnection = FGuid::NewGuid();
    const FGuid OldWorld = FGuid::NewGuid();
    const FGuid OldStore = FGuid::NewGuid();
    const FGuid OldEpoch = FGuid::NewGuid();
    const FGuid OldOwner = FGuid::NewGuid();
    Network->ClientBegin_Implementation(OldConnection, OldWorld, OldStore, OldEpoch, OldOwner);
    int32 ResetCount = 0;
    const FDelegateHandle ResetHandle = Network->OnConnectionReset.AddLambda(
        [this, &ResetCount, OldStore, OldEpoch](const FGuid& Store, const FGuid& Epoch, EInventoryResultCode Reason)
        {
            ++ResetCount;
            TestEqual(TEXT("重置携带原Store"), Store, OldStore);
            TestEqual(TEXT("重置携带原Epoch"), Epoch, OldEpoch);
            TestEqual(TEXT("重置保留真实原因"), Reason, EInventoryResultCode::WorldNotReady);
        });
    Network->ClientReset_Implementation(FGuid::NewGuid(), EInventoryResultCode::WorldNotReady);
    TestEqual(TEXT("旧连接重置不影响当前连接"), ResetCount, 0);
    Network->ClientReset_Implementation(OldConnection, EInventoryResultCode::WorldNotReady);
    TestEqual(TEXT("尚无快照也发布上下文关闭"), ResetCount, 1);
    TestFalse(TEXT("重置后连接身份失效"), Network->ConnectionId.IsValid());
    Network->OnConnectionReset.Remove(ResetHandle);

    Network->ClientBegin_Implementation(OldConnection, OldWorld, OldStore, OldEpoch, OldOwner);
    const FGuid ContainerId = FGuid::NewGuid();
    const FGuid ClosedSession = FGuid::NewGuid();
    const FGuid ReopenedSession = FGuid::NewGuid();
    UInventoryNetworkComponent::FClientView ContainerView;
    ContainerView.SessionId = ClosedSession;
    ContainerView.Snapshot.InventoryId = ContainerId;
    Network->Views.Add(ContainerId, ContainerView);
    Network->LocallyClosedSessions.Add(ClosedSession);
    TestNull(TEXT("主动关闭后旧快照立即不可读"), Network->FindView(ContainerId));
    TestFalse(TEXT("主动关闭后旧会话不可用于请求"), Network->GetSessionId(ContainerId).IsValid());
    int32 IntentionalClosures = 0;
    const FDelegateHandle IntentionalHandle = Network->OnSessionClosed.AddLambda(
        [&IntentionalClosures](const FGuid&, EInventoryResultCode) { ++IntentionalClosures; });
    Network->ClientClosed_Implementation(OldConnection, ContainerId, ClosedSession, EInventoryResultCode::Success);
    TestEqual(TEXT("主动关闭回执不关闭新页面"), IntentionalClosures, 0);
    TestFalse(TEXT("主动关闭回执清理旧视图"), Network->Views.Contains(ContainerId));
    ContainerView.SessionId = ReopenedSession;
    Network->Views.Add(ContainerId, ContainerView);
    Network->LocallyClosedSessions.Add(ClosedSession);
    Network->ClientClosed_Implementation(OldConnection, ContainerId, ClosedSession, EInventoryResultCode::Success);
    TestEqual(TEXT("迟到旧关闭不清新会话"), Network->GetSessionId(ContainerId), ReopenedSession);
    TestEqual(TEXT("迟到旧关闭不通知新页面"), IntentionalClosures, 0);
    Network->ClientClosed_Implementation(OldConnection, ContainerId, ReopenedSession, EInventoryResultCode::OutOfRange);
    TestEqual(TEXT("真正越界关闭仍通知界面"), IntentionalClosures, 1);
    Network->OnSessionClosed.Remove(IntentionalHandle);
    UInventoryNetworkComponent::FPendingRequest Pending;
    Pending.Request.RequestId = 7;
    Network->Pending.Add(7, Pending);
    Network->Views.Add(OldOwner, UInventoryNetworkComponent::FClientView());
    const FGuid NewConnection = FGuid::NewGuid();
    const FGuid NewWorld = FGuid::NewGuid();
    const FGuid NewStore = FGuid::NewGuid();
    const FGuid NewEpoch = FGuid::NewGuid();
    const FGuid NewOwner = FGuid::NewGuid();
    bool bReentered = false;
    int32 StaleResults = 0;
    int32 StaleClosures = 0;
    Network->OnResult.AddLambda([&StaleResults](const FInventoryCommandResult&) { ++StaleResults; });
    Network->OnSessionClosed.AddLambda([&StaleClosures](const FGuid&, EInventoryResultCode) { ++StaleClosures; });
    Network->OnConnectionReset.AddLambda(
        [&bReentered, Target = Network.Get(), NewConnection, NewWorld, NewStore, NewEpoch, NewOwner](const FGuid&, const FGuid&, EInventoryResultCode)
        {
            if (bReentered) return;
            bReentered = true;
            Target->ClientBegin_Implementation(NewConnection, NewWorld, NewStore, NewEpoch, NewOwner);
        });
    Network->ClientBegin_Implementation(FGuid::NewGuid(), FGuid::NewGuid(), FGuid::NewGuid(), FGuid::NewGuid(), FGuid::NewGuid());
    TestTrue(TEXT("重置观察者可建立新上下文"), bReentered);
    TestEqual(TEXT("旧Begin返回栈不覆盖新连接"), Network->ConnectionId, NewConnection);
    TestEqual(TEXT("旧Begin不覆盖新Store"), Network->GetStoreId(), NewStore);
    TestEqual(TEXT("旧Begin不覆盖新Epoch"), Network->GetWorldEpoch(), NewEpoch);
    TestEqual(TEXT("旧Begin不覆盖新玩家库存"), Network->GetOwnerInventoryId(), NewOwner);
    TestEqual(TEXT("新上下文后不分发旧请求结果"), StaleResults, 0);
    TestEqual(TEXT("新上下文后不分发旧会话关闭"), StaleClosures, 0);
    return true;
}

#endif
