#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Voxel/Save/VoxelBlockEntityCodec.h"
#include "Voxel/Save/VoxelDeltaCodec.h"
#include "Voxel/Serialization/VoxelBinaryCodec.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"
#include "Voxel/Tests/VoxelTestUtilities.h"

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
	if (!FVoxelBlockEntityCodec::MakeDefault(1, Container) || !FVoxelBlockEntityCodec::MakeDefault(2, Counter)) return false;
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
	TArray<FVoxelItemStack> Slots;
	TestFalse(TEXT("无分配容器校验不把 None 类型作为有效资产"), FVoxelBlockEntityCodec::Validate(Container));
	TestFalse(TEXT("原容器读回具有相同资产语义"), FVoxelBlockEntityCodec::DecodeContainer(Container, Slots));
	return true;
}

#endif
