#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "HAL/PlatformProcess.h"
#include "Voxel/Generation/VoxelGenerationPipeline.h"
#include "Voxel/Generation/VoxelManifestCodec.h"
#include "Voxel/Runtime/VoxelRegistry.h"
#include "Voxel/Runtime/VoxelWorldRuntime.h"
#include "Voxel/Rendering/VoxelProxyData.h"
#include "Voxel/Rendering/VoxelLodMesher.h"
#include "Voxel/Tasks/VoxelTaskScheduler.h"
namespace
{
FVoxelGenerationRuntimeConfig Config()
{
    FVoxelGenerationRuntimeConfig C;C.BlockSizeCentimeters=25;
    C.Settings.MinZ=-64;C.Settings.MaxZ=192;C.Settings.BaseHeight=32;C.Settings.SeaLevel=0;
    C.Settings.LavaCeiling=-48;C.Settings.ContinentalAmplitude=0;C.Settings.MountainAmplitude=0;
    C.Settings.DetailAmplitude=0;C.Settings.RiverWidthQ15=0;
    auto P=std::make_shared<VoxelGen::Catalog>();
    P->blocks={"core:air","test:stone","test:dirt","test:grass","test:sand","test:snow","test:water","test:lava","test:bedrock","test:road"};
    C.Catalog=P;for(uint16 I=0;I<10;++I){C.SymbolToRuntime.push_back(I);C.RuntimeToSymbol.push_back(I);}
    C.Stone=1;C.Dirt=2;C.Grass=3;C.Sand=4;C.Snow=5;C.Water=6;C.Lava=7;
    return C;
}
TSharedRef<FVoxelRegistrySnapshot,ESPMode::ThreadSafe> Registry()
{
    auto R=MakeShared<FVoxelRegistrySnapshot,ESPMode::ThreadSafe>();
    auto C=Config();
    for(uint16 I=0;I<10;++I)
    {
        FVoxelRuntimeDefinition D;D.TypeId=I;D.BlockName=FName(UTF8_TO_TCHAR(C.Catalog->blocks[I].c_str()));
        D.Shape=(I==6||I==7)?EVoxelShapeKind::Fluid:EVoxelShapeKind::FullCube;
        D.RenderGroup=I==6?EVoxelRenderGroup::Water:I==7?EVoxelRenderGroup::Emissive:EVoxelRenderGroup::Opaque;
        D.bSolid=I!=0&&I!=6&&I!=7;D.bOccludes=D.bSolid;D.DropCount=0;
        R->Definitions.Add(D);R->Names.Add(D.BlockName,I);
    }
    return R;
}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelPhase2ManifestTest,"WHFramework.Voxel.Phase2.Manifest",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVoxelPhase2ManifestTest::RunTest(const FString&)
{
    FVoxelWorldManifest M;M.WorldId=FGuid::NewGuid();M.Settings=Config().Settings;M.BlockSizeCentimeters=25;
    M.RegistryHash=11;M.CatalogHash=22;M.BaseSampleHash=33;M.RecipeHash=FVoxelManifestCodec::RecipeFingerprint(M);
    TArray<uint8> B;TestTrue(TEXT("encode"),FVoxelManifestCodec::Encode(M,B));FVoxelWorldManifest D;
    TestTrue(TEXT("decode"),FVoxelManifestCodec::Decode(B,D));TestEqual(TEXT("catalog"),D.CatalogHash,uint64(22));
    auto Truncated=B;Truncated.Pop();TestFalse(TEXT("truncated rejected"),FVoxelManifestCodec::Decode(Truncated,D));
    auto Trailing=B;Trailing.Add(0);TestFalse(TEXT("trailing rejected"),FVoxelManifestCodec::Decode(Trailing,D));
    M.CatalogHash++;TestNotEqual(TEXT("catalog changes recipe"),M.RecipeHash,FVoxelManifestCodec::RecipeFingerprint(M));return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelPhase2BaseTest,"WHFramework.Voxel.Phase2.BaseAndOverlay",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVoxelPhase2BaseTest::RunTest(const FString&)
{
    auto C=Config();auto G=MakeShared<FVoxelGenerationPipeline,ESPMode::ThreadSafe>(C);auto R=Registry();
    FVoxelWorldRuntime Runtime(1,true,R,G);const FVoxelSectionKey K{0,0,-4};
    auto* S=Runtime.Allocate(K,0);if(!TestNotNull(TEXT("allocate"),S))return false;
    FVoxelSectionStorage Base;TestTrue(TEXT("generate"),G->GenerateSection(K,Base));
    FVoxelSectionOverlay Empty;Empty.Key=K;
    TestTrue(TEXT("publish"),Runtime.PublishLoaded(S->Stamp,MoveTemp(Base),Empty,true));
    TestTrue(TEXT("compressed base retained"),S->BaseBlocks.IsValid());
    const auto Natural=S->Blocks.Get(0);FVoxelCellEdit Edit;Edit.Position=FIntVector(0,0,-64);Edit.Expected=Natural;Edit.Value={};
    FVoxelPreparedEdit Prepared;FString Error;
    TestTrue(TEXT("prepare remove"),Runtime.PrepareEdit({Edit},{},Prepared,Error));FVoxelEditBatch Batch;
    TestTrue(TEXT("commit remove"),Runtime.CommitEdit(MoveTemp(Prepared),Batch));
    TestTrue(TEXT("air overlay retained"),S->Overlay.Blocks.Contains(0));
    Edit.Expected={};Edit.Value=Natural;
    TestTrue(TEXT("prepare restore"),Runtime.PrepareEdit({Edit},{},Prepared,Error));
    TestTrue(TEXT("commit restore"),Runtime.CommitEdit(MoveTemp(Prepared),Batch));
    TestFalse(TEXT("restoring natural base removes delta"),S->Overlay.Blocks.Contains(0));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelPhase2SeamTest,"WHFramework.Voxel.Phase2.LodBoundary",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVoxelPhase2SeamTest::RunTest(const FString&)
{
    auto Fine=std::make_shared<VoxelView::Grid>();Fine->key={{1,0,10},0};Fine->cells.fill(VoxelGen::Pack(1));
    auto Coarse=std::make_shared<VoxelView::Grid>();Coarse->key={{1,0,5},1};Coarse->cells.fill(VoxelGen::Pack(1));
    FVoxelLodMeshInput I;I.Current=Fine;I.Neighbors={Coarse};I.CoverageSerial=1;
    FVoxelShapeRegistry H;H.BuildDefaults();auto R=Registry();FVoxelGenerationPipeline G(Config());
    FVoxelLodBuiltMesh M;FString Error;
    if(!TestTrue(TEXT("LOD mesh"),FVoxelLodMesher::Build(I,G,*R,H,M,Error)))return false;
    int32 Triangles=0;for(const auto& B:M.Mesh.Batches)Triangles+=B.Mesh.Triangles.Num()/3;
    TestEqual(TEXT("Five external faces, shared coarse face hidden"),Triangles,10);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelPhase2ProxyWireTest,"WHFramework.Voxel.Phase2.ProxyWire",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVoxelPhase2ProxyWireTest::RunTest(const FString&)
{
    FVoxelProxyRequest Q;Q.Key={{-7,2,0},4};Q.Serial=42;Q.KnownVersion=MAX_uint64;TArray<uint8> B;
    TestTrue(TEXT("encode request"),FVoxelProxyData::EncodeRequest(Q,B));FVoxelProxyRequest D;
    TestTrue(TEXT("decode request"),FVoxelProxyData::DecodeRequest(B,D));TestEqual(TEXT("serial"),D.Serial,Q.Serial);
    B.Add(0);TestFalse(TEXT("trailing"),FVoxelProxyData::DecodeRequest(B,D));
    FVoxelProxyReply X;X.Key=Q.Key;X.Serial=42;X.Version=3;X.Mode=EVoxelProxyMode::Natural;
    TestTrue(TEXT("encode natural proof"),FVoxelProxyData::EncodeReply(X,B));FVoxelProxyReply Y;
    TestTrue(TEXT("decode natural proof"),FVoxelProxyData::DecodeReply(B,10,Y));return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelPhase2TaskTest,"WHFramework.Voxel.Phase2.DerivedTaskIdentity",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVoxelPhase2TaskTest::RunTest(const FString&)
{
    FVoxelTaskScheduler S;int32 Success=0;int32 Canceled=0;
    for(uint64 ID=1;ID<=2;++ID)
    {
        FVoxelTaskRequest Q;Q.Kind=EVoxelTaskKind::BuildProxy;Q.Stamp.WorldEpoch=1;Q.Stamp.GenerationToken=ID;
        Q.Stamp.Key={0,0,0};Q.ReservedBytes=1024;
        Q.Execute=[](const std::atomic_bool&){FVoxelTaskResult R;R.bSuccess=true;return R;};
        TestTrue(TEXT("enqueue different requests"),S.Enqueue(MoveTemp(Q)));
    }
    const double End=FPlatformTime::Seconds()+5;
    while(Success+Canceled<2&&FPlatformTime::Seconds()<End)
    {
        S.Tick([&](FVoxelTaskResult&& R){if(R.bSuccess&&!R.bCanceled)++Success;else ++Canceled;});
        FPlatformProcess::Sleep(.001f);
    }
    S.StopAndJoin();TestEqual(TEXT("both peers complete"),Success,2);TestEqual(TEXT("no alias cancellation"),Canceled,0);return true;
}
#endif
