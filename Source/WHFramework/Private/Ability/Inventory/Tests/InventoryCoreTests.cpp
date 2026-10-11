#include "Ability/Inventory/InventoryPlanner.h"
#include "Ability/Inventory/InventorySnapshotCodec.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "NativeGameplayTags.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_InventoryCoreTest_Storage, "Inventory.Test.Storage");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_InventoryCoreTest_Secondary, "Inventory.Test.Secondary");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_InventoryCoreTest_Ore, "Inventory.Test.Ore");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_InventoryCoreTest_Coin, "Inventory.Test.Coin");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_InventoryCoreTest_Equip, "Inventory.Test.Equip");

namespace InventoryCoreTests
{
    struct FFixture
    {
        FInventoryItemRegistry Registry;
        TMap<FPrimaryAssetId, FInventoryProfileDefinition> Profiles;
        FInventoryProfileDefinition Profile;
        FPrimaryAssetId OreId = FPrimaryAssetId(TEXT("Item:TestOre"));
        FPrimaryAssetId CoinId = FPrimaryAssetId(TEXT("Item:TestCoin"));
        FPrimaryAssetId SwordId = FPrimaryAssetId(TEXT("Item:TestSword"));
        FGuid WorldId = FGuid(101, 102, 103, 104);
        FString Error;

        bool Initialize(const int32 InSlotCount = 6)
        {
            FInventoryItemDefinition Ore;
            Ore.ItemId = OreId;
            Ore.MaxStack = 10;
            Ore.MaxLevel = 5;
            Ore.Tags.AddTag(TAG_InventoryCoreTest_Ore);
            FInventoryPropertyRule Bound;
            Bound.Key = TEXT("BoundOwner");
            Bound.Type = EInventoryPropertyType::Guid;
            Ore.PropertyRules.Add(Bound);
            FInventoryPropertyRule Tint;
            Tint.Key = TEXT("Tint");
            Tint.Type = EInventoryPropertyType::Name;
            Tint.AllowedNames = { FName(TEXT("Blue")), FName(TEXT("Red")) };
            Ore.PropertyRules.Add(Tint);
            FInventoryItemDefinition Coin;
            Coin.ItemId = CoinId;
            Coin.MaxStack = 100;
            Coin.MaxLevel = 0;
            Coin.Tags.AddTag(TAG_InventoryCoreTest_Coin);
            FInventoryItemDefinition Sword;
            Sword.ItemId = SwordId;
            Sword.StackPolicy = EInventoryStackPolicy::Unique;
            Sword.MaxStack = 1;
            Sword.MaxLevel = 10;
            TArray<FInventoryItemDefinition> Definitions = { Ore, Coin, Sword };
            if (!Registry.Initialize(Definitions, Error))
            {
                return false;
            }
            FInventorySlotGroupDefinition Group;
            Group.Group = TAG_InventoryCoreTest_Storage;
            Group.Count = InSlotCount;
            TArray<FInventorySlotGroupDefinition> Groups = { Group };
            if (!FInventoryProfileDefinition::Build(FPrimaryAssetId(TEXT("InventoryProfile:Test")), Groups, Profile, Error))
            {
                return false;
            }
            Profiles.Add(Profile.ProfileId, Profile);
            return true;
        }

        FInventorySnapshot State(const uint32 InId) const
        {
            return Profile.CreateState(FGuid(InId, 1, 2, 3), WorldId, FGuid(InId, 5, 6, 7));
        }

        FInventoryOperation Add(const FInventorySnapshot& InState, const FPrimaryAssetId& InItemId, const int32 InCount) const
        {
            FInventoryOperation Operation;
            Operation.Type = EInventoryOperationType::Add;
            Operation.TargetInventoryId = InState.InventoryId;
            Operation.Item = FAbilityItem(InItemId);
            Operation.Requested = InCount;
            return Operation;
        }

        EInventoryResultCode Plan(TConstArrayView<FInventorySnapshot> InStates, TConstArrayView<FInventoryOperation> InOperations, FInventoryPlan& OutPlan)
        {
            const FInventoryPlannerContext Context(Registry, Profiles);
            return FInventoryPlanner::PlanBatch(Context, InStates, InOperations, OutPlan, Error);
        }

