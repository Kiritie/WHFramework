#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Voxel/Prefabs/Data/VoxelPrefabData.h"
#include "Voxel/Runtime/VoxelWorldRuntime.h"
#include "Voxel/Geometry/VoxelShapeRegistry.h"
#include "Voxel/Interaction/VoxelRaycast.h"
#include "Voxel/Network/VoxelNetworkTransfer.h"
#include "Voxel/Chunks/VoxelSectionCodec.h"
#include "Voxel/Tasks/VoxelTaskScheduler.h"
namespace
{
struct FFixture
{
	TSharedRef<FVoxelRegistrySnapshot, ESPMode::ThreadSafe> Registry = MakeShared<FVoxelRegistrySnapshot, ESPMode::ThreadSafe>();
	TSharedPtr<const FVoxelGenerationPipeline, ESPMode::ThreadSafe> Generator;
	FFixture()
	{
		for (uint16 I = 0; I < 7; ++I)
		{
			FVoxelRuntimeDefinition D;
			D.TypeId = I;
			D.BlockName = FName(*FString::Printf(TEXT("test:%d"), I));
			D.Shape = I == 6 ? EVoxelShapeKind::Fluid : EVoxelShapeKind::FullCube;
			D.bSolid = I > 0 && I < 6;
			D.bOccludes = D.bSolid;
			Registry->Definitions.Add(D);
			Registry->Names.Add(D.BlockName, I);
		}
		FVoxelGenerationRuntimeConfig C;
		C.Stone = 1;
		C.Dirt = 2;
		C.Grass = 3;
		C.Sand = 4;
		C.Snow = 5;
		C.Water = 6;
		Generator = MakeShared<FVoxelGenerationPipeline, ESPMode::ThreadSafe>(C);
	}
	bool Load(FVoxelWorldRuntime& W, FVoxelSectionKey K)
	{
		auto* S = W.Allocate(K, 0);
		if (!S)
			return false;
		FVoxelSectionStorage B;
		if (!Generator->GenerateSection(K, B))
			return false;
		FVoxelSectionOverlay O;
		O.Key = K;
		return W.PublishLoaded(S->Stamp, MoveTemp(B), O, true);
	}
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelStorageTest, "DreamWorld.Voxel.Phase1.Storage", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVoxelStorageTest::RunTest(const FString&)
{
	FVoxelSectionStorage S;
	S.Fill({1, 0});
	TestEqual(TEXT("Uniform payload"), S.PayloadBytes(), uint64(4));
	S.Set(17, {2, 0});
	TestEqual(TEXT("Changed cell"), S.Get(17).Pack(), FVoxelBlockState{2, 0}.Pack());
	TArray<uint8> B;
	TestTrue(TEXT("Encode"), FVoxelSectionCodec::Encode(S, B));
	FVoxelSectionStorage R;
	TestTrue(TEXT("Decode"),
	         FVoxelSectionCodec::Decode(
	             B,
	             [](FVoxelBlockState V)
	             {
		             return V.TypeId <= 6;
	             },
	             R));
	TestEqual(TEXT("Fingerprint"), FVoxelSectionCodec::Fingerprint(S), FVoxelSectionCodec::Fingerprint(R));
	B.SetNum(B.Num() - 1);
	R.Fill({5, 0});
	TestFalse(TEXT("Reject truncated"),
	          FVoxelSectionCodec::Decode(
	              B,
	              [](auto)
	              {
		              return true;
	              },
	              R));
	TestEqual(TEXT("Failure leaves output intact"), R.Get(0).TypeId, uint16(5));
	return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelAtomicTest,
                                 "DreamWorld.Voxel.Phase1.AtomicOverlay",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVoxelAtomicTest::RunTest(const FString&)
{
	FFixture F;
	FVoxelWorldRuntime W(1, true, F.Registry, F.Generator.ToSharedRef());
	if (!TestTrue(TEXT("Load A"), F.Load(W, {0, 0, 12})) || !TestTrue(TEXT("Load B"), F.Load(W, {1, 0, 12})))
		return false;
	FVoxelCellEdit A;
	A.Position = {15, 1, 193};
	W.TryGetBlock(A.Position, A.Expected);
	A.Value = {1, 0};
	FVoxelCellEdit B;
	B.Position = {16, 1, 193};
	W.TryGetBlock(B.Position, B.Expected);
	B.Value = {2, 0};
	FVoxelPreparedEdit Both;
	FString E;
	TestTrue(TEXT("Prepare two sections"), W.PrepareEdit({A, B}, {}, Both, E));
	FVoxelPreparedEdit First;
	W.PrepareEdit({A}, {}, First, E);
	FVoxelEditBatch Batch;
	TestTrue(TEXT("Intervening edit"), W.CommitEdit(MoveTemp(First), Batch));
	TestFalse(TEXT("Reject stale atomic batch"), W.CommitEdit(MoveTemp(Both), Batch));
	FVoxelBlockState Actual;
	W.TryGetBlock(B.Position, Actual);
	TestEqual(TEXT("B was not partially changed"), Actual.Pack(), B.Expected.Pack());
	uint64 Saved = W.Find({0, 0, 12})->Stamp.Revision;
	FVoxelCellEdit Undo;
	Undo.Position = A.Position;
	Undo.Expected = A.Value;
	Undo.Value = A.Expected;
	FVoxelPreparedEdit P;
	W.PrepareEdit({Undo}, {}, P, E);
	W.CommitEdit(MoveTemp(P), Batch);
	W.MarkCommitted({0, 0, 12}, Saved);
	TestTrue(TEXT("New edit remains dirty"), W.Find({0, 0, 12})->IsSaveDirty());
	TestTrue(TEXT("Restore base removes overlay"), W.Find({0, 0, 12})->Overlay.Blocks.IsEmpty());
	auto Old = W.Find({1, 0, 12})->Stamp;
	TestTrue(TEXT("Unload clean section"), W.Remove({1, 0, 12}));
	F.Load(W, {1, 0, 12});
	TestFalse(TEXT("Token blocks ABA"), W.IsCurrent(Old, false));
	FVoxelWorldRuntime Client(2, false, F.Registry, F.Generator.ToSharedRef());
	Client.Allocate({0, 0, 12}, 0);
	FVoxelSectionOverlay O;
	O.Key = {0, 0, 12};
	O.Revision = 4;
	O.Blocks.Add(0, FVoxelBlockState{1, 0});
	TArray<FVoxelSectionStorage> Bases;
	Bases.SetNum(1);
	F.Generator->GenerateSection(O.Key, Bases[0]);
	TestTrue(TEXT("Initial snapshot"), Client.ApplyRemoteSnapshots({O}, MoveTemp(Bases)));
	O.Revision = 5;
	O.Blocks.Reset();
	Bases.SetNum(1);
	F.Generator->GenerateSection(O.Key, Bases[0]);
	TestTrue(TEXT("Empty snapshot replaces overlay"), Client.ApplyRemoteSnapshots({O}, MoveTemp(Bases)));
	TestTrue(TEXT("Empty state cleared"), Client.Find(O.Key)->Overlay.Blocks.IsEmpty());
	return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelShapeTest, "DreamWorld.Voxel.Phase1.Shapes", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVoxelShapeTest::RunTest(const FString&)
{
	FVoxelShapeRegistry S;
	S.BuildDefaults();
	TestEqual(TEXT("FullCube faces"), S.Get(EVoxelShapeKind::FullCube, 0).Quads.Num(), 6);
	TestEqual(TEXT("FullCube coverage"), S.Get(EVoxelShapeKind::FullCube, 0).OcclusionMask, uint8(63));
	TestFalse(TEXT("Invalid facing"), FVoxelShapeRegistry::IsValidState(EVoxelShapeKind::Door, 7));
	TestFalse(TEXT("Half slab is not a full cube"), S.Get(EVoxelShapeKind::Slab, 0).OcclusionMask == 63);
	TestTrue(TEXT("Cross plant selectable"), !S.Get(EVoxelShapeKind::CrossPlant, 0).SelectionQuads.IsEmpty());
	TestTrue(TEXT("Cross plant has no collision"), S.Get(EVoxelShapeKind::CrossPlant, 0).CollisionBoxes.IsEmpty());
	for (uint8 F = 0; F < 6; ++F)
		TestEqual(TEXT("Four rotations preserve face"), FVoxelShapeRegistry::RotateFace(F, 4), F);
	return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelTransferTest,
                                 "DreamWorld.Voxel.Phase1.FragmentReassembly",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVoxelTransferTest::RunTest(const FString&)
{
	FVoxelNetworkTransfer Sender, Receiver;
	TArray<uint8> Original;
	Original.SetNum(10000);
	for (int32 I = 0; I < Original.Num(); ++I)
		Original[I] = uint8(I);
	TArray<uint8> Copy = Original;
	TestTrue(TEXT("Queue"), Sender.Enqueue(MoveTemp(Copy)));
	TArray<TArray<uint8>> Packets;
	Sender.Tick(1,
	            [&](const auto& P)
	            {
		            Packets.Add(P);
	            });
	TestEqual(TEXT("Three fragments"), Packets.Num(), 3);
	TArray<uint8> Done;
	for (int32 I = Packets.Num() - 1; I >= 0; --I)
		TestTrue(TEXT("Out-of-order fragment"), Receiver.Receive(Packets[I], 1, Done));
	TestTrue(TEXT("Reassembled bytes"), Done == Original);
	TestEqual(TEXT("Send bytes released"), Sender.QueuedBytes(), uint64(0));
	TArray<uint8> Bad = {0, 1, 2};
	TestFalse(TEXT("Malformed packet"), Receiver.Receive(Bad, 1, Done));
	return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelShutdownTest,
                                 "DreamWorld.Voxel.Phase1.TaskShutdown",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVoxelShutdownTest::RunTest(const FString&)
{
	FVoxelTaskScheduler Scheduler;
	FVoxelTaskRequest Q;
	Q.Stamp.WorldEpoch = 1;
	Q.Stamp.GenerationToken = 1;
	Q.Stamp.Key = {0, 0, 0};
	Q.ReservedBytes = 4096;
	Q.Execute = [](const std::atomic_bool& C)
	{
		FVoxelTaskResult R;
		R.bCanceled = C.load();
		R.bSuccess = !R.bCanceled;
		return R;
	};
	TestTrue(TEXT("Queue"), Scheduler.Enqueue(MoveTemp(Q)));
	Scheduler.Tick([](FVoxelTaskResult&&) {});
	Scheduler.StopAndJoin();
	TestEqual(TEXT("All workers joined"), Scheduler.ActiveCount(), 0);
	Scheduler.StopAndJoin();
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelPrefabContractTest,
                                 "DreamWorld.Voxel.Phase1.PrefabContract",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVoxelPrefabContractTest::RunTest(const FString&)
{
	FVoxelRegistrySnapshot Registry;
	Registry.Definitions.Add(FVoxelRuntimeDefinition());
	FVoxelRuntimeDefinition Stone;
	Stone.TypeId = 1;
	Stone.Shape = EVoxelShapeKind::FullCube;
	Stone.AssetID = FPrimaryAssetId(TEXT("Voxel"), TEXT("TestStone"));
	Registry.Definitions.Add(Stone);
	Registry.Assets.Add(Stone.AssetID, Stone.TypeId);
	FVoxelRuntimeDefinition Door;
	Door.TypeId = 2;
	Door.Shape = EVoxelShapeKind::Door;
	Door.AssetID = FPrimaryAssetId(TEXT("Voxel"), TEXT("TestDoor"));
	Registry.Definitions.Add(Door);
	Registry.Assets.Add(Door.AssetID, Door.TypeId);
	FString Error;
	FVoxelPrefabSaveData Data;
	TestTrue(TEXT("Empty preview is valid"), UVoxelPrefabData::ValidateCells(Data, Registry, Error));
	FVoxelPrefabSaveData Oversized;
	Oversized.Cells.SetNum(VoxelPrefab::MaxCellCount + 1);
	TestFalse(TEXT("Prefab cell limit is enforced"), UVoxelPrefabData::ValidateCells(Oversized, Registry, Error));
	FVoxelPrefabCell Lower;
	Lower.Offset = FIntVector(-1, 0, 15);
	Lower.Item = FVoxelItem(Door.AssetID, 0, 1);
	Data.Cells.Add(Lower);
	TestFalse(TEXT("Door requires both halves"), UVoxelPrefabData::ValidateCells(Data, Registry, Error));
	FVoxelPrefabCell Upper = Lower;
	Upper.Offset.Z += 1;
	Upper.Item.State = VoxelState::HalfMask;
	Data.Cells.Add(Upper);
	TestTrue(TEXT("Door can cross a section boundary"), UVoxelPrefabData::ValidateCells(Data, Registry, Error));
	Data.Cells[1].Item.State |= VoxelState::OpenMask;
	TestFalse(TEXT("Door flags must agree"), UVoxelPrefabData::ValidateCells(Data, Registry, Error));
	Data.Cells[1].Item.State = VoxelState::HalfMask;
	Data.Cells[1].Item.Count = 2;
	TestFalse(TEXT("No stack count in a prefab cell"), UVoxelPrefabData::ValidateCells(Data, Registry, Error));
	Data.Cells[1].Item.Count = 1;
	Data.Cells.Add(Lower);
	TestFalse(TEXT("Duplicate positions are rejected"), UVoxelPrefabData::ValidateCells(Data, Registry, Error));
	Data.Cells.RemoveAt(2);
	UVoxelPrefabData* Asset = NewObject<UVoxelPrefabData>();
	Asset->Data = Data;
	const FBox Bounds = Asset->GetVoxelBounds();
	TestEqual(TEXT("Negative minimum X"), Bounds.Min.X, -1.0);
	TestEqual(TEXT("Exclusive maximum Z"), Bounds.Max.Z, 17.0);
	return !HasAnyErrors();
}

#endif
