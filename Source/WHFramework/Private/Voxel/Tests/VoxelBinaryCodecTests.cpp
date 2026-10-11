#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Voxel/Save/VoxelBlockEntityCodec.h"
#include "Voxel/Save/InventoryVoxelEntityCodec.h"
#include "NativeGameplayTags.h"
#include "Voxel/Save/VoxelDeltaCodec.h"
#include "Voxel/Serialization/VoxelBinaryCodec.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"
#include "Voxel/Tests/VoxelTestUtilities.h"
#include "Voxel/Network/VoxelNetworkCodec.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelBinaryGrantedCompressionTest,
	"WHFramework.Voxel.Save.GrantedCompression",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelBinaryGrantedCompressionTest::RunTest(const FString& InParameters)
{
	const auto Registry = VoxelTest::MakeRegistry();
	FVoxelWorldManifest Manifest;
	Manifest.WorldId = FGuid::NewGuid();
	Manifest.RegistryHash = Registry->Hash;
	Manifest.RecipeHash = VoxelTest::MakeGenerationConfig()->Recipe->RecipeHash;
	FVoxelPersistentSection Section;
	Section.Section = FIntVector(-11, 7, -3);
	Section.Revision = 17;
	Section.Blocks.Add(4095, FVoxelBlockState(1, 0));
	Section.Blocks.Add(0, FVoxelBlockState());
	FVoxelBlockEntityState Container;
	FVoxelBlockEntityState Counter;
	if (!FInventoryVoxelEntityCodec::Register() || !FVoxelBlockEntityCodec::MakeDefault(FInventoryVoxelEntityCodec::Kind, Container)
        || !FVoxelBlockEntityCodec::MakeDefault(2, Counter)) return false;
	Section.Entities.Add(4095, MoveTemp(Container));
	Section.Entities.Add(0, MoveTemp(Counter));
	const FVoxelDeltaEncodeView View{Section.Section, Section.Revision, Section.Blocks, Section.Entities};
	const auto Consumer = FVoxelGenerationConsumerKey::Section(81, FGuid::NewGuid(), 1, Section.Section, 1);
	TArray<uint8> Original;
	if (!TestTrue(TEXT("原引擎 compress2 协议编码成功"), FVoxelDeltaCodec::Encode(Manifest, *Registry, Section, Original))) return false;
	const auto Limited = MakeShared<FVoxelTerrainResources, ESPMode::ThreadSafe>();
	FVoxelTerrainResourceLimits Limits;
	Limits.WorkingBytes = 32 * 1024;
	Limited->Configure(Limits);
	FVoxelBinaryBuffer Refused;
	if (!Limited->Acquire(Consumer, EVoxelTerrainResourcePool::Working,
		VoxelBoundedAllocation::ArrayBytes<uint8>(3), Refused.ResourceLease)) return false;
	Refused.Bytes.Reserve(3);
	Refused.Bytes.Append({7, 8, 9});
	const uint64 Retained = Limited->Snapshot().Used[int32(EVoxelTerrainResourcePool::Working)];
	FString Error;
	TestFalse(TEXT("数组可授予时原生 zlib 内部分配仍受容量约束"),
		FVoxelDeltaCodec::EncodeGranted(Manifest, *Registry, View, Limited, Consumer, Refused, Error));
	TestTrue(TEXT("原生必需工作区超过总上限明确失败"), Error.StartsWith(TEXT("CapacityUnachievable:")));
	TestTrue(TEXT("失败不覆盖原输出正文"), Refused.Bytes == TArray<uint8>({7, 8, 9}));
	TestEqual(TEXT("部分 deflateInit 分配实际释放后才归还"),
		Limited->Snapshot().Used[int32(EVoxelTerrainResourcePool::Working)], Retained);
	Refused = FVoxelBinaryBuffer();
	TestEqual(TEXT("原正文释放后归还输出租约"), Limited->Snapshot().Used[int32(EVoxelTerrainResourcePool::Working)], uint64(0));
	const auto Pressured = MakeShared<FVoxelTerrainResources, ESPMode::ThreadSafe>();
	Limits.WorkingBytes = 1024 * 1024;
	if (!Pressured->Configure(Limits)) return false;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Blocker;
	const auto Other = FVoxelGenerationConsumerKey::Section(81, FGuid::NewGuid(), 1, Section.Section, 2);
	if (!Pressured->Acquire(Other, EVoxelTerrainResourcePool::Working, Limits.WorkingBytes - 32 * 1024, Blocker)) return false;
	FVoxelBinaryBuffer Waiting;
	TestFalse(TEXT("外部占用拒收真实原生分配"),
		FVoxelDeltaCodec::EncodeGranted(Manifest, *Registry, View, Pressured, Consumer, Waiting, Error));
	TestTrue(TEXT("必需工作区可达时保持临时容量分类"), Error.StartsWith(TEXT("CapacityBlocked:")));
	Blocker.Reset();
	TestTrue(TEXT("释放外部占用后同consumer可完成原编码"),
		FVoxelDeltaCodec::EncodeGranted(Manifest, *Registry, View, Pressured, Consumer, Waiting, Error));
	TestTrue(TEXT("重试仍保持原协议字节"), Waiting.Bytes == Original);
	const auto Resources = MakeShared<FVoxelTerrainResources, ESPMode::ThreadSafe>();
	Resources->Configure({});
	FVoxelBinaryBuffer Encoded;
	if (!TestTrue(TEXT("容量足够时实际原生编码成功"),
		FVoxelDeltaCodec::EncodeGranted(Manifest, *Registry, View, Resources, Consumer, Encoded, Error))) return false;
	TestTrue(TEXT("原 wire 与原引擎压缩结果逐字节一致"), Encoded.Bytes == Original);
	TestTrue(TEXT("实际输出 backing 保留租约"), Encoded.ResourceLease && Encoded.Bytes.GetAllocatedSize() <= Encoded.ResourceLease->Bytes());
	TestEqual(TEXT("编码临时区退出后只保留实际输出"),
		Resources->Snapshot().Used[int32(EVoxelTerrainResourcePool::Working)], Encoded.ResourceLease->Bytes());
	FVoxelDeltaDecoder Decoder;
	TSharedPtr<FVoxelPersistentSection, ESPMode::ThreadSafe> Decoded;
	if (!TestTrue(TEXT("原生授予的 inflate 可读回冻结记录"), Decoder.Advance(Encoded.Bytes, Manifest, *Registry,
		Resources, Consumer, EVoxelTerrainResourcePool::Staged, Decoded, Error))) return false;
	TestEqual(TEXT("负坐标和内容版本保持原语义"), Decoded->Section, Section.Section);
	TestEqual(TEXT("切点版本保持"), Decoded->Revision, Section.Revision);
	TestTrue(TEXT("实体正文未丢失"), Decoded->Entities.FindChecked(4095).Payload == Section.Entities.FindChecked(4095).Payload);
	TestEqual(TEXT("解压 Native/Raw/Palette 实际临时区均退出"),
		Resources->Snapshot().Used[int32(EVoxelTerrainResourcePool::Working)], Encoded.ResourceLease->Bytes());
	Encoded = FVoxelBinaryBuffer();
	TestEqual(TEXT("最后一个输出容器退休归还容量"), Resources->Snapshot().Used[int32(EVoxelTerrainResourcePool::Working)], uint64(0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelStringViewValidationTest,
	"WHFramework.Voxel.Save.StringViewValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelStringViewValidationTest::RunTest(const FString& InParameters)
{
	const TArray<TArray<uint8>> Cases = {
		{}, {'a', ':', 'b'}, {0xe4, 0xb8, 0xad}, {0xf0, 0x9f, 0x8c, 0x8d},
		{0}, {0xc0, 0x80}, {0x80}, {0xed, 0xa0, 0x80}, {0xf4, 0x90, 0x80, 0x80}, {0xe4, 0xb8}
	};
	for (const auto& Bytes : Cases)
	{
		FVoxelByteWriter Writer(1024);
		Writer.Blob(Bytes, 256);
		TArray<uint8> Stored;
		if (!Writer.Finish(Stored)) return false;
		FVoxelByteReader Original(Stored);
		Original.String(256);
		FVoxelByteReader Borrowed(Stored);
		const auto View = Borrowed.StringView(256);
		TestEqual(TEXT("无分配 UTF8 视图与原 roundtrip 校验结果一致"), Borrowed.End(), Original.End());
		if (Borrowed.End()) TestEqual(TEXT("视图借用原字节且不改变长度"), View.Num(), Bytes.Num());
	}
	FVoxelBlockEntityState Container;
	Container.Kind = 1;
	FVoxelByteWriter Writer(32768);
	Writer.U8(27);
	for (int32 Index = 0; Index < 27; ++Index)
	{
		Writer.String(Index == 0 ? TEXT("None:Something") : TEXT(""), 256);
		Writer.I32(Index == 0 ? 1 : 0);
		Writer.I32(0);
	}
	Writer.Finish(Container.Payload);
	TestFalse(TEXT("旧 Kind1 容器即使字节结构完整也拒绝"), FVoxelBlockEntityCodec::Validate(Container));
    FInventoryItemDefinition Invalid;
    Invalid.ItemId = FPrimaryAssetId(TEXT("None:Something"));
    Invalid.MaxStack = 1;
    FInventoryItemRegistry ItemRegistry;
    FString Error;
    const TArray<FInventoryItemDefinition> Definitions = {Invalid};
    TestFalse(TEXT("冻结定义目录拒绝 None 类型资产"), ItemRegistry.Initialize(Definitions, Error));
	return true;
}

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_InventoryVoxelCodec_Storage, "Inventory.Test.VoxelCodec.Storage");

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryVoxelEnvelopeTest,
    "WHFramework.Voxel.Save.InventoryEnvelopeAndPrivacy",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInventoryVoxelEnvelopeTest::RunTest(const FString& InParameters)
{
    FInventoryItemDefinition Ore;
    Ore.ItemId = FPrimaryAssetId(TEXT("Raw:EnvelopeTestOre"));
    Ore.MaxStack = 64;
    FInventoryItemDefinition Unique;
    Unique.ItemId = FPrimaryAssetId(TEXT("Equip:EnvelopeTestUnique"));
    Unique.StackPolicy = EInventoryStackPolicy::Unique;
    Unique.MaxStack = 1;
    Unique.MaxLevel = 10;
    const TArray<FInventoryItemDefinition> Definitions = {Ore, Unique};
    FInventoryItemRegistry Registry;
    FString Error;
    if (!TestTrue(TEXT("纯值定义可初始化"), Registry.Initialize(Definitions, Error))) return false;
    FInventorySlotGroupDefinition Group;
    Group.Group = TAG_InventoryVoxelCodec_Storage;
    Group.Count = 27;
    const TArray<FInventorySlotGroupDefinition> Groups = {Group};
    FInventoryProfileDefinition Profile;
    if (!FInventoryProfileDefinition::Build(FPrimaryAssetId(TEXT("InventoryProfile:VoxelEnvelopeTest")), Groups, Profile, Error)) return false;
    TMap<FPrimaryAssetId, FInventoryProfileDefinition> Profiles;
    Profiles.Add(Profile.ProfileId, Profile);
    FInventorySnapshot Snapshot = Profile.CreateState(FGuid(1, 2, 3, 4), FGuid(5, 6, 7, 8), FGuid(9, 10, 11, 12));
    Snapshot.Generation = 3;
    Snapshot.Revision = 91;
    Snapshot.Slots[0].Item = FAbilityItem(Ore.ItemId, 64);
    Snapshot.Slots[0].bFavorite = true;
    Snapshot.Slots[26].Item = FAbilityItem(Unique.ItemId, 1, 5);
    Snapshot.Slots[26].Item.InstanceId = FGuid(20, 21, 22, 23);
    FVoxelBlockEntityState Entity;
    if (!TestTrue(TEXT("统一库存实体Codec注册"), FInventoryVoxelEntityCodec::Register())
        || !TestTrue(TEXT("实体完整快照可编码"), FInventoryVoxelEntityCodec::Encode(Snapshot, Registry, Profiles, Entity, Error))) return false;
    TestTrue(TEXT("Worker只校验信封可接受完整持久态"), FVoxelBlockEntityCodec::Validate(Entity));
    TestFalse(TEXT("完整实体不被误认公开摘要"), FInventoryVoxelEntityCodec::IsPublic(Entity));
    FInventorySnapshot Restored;
    TestTrue(TEXT("GT可恢复统一库存"), FInventoryVoxelEntityCodec::Decode(Entity, Registry, Profiles, Restored, Error));
    TestTrue(TEXT("身份、Revision、收藏、尾槽唯一实例全部一致"), Restored == Snapshot);
    FVoxelBlockEntityState Damaged = Entity;
    Damaged.Payload.Last() ^= 0x40;
    TestFalse(TEXT("损坏持久信封CRC拒绝"), FVoxelBlockEntityCodec::Validate(Damaged));
    TestFalse(TEXT("损坏正文不能改写已有输出"), FInventoryVoxelEntityCodec::Decode(Damaged, Registry, Profiles, Restored, Error));
    TestTrue(TEXT("失败后已有输出保持"), Restored == Snapshot);
    Damaged = Entity;
    ++Damaged.Schema;
    TestFalse(TEXT("未知实体Schema拒绝"), FVoxelBlockEntityCodec::Validate(Damaged));
    Damaged = Entity;
    Damaged.Payload.SetNum(FInventoryVoxelEntityCodec::MaximumBytes + 1);
    TestFalse(TEXT("超上限实体在解析前拒绝"), FInventoryVoxelEntityCodec::Validate(Damaged));

    FVoxelBlockEntityState Public;
    FInventoryVoxelEntityCodec::MakePublic(Public);
    TestEqual(TEXT("普通Section只需八字节公共标记"), Public.Payload.Num(), 8);
    TestTrue(TEXT("公共摘要具有独立明确模式"), FInventoryVoxelEntityCodec::IsPublic(Public));
    TestFalse(TEXT("公开摘要不得反向恢复私有物品"), FInventoryVoxelEntityCodec::Decode(Public, Registry, Profiles, Restored, Error));
    TestTrue(TEXT("隐私摘要解码失败不生成伪空箱"), Restored == Snapshot);
    FVoxelBlockEntityState Natural;
    TestTrue(TEXT("自然箱子显式未初始化态可由默认工厂创建"),
        FVoxelBlockEntityCodec::MakeDefault(FInventoryVoxelEntityCodec::Kind, Natural));
    TestTrue(TEXT("自然箱未初始化标志准确"), FInventoryVoxelEntityCodec::IsUninitialized(Natural));
    TestFalse(TEXT("未初始化态不是公开副本"), FInventoryVoxelEntityCodec::IsPublic(Natural));
    TestFalse(TEXT("未初始化态不能当作已创建库存恢复"), FInventoryVoxelEntityCodec::Decode(Natural, Registry, Profiles, Restored, Error));
    TestTrue(TEXT("默认工厂的预算契约不扩张到持久快照上限"),
        Natural.Payload.Num() <= FVoxelBlockEntityCodec::MaximumDefaultPayloadBytes);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelEditStoreIdentityTest,
    "WHFramework.Voxel.Network.EditStoreIdentity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelEditStoreIdentityTest::RunTest(const FString& InParameters)
{
    FVoxelEditIntent Intent;
    Intent.RequestId = 41;
    Intent.InventoryStoreId = FGuid::NewGuid();
    Intent.Action = EVoxelEditAction::BreakCancel;
    Intent.Direction = FVector::ForwardVector;
    TArray<uint8> Bytes;
    if (!TestTrue(TEXT("新协议携带分支身份"), FVoxelNetworkCodec::EncodeIntent(Intent, Bytes))) return false;
    FVoxelEditIntent Decoded;
    TestTrue(TEXT("请求分支身份逐字节恢复"), FVoxelNetworkCodec::DecodeIntent(Bytes, Decoded) &&
        Decoded.InventoryStoreId == Intent.InventoryStoreId && Decoded.RequestId == Intent.RequestId);
    const FGuid Preserved = Decoded.InventoryStoreId;
    for (int32 Index = 8; Index < 24; ++Index) Bytes[Index] = 0;
    TestFalse(TEXT("无分支身份的网络请求拒绝"), FVoxelNetworkCodec::DecodeIntent(Bytes, Decoded));
    TestEqual(TEXT("失败解码不覆盖已验证输出"), Decoded.InventoryStoreId, Preserved);
    Intent.InventoryStoreId.Invalidate();
    TestFalse(TEXT("客户端未完成分支握手时不编码写请求"), FVoxelNetworkCodec::EncodeIntent(Intent, Bytes));
    return true;
}

#endif