        static void Apply(const FInventoryPlan& InPlan, TArray<FInventorySnapshot>& InOutStates)
        {
            for (const FInventorySnapshot& After : InPlan.After)
            {
                FInventorySnapshot* Existing = InOutStates.FindByPredicate([&After](const FInventorySnapshot& InState)
                {
                    return InState.InventoryId == After.InventoryId;
                });
                if (Existing)
                {
                    *Existing = After;
                }
            }
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryCoreDataTest, "WHFramework.Inventory.Core.DataAndRegistry", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInventoryCoreDataTest::RunTest(const FString& Parameters)
{
    InventoryCoreTests::FFixture Fixture;
    if (!TestTrue(TEXT("测试定义初始化"), Fixture.Initialize()))
    {
        return false;
    }
    FString Error;
    TestTrue(TEXT("唯一规范空栈"), Fixture.Registry.ValidateItem(FAbilityItem(), Error));
    FAbilityItem Invalid(Fixture.OreId, 0);
    TestFalse(TEXT("有效身份零数量拒绝"), Fixture.Registry.ValidateItem(Invalid, Error));
    Invalid.Count = -1;
    TestFalse(TEXT("负数量拒绝"), Fixture.Registry.ValidateItem(Invalid, Error));
    Invalid.Count = 11;
    TestFalse(TEXT("超堆叠上限拒绝"), Fixture.Registry.ValidateItem(Invalid, Error));
    FAbilityItem Ore(Fixture.OreId, 3, 1);
    FAbilityItem OtherLevel(Fixture.OreId, 3, 2);
    TestFalse(TEXT("不同等级不能合堆"), Ore.CanStackWith(OtherLevel));
    FInventoryItemProperty Tint;
    Tint.Key = TEXT("Tint");
    Tint.Type = EInventoryPropertyType::Name;
    Tint.NameValue = TEXT("Red");
    Ore.Properties.Add(Tint);
    TestTrue(TEXT("白名单属性合法"), Fixture.Registry.ValidateItem(Ore, Error));
    FAbilityItem BlueOre = Ore;
    BlueOre.Properties[0].NameValue = TEXT("Blue");
    TestFalse(TEXT("不同属性不能合堆"), Ore.CanStackWith(BlueOre));
    Ore.Properties[0].IntValue = 1;
    TestFalse(TEXT("非活跃字段非零拒绝"), Fixture.Registry.ValidateItem(Ore, Error));
    Ore.Properties[0].IntValue = 0;
    Ore.Properties[0].SchemaVersion = 2;
    TestFalse(TEXT("未知属性版本拒绝"), Fixture.Registry.ValidateItem(Ore, Error));
    FAbilityItem Sword(Fixture.SwordId);
    TestFalse(TEXT("唯一物品缺少实例身份拒绝"), Fixture.Registry.ValidateItem(Sword, Error));
    Sword.InstanceId = FGuid(10, 20, 30, 40);
    TestTrue(TEXT("唯一物品单件合法"), Fixture.Registry.ValidateItem(Sword, Error));
    TestFalse(TEXT("唯一物品不可合堆"), Sword.CanStackWith(Sword));
    TArray<FInventoryItemDefinition> Definitions;
    Fixture.Registry.GetDefinitions(Definitions);
    TestFalse(TEXT("已冻结注册表拒绝重新初始化"), Fixture.Registry.Initialize(Definitions, Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryCoreAtomicTest, "WHFramework.Inventory.Core.ExactPartialAndBatch", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInventoryCoreAtomicTest::RunTest(const FString& Parameters)
{
    InventoryCoreTests::FFixture Fixture;
    if (!TestTrue(TEXT("测试定义初始化"), Fixture.Initialize(2)))
    {
        return false;
    }
    FInventorySnapshot State = Fixture.State(1);
    State.Slots[0].Item = FAbilityItem(Fixture.OreId, 7);
    State.Slots[1].Item = FAbilityItem(Fixture.CoinId, 100);
    TArray<FInventorySnapshot> States = { State };
    TArray<FInventoryOperation> Operations = { Fixture.Add(State, Fixture.OreId, 7) };
    FInventoryPlan Plan;
    TestTrue(TEXT("精确添加空间不足拒绝"), Fixture.Plan(States, Operations, Plan) == EInventoryResultCode::NoCapacity);
    TestTrue(TEXT("失败无变化列表"), Plan.After.IsEmpty() && Plan.Changes.ChangedSlots.IsEmpty());
    TestTrue(TEXT("输入快照未修改"), States[0] == State);
    Operations[0].AmountMode = EInventoryAmountMode::UpTo;
    TestTrue(TEXT("部分添加成功"), Fixture.Plan(States, Operations, Plan) == EInventoryResultCode::Partial);
    TestEqual(TEXT("实际接受三个"), Plan.AcceptedCount, int64(3));
    TestEqual(TEXT("余量四个"), Plan.RemainingCount, int64(4));
    TestEqual(TEXT("版本只增加一次"), Plan.After[0].Revision, uint64(1));
    State.Slots[0].Item.Count = 8;
    States[0] = State;
    FInventoryOperation Remove;
    Remove.Type = EInventoryOperationType::Remove;
    Remove.SourceInventoryId = State.InventoryId;
    Remove.Filter.ItemId = Fixture.OreId;
    Remove.Requested = 5;
    Operations = { Remove, Remove };
    TestTrue(TEXT("同批重复消费从同工作集扣除"), Fixture.Plan(States, Operations, Plan) == EInventoryResultCode::InsufficientItems);
    TestTrue(TEXT("整批失败不留下第一笔消费"), Plan.After.IsEmpty() && States[0].Slots[0].Item.Count == 8);
    Remove.Requested = 8;
    Operations = { Remove, Fixture.Add(State, Fixture.CoinId, 100) };
    TestTrue(TEXT("材料消费释放容量给产物"), Fixture.Plan(States, Operations, Plan) == EInventoryResultCode::Success);
    TestEqual(TEXT("材料及产物同笔版本"), Plan.After[0].Revision, uint64(1));
    TestEqual(TEXT("新产物进入释放格位"), Plan.After[0].Slots[0].Item.Count, 100);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryCoreMoveTest, "WHFramework.Inventory.Core.MoveSplitMergeFavorite", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInventoryCoreMoveTest::RunTest(const FString& Parameters)
{
    InventoryCoreTests::FFixture Fixture;
    if (!TestTrue(TEXT("测试定义初始化"), Fixture.Initialize(3)))
    {
        return false;
    }
    FInventorySnapshot State = Fixture.State(1);
    State.Slots[0].Item = FAbilityItem(Fixture.OreId, 8);
    State.Slots[0].bFavorite = true;
    State.Slots[2].Item = FAbilityItem(Fixture.OreId, 9);
    FInventoryOperation Operation;
    Operation.Type = EInventoryOperationType::Split;
    Operation.SourceInventoryId = State.InventoryId;
    Operation.TargetInventoryId = State.InventoryId;
    Operation.SourceSlot = State.Slots[0].Key;
    Operation.TargetSlot = State.Slots[1].Key;
    TArray<FInventorySnapshot> States = { State };
    FInventoryPlan Plan;
    for (const int32 InvalidCount : { 0, -1, 8, 9 })
    {
        Operation.Requested = InvalidCount;
        TArray<FInventoryOperation> Operations = { Operation };
        const EInventoryResultCode Code = Fixture.Plan(States, Operations, Plan);
        TestTrue(TEXT("非法拆分数量拒绝"), Code != EInventoryResultCode::Success && Code != EInventoryResultCode::Partial);
        TestTrue(TEXT("非法拆分没有任何输出变更"), Plan.After.IsEmpty());
    }
    Operation.Requested = 3;
    TArray<FInventoryOperation> Operations = { Operation };
    TestTrue(TEXT("拆分成功"), Fixture.Plan(States, Operations, Plan) == EInventoryResultCode::Success);
    TestTrue(TEXT("手动拆分收藏随两部分物品保留"), Plan.After[0].Slots[0].bFavorite && Plan.After[0].Slots[1].bFavorite);
    TestEqual(TEXT("源余五个"), Plan.After[0].Slots[0].Item.Count, 5);
    TestEqual(TEXT("同库存双格只提交一次"), Plan.Changes.Revisions.Num(), 1);
    Operation.Type = EInventoryOperationType::Merge;
    Operation.TargetSlot = State.Slots[2].Key;
    Operation.Requested = 8;
    Operation.AmountMode = EInventoryAmountMode::UpTo;
    Operations = { Operation };
    TestTrue(TEXT("合堆按目标容量部分成功"), Fixture.Plan(States, Operations, Plan) == EInventoryResultCode::Partial);
    TestEqual(TEXT("只合入一个"), Plan.AcceptedCount, int64(1));
    TestTrue(TEXT("合堆收藏取并集"), Plan.After[0].Slots[2].bFavorite);
    Operation.bAutomatic = true;
    Operations = { Operation };
    TestTrue(TEXT("自动操作保护收藏源"), Fixture.Plan(States, Operations, Plan) == EInventoryResultCode::SlotRuleRejected);
    State.Slots[0].bFavorite = false;
    State.Slots[0].bLocked = true;
    States[0] = State;
    Operation.bAutomatic = false;
    Operations = { Operation };
    TestTrue(TEXT("锁格禁止人工移动"), Fixture.Plan(States, Operations, Plan) == EInventoryResultCode::ItemLocked);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryCoreExchangeTest, "WHFramework.Inventory.Core.FullInventoryExchange", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInventoryCoreExchangeTest::RunTest(const FString& Parameters)
{
    InventoryCoreTests::FFixture Fixture;
    if (!TestTrue(TEXT("测试定义初始化"), Fixture.Initialize(1)))
    {
        return false;
    }
    FInventorySnapshot Seller = Fixture.State(1);
    FInventorySnapshot Buyer = Fixture.State(2);
    Seller.Slots[0].Item = FAbilityItem(Fixture.OreId, 10);
    Buyer.Slots[0].Item = FAbilityItem(Fixture.CoinId, 100);
    FInventoryOperation Exchange;
    Exchange.Type = EInventoryOperationType::Exchange;
    FInventoryExchangeLine Goods;
    Goods.SourceInventoryId = Seller.InventoryId;
    Goods.TargetInventoryId = Buyer.InventoryId;
    Goods.Filter.ItemId = Fixture.OreId;
    Goods.Requested = 10;
    FInventoryExchangeLine Payment;
    Payment.SourceInventoryId = Buyer.InventoryId;
    Payment.TargetInventoryId = Seller.InventoryId;
    Payment.Filter.ItemId = Fixture.CoinId;
    Payment.Requested = 100;
    Exchange.ExchangeLines = { Goods, Payment };
    TArray<FInventorySnapshot> States = { Buyer, Seller };
    TArray<FInventoryOperation> Operations = { Exchange };
    FInventoryPlan Plan;
    TestTrue(TEXT("双方满包但最终可互换时成功"), Fixture.Plan(States, Operations, Plan) == EInventoryResultCode::Success);
    TestTrue(TEXT("交换没有授权生成消费"), Plan.QuantityDeltas.IsEmpty());
    InventoryCoreTests::FFixture::Apply(Plan, States);
    TestTrue(TEXT("买方持有商品"), States[0].Slots[0].Item.ItemId == Fixture.OreId);
    TestTrue(TEXT("卖方持有价款"), States[1].Slots[0].Item.ItemId == Fixture.CoinId);
    Exchange.ExchangeLines[1].Requested = 101;
    Operations = { Exchange };
    States = { Buyer, Seller };
    TestTrue(TEXT("任一支付不足整批失败"), Fixture.Plan(States, Operations, Plan) == EInventoryResultCode::InsufficientItems);
    TestTrue(TEXT("失败不扣货不扣款"), Plan.After.IsEmpty() && States[0] == Buyer && States[1] == Seller);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryCoreProfileTest, "WHFramework.Inventory.Core.ProfileOrderSortAndRules", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInventoryCoreProfileTest::RunTest(const FString& Parameters)
{
    InventoryCoreTests::FFixture Fixture;
    if (!TestTrue(TEXT("测试定义初始化"), Fixture.Initialize(4)))
    {
        return false;
    }
    FInventorySlotGroupDefinition Storage;
    Storage.Group = TAG_InventoryCoreTest_Storage;
    Storage.Count = 2;
    Storage.Order = 5;
    FInventorySlotGroupDefinition Secondary;
    Secondary.Group = TAG_InventoryCoreTest_Secondary;
    Secondary.Count = 1;
    Secondary.Order = 1;
    TArray<FInventorySlotGroupDefinition> Groups = { Storage, Secondary };
    FInventoryProfileDefinition First;
    FInventoryProfileDefinition Second;
    FString Error;
    TestTrue(TEXT("第一份布局合法"), FInventoryProfileDefinition::Build(Fixture.Profile.ProfileId, Groups, First, Error));
    Swap(Groups[0], Groups[1]);
    TestTrue(TEXT("输入顺序改变后布局合法"), FInventoryProfileDefinition::Build(Fixture.Profile.ProfileId, Groups, Second, Error));
    TestTrue(TEXT("布局顺序由Order决定"), First.Slots[0].Key == Second.Slots[0].Key && First.Slots[0].Key.Group == TAG_InventoryCoreTest_Secondary);
    FInventorySnapshot State = Fixture.State(1);
    State.Slots[0].Item = FAbilityItem(Fixture.OreId, 2);
    State.Slots[0].bFavorite = true;
    State.Slots[1].Item = FAbilityItem(Fixture.OreId, 5);
    State.Slots[2].Item = FAbilityItem(Fixture.CoinId, 4);
    State.Slots[3].Item = FAbilityItem(Fixture.OreId, 4);
    FInventoryOperation Operation;
    Operation.Type = EInventoryOperationType::Sort;
    Operation.SourceInventoryId = State.InventoryId;
    TArray<FInventorySnapshot> States = { State };
    TArray<FInventoryOperation> Operations = { Operation };
    FInventoryPlan Plan;
    TestTrue(TEXT("稳定整理成功"), Fixture.Plan(States, Operations, Plan) == EInventoryResultCode::Success);
    TestTrue(TEXT("收藏槽保持原值"), Plan.After[0].Slots[0] == State.Slots[0]);
    TestEqual(TEXT("相同普通栈被压到九个"), Plan.After[0].Slots[2].Item.Count, 9);
    InventoryCoreTests::FFixture::Apply(Plan, States);
    TestTrue(TEXT("再次整理无变化"), Fixture.Plan(States, Operations, Plan) == EInventoryResultCode::NoChange);
    Fixture.Profile.Slots[0].Rule.RequiredAllTags.AddTag(TAG_InventoryCoreTest_Ore);
    Fixture.Profile.Slots[1].Rule.RequiredAllTags.AddTag(TAG_InventoryCoreTest_Coin);
    Fixture.Profiles[Fixture.Profile.ProfileId] = Fixture.Profile;
    State = Fixture.State(1);
    State.Slots[0].Item = FAbilityItem(Fixture.OreId, 1);
    State.Slots[1].Item = FAbilityItem(Fixture.CoinId, 1);
    Operation.Type = EInventoryOperationType::Swap;
    Operation.SourceInventoryId = State.InventoryId;
    Operation.TargetInventoryId = State.InventoryId;
    Operation.SourceSlot = State.Slots[0].Key;
    Operation.TargetSlot = State.Slots[1].Key;
    States = { State };
    Operations = { Operation };
    TestTrue(TEXT("互不接受物品的双向交换拒绝"), Fixture.Plan(States, Operations, Plan) == EInventoryResultCode::SlotRuleRejected);
    TestTrue(TEXT("规则拒绝不改变原布局"), Plan.After.IsEmpty());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryCoreIdentityTest, "WHFramework.Inventory.Core.IdentityBoundAndTransform", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInventoryCoreIdentityTest::RunTest(const FString& Parameters)
{
    InventoryCoreTests::FFixture Fixture;
    if (!TestTrue(TEXT("测试定义初始化"), Fixture.Initialize(2)))
    {
        return false;
    }
    FInventorySnapshot Source = Fixture.State(1);
    FInventorySnapshot Target = Fixture.State(2);
    Source.Slots[0].Item = FAbilityItem(Fixture.SwordId, 1, 2);
    Source.Slots[0].Item.InstanceId = FGuid(90, 91, 92, 93);
    Target.Slots[0].Item = Source.Slots[0].Item;
    TArray<FInventorySnapshot> States = { Source, Target };
    FString Error;
    TestFalse(TEXT("全工作集重复实例拒绝"), FInventoryPlanner::ValidateUniqueInstances(States, Error));
    Target.Slots[0].Item = FAbilityItem();
    States = { Source, Target };
    FInventoryOperation Transform;
    Transform.Type = EInventoryOperationType::Transform;
    Transform.SourceInventoryId = Source.InventoryId;
    Transform.SourceSlot = Source.Slots[0].Key;
    Transform.Item = FAbilityItem(Fixture.SwordId, 1, 3);
    TArray<FInventoryOperation> Operations = { Transform };
    FInventoryPlan Plan;
    TestTrue(TEXT("实例升级成功"), Fixture.Plan(States, Operations, Plan) == EInventoryResultCode::Success);
    TestTrue(TEXT("升级保持唯一实例身份"), Plan.After[0].Slots[0].Item.InstanceId == Source.Slots[0].Item.InstanceId);
    TestEqual(TEXT("升级等级改变"), Plan.After[0].Slots[0].Item.Level, 3);
    TestEqual(TEXT("旧新签名记录消费和生成"), Plan.QuantityDeltas.Num(), 2);
    Source.Slots[1].Item = FAbilityItem(Fixture.OreId, 4);
    FInventoryItemProperty Bound;
    Bound.Key = TEXT("BoundOwner");
    Bound.Type = EInventoryPropertyType::Guid;
    Bound.GuidValue = Source.OwnerId;
    Source.Slots[1].Item.Properties.Add(Bound);
    FInventoryOperation Transfer;
    Transfer.Type = EInventoryOperationType::Transfer;
    Transfer.SourceInventoryId = Source.InventoryId;
    Transfer.TargetInventoryId = Target.InventoryId;
    Transfer.SourceSlot = Source.Slots[1].Key;
    Transfer.Requested = 4;
    States = { Source, Target };
    Operations = { Transfer };
    TestTrue(TEXT("绑定物品禁止转到其他所有者"), Fixture.Plan(States, Operations, Plan) == EInventoryResultCode::ItemBound);
    TestTrue(TEXT("绑定拒绝没有任何物品变化"), Plan.After.IsEmpty());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryCoreCodecTest, "WHFramework.Inventory.Core.CodecAndInt64Query", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInventoryCoreCodecTest::RunTest(const FString& Parameters)
{
    InventoryCoreTests::FFixture Fixture;
    if (!TestTrue(TEXT("测试定义初始化"), Fixture.Initialize(3)))
    {
        return false;
    }
    FInventorySnapshot State = Fixture.State(1);
    State.Slots[0].Item = FAbilityItem(Fixture.OreId, 8, 1);
    State.Slots[1].Item = FAbilityItem(Fixture.SwordId, 1, 2);
    State.Slots[1].Item.InstanceId = FGuid(80, 81, 82, 83);
    State.Slots[0].bFavorite = true;
    TArray<uint8> Bytes;
    FString Error;
    if (!TestTrue(TEXT("新格式编码成功"), FInventorySnapshotCodec::Encode(State, Fixture.Registry, Fixture.Profiles, Bytes, Error)))
    {
        return false;
    }
    FInventorySnapshot Restored;
    TestTrue(TEXT("新格式解码成功"), FInventorySnapshotCodec::Decode(Bytes, Fixture.Registry, Fixture.Profiles, Restored, Error));
    TestTrue(TEXT("所有快照字段往返相等"), Restored == State);
    Bytes[Bytes.Num() / 2] ^= 0x40;
    TestFalse(TEXT("损坏字节CRC拒绝"), FInventorySnapshotCodec::Decode(Bytes, Fixture.Registry, Fixture.Profiles, Restored, Error));
    State.SchemaVersion = 1;
    TestFalse(TEXT("旧Schema不读取"), FInventorySnapshotCodec::Encode(State, Fixture.Registry, Fixture.Profiles, Bytes, Error));
    FInventoryItemRegistry LargeRegistry;
    FInventoryItemDefinition Definition;
    Definition.ItemId = Fixture.OreId;
    Definition.MaxStack = MAX_int32;
    TArray<FInventoryItemDefinition> Definitions = { Definition };
    TestTrue(TEXT("大数量定义合法"), LargeRegistry.Initialize(Definitions, Error));
    State = Fixture.State(1);
    State.Slots[0].Item = FAbilityItem(Fixture.OreId, MAX_int32);
    State.Slots[1].Item = FAbilityItem(Fixture.OreId, MAX_int32);
    FInventoryQueryRequest Request;
    Request.Filter.ItemId = Fixture.OreId;
    Request.CapacityItem = FAbilityItem(Fixture.OreId);
    FInventoryQueryResult Result;
    const FInventoryPlannerContext Context(LargeRegistry, Fixture.Profiles);
    TestTrue(TEXT("int64查询成功"), FInventoryPlanner::Query(Context, State, Request, Result, Error) == EInventoryResultCode::Success);
    TestEqual(TEXT("总数不发生int32截断"), Result.MatchingCount, int64(MAX_int32) * 2);
    TestEqual(TEXT("剩余容量准确"), Result.Capacity, int64(MAX_int32));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryCoreRandomTest, "WHFramework.Inventory.Core.RandomConservation50000", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInventoryCoreRandomTest::RunTest(const FString& Parameters)
{
    InventoryCoreTests::FFixture Fixture;
    if (!TestTrue(TEXT("测试定义初始化"), Fixture.Initialize(6)))
    {
        return false;
    }
    FRandomStream Random(20261011);
    TArray<FInventorySnapshot> States = { Fixture.State(1), Fixture.State(2), Fixture.State(3) };
    for (int32 StateIndex = 0; StateIndex < States.Num(); ++StateIndex)
    {
        States[StateIndex].Slots[0].Item = FAbilityItem(Fixture.OreId, 5 + StateIndex);
        States[StateIndex].Slots[1].Item = FAbilityItem(Fixture.CoinId, 30 + StateIndex);
        States[StateIndex].Slots[2].Item = FAbilityItem(Fixture.OreId, 2, 1);
    }
    for (int32 Iteration = 0; Iteration < 50000; ++Iteration)
    {
        const TArray<FInventorySnapshot> Before = States;
        const int32 SourceIndex = Random.RandRange(0, States.Num() - 1);
        const int32 TargetIndex = Random.RandRange(0, States.Num() - 1);
        const int32 SourceSlotIndex = Random.RandRange(0, 5);
        const int32 TargetSlotIndex = Random.RandRange(0, 5);
        FInventoryOperation Operation;
        Operation.SourceInventoryId = States[SourceIndex].InventoryId;
        Operation.TargetInventoryId = States[TargetIndex].InventoryId;
        Operation.SourceSlot = States[SourceIndex].Slots[SourceSlotIndex].Key;
        Operation.TargetSlot = States[TargetIndex].Slots[TargetSlotIndex].Key;
        Operation.Requested = Random.RandRange(1, 12);
        Operation.AmountMode = Random.RandRange(0, 1) == 0 ? EInventoryAmountMode::Exact : EInventoryAmountMode::UpTo;
        switch (Random.RandRange(0, 5))
        {
            case 0:
                Operation.Type = EInventoryOperationType::Move;
                break;
            case 1:
                Operation.Type = EInventoryOperationType::Swap;
                break;
            case 2:
                Operation.Type = EInventoryOperationType::Merge;
                break;
            case 3:
                Operation.Type = EInventoryOperationType::Split;
                Operation.AmountMode = EInventoryAmountMode::Exact;
                break;
            case 4:
                Operation.Type = EInventoryOperationType::Transfer;
                Operation.TargetSlot = FInventorySlotKey();
                break;
            default:
                Operation.Type = EInventoryOperationType::Sort;
                Operation.SourceSlot = FInventorySlotKey();
                break;
        }
        const TArray<FInventoryOperation> Operations = { Operation };
        FInventoryPlan Plan;
        FInventoryPlan Repeated;
        const EInventoryResultCode Code = Fixture.Plan(States, Operations, Plan);
        const EInventoryResultCode RepeatCode = Fixture.Plan(States, Operations, Repeated);
        if (Code != RepeatCode || Plan.After != Repeated.After || States != Before)
        {
            AddError(FString::Printf(TEXT("第 %d 次规划不满足确定性或输入不可变。"), Iteration));
            return false;
        }
        if (Code == EInventoryResultCode::Success || Code == EInventoryResultCode::Partial)
        {
            InventoryCoreTests::FFixture::Apply(Plan, States);
            FString Error;
            const TArray<FInventoryQuantityDelta> NoDeltas;
            if (!FInventoryPlanner::ValidateConservation(Before, States, NoDeltas, Error)
                || !FInventoryPlanner::ValidateUniqueInstances(States, Error))
            {
                AddError(FString::Printf(TEXT("第 %d 次命令违反物品不变量：%s"), Iteration, *Error));
                return false;
            }
            for (const FInventoryRevision& Revision : Plan.Changes.Revisions)
            {
                if (Revision.After != Revision.Before + 1)
                {
                    AddError(TEXT("单笔事务没有恰好增加一次版本。"));
                    return false;
                }
            }
        }
        else if (!Plan.After.IsEmpty())
        {
            AddError(TEXT("失败事务泄露了部分变更。"));
            return false;
        }
    }
    return true;
}

#endif
