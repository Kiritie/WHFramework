#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "PackageTools.h"
#include "Task/Base/TaskAsset.h"
#include "Task/Base/TaskBase.h"
#include "Step/Base/StepAsset.h"
#include "Step/Base/StepBase.h"
#include "Procedure/Base/ProcedureAsset.h"
#include "Procedure/Base/ProcedureBase.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/StrongObjectPtr.h"

namespace GraphPersistenceTests
{
	struct FMount
	{
		FString Root = TEXT("/GraphPersistence_") + FGuid::NewGuid().ToString(EGuidFormats::Digits) + TEXT("/");
		FString Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Automation/GraphPersistence") / FGuid::NewGuid().ToString(EGuidFormats::Digits)) + TEXT("/");

		FMount()
		{
			IFileManager::Get().MakeDirectory(*Directory, true);
			FPackageName::RegisterMountPoint(Root, Directory);
		}

		~FMount()
		{
			FPackageName::UnRegisterMountPoint(Root, Directory);
		}
	};

	UObject* SaveAndReload(UObject* Asset, FAutomationTestBase& Test)
	{
		UPackage* Package = Asset->GetOutermost();
		const FString PackageName = Package->GetName();
		const FString AssetName = Asset->GetName();
		const FString Filename = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		if(!Test.TestTrue(TEXT("图资产写入磁盘"), UPackage::SavePackage(Package, Asset, *Filename, Args)))
		{
			return nullptr;
		}
		FText Error;
		if(!Test.TestTrue(TEXT("原包完全卸载"), UPackageTools::UnloadPackages({Package}, Error)))
		{
			Test.AddError(Error.ToString());
			return nullptr;
		}
		if(!Test.TestNull(TEXT("重载前没有旧包可供复用"), FindPackage(nullptr, *PackageName)))
		{
			return nullptr;
		}
		UPackage* Reloaded = LoadPackage(nullptr, *PackageName, LOAD_None);
		return Reloaded ? FindObject<UObject>(Reloaded, *AssetName) : nullptr;
	}
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FGraphPersistenceTest, "WHFramework.Graph.Persistence.SaveCloseReopen",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FGraphPersistenceTest::GetTests(TArray<FString>& Names, TArray<FString>& Commands) const
{
	for(const FString& Name : {FString(TEXT("Task")), FString(TEXT("Step")), FString(TEXT("Procedure"))})
	{
		Names.Add(Name);
		Commands.Add(Name);
	}
}

bool FGraphPersistenceTest::RunTest(const FString& Parameters)
{
	GraphPersistenceTests::FMount Mount;
	UPackage* Package = CreatePackage(*(Mount.Root + Parameters));
	TArray<FText> Errors;
	if(Parameters == TEXT("Task"))
	{
		UTaskAsset* Asset = NewObject<UTaskAsset>(Package, TEXT("Task"), RF_Public | RF_Standalone);
		Asset->InitializeGraph();
		const FGuid StartId = Asset->StartGUID;
		Asset->StartPosition = FVector2D(-317.5, 42.25);
		UTaskBase* Root = NewObject<UTaskBase>(Asset);
		UTaskBase* Child = NewObject<UTaskBase>(Asset);
		UTaskBase* Draft = NewObject<UTaskBase>(Asset);
		Root->TaskGUID = TEXT("Persist.Root");
		Child->TaskGUID = TEXT("Persist.Child");
		Draft->TaskGUID = TEXT("Persist.Draft");
		Root->GraphChildren = {Child};
		Asset->AuthoredTasks = {Root, Child, Draft};
		Asset->StartTasks = {Root};
		if(!TestTrue(TEXT("保存前编译任务图"), Asset->CompileTasks(Errors))) return false;
		TStrongObjectPtr<UTaskAsset> Reloaded(Cast<UTaskAsset>(GraphPersistenceTests::SaveAndReload(Asset, *this)));
		if(!TestNotNull(TEXT("重新加载真实任务包"), Reloaded.Get())) return false;
		TestEqual(TEXT("Start 身份保留"), Reloaded->StartGUID, StartId);
		TestEqual(TEXT("Start 位置保留"), Reloaded->StartPosition, FVector2D(-317.5, 42.25));
		TestEqual(TEXT("作者草稿保留"), Reloaded->AuthoredTasks.Num(), 3);
		TestEqual(TEXT("PostLoad 建立可达运行索引"), Reloaded->TaskMap.Num(), 2);
		TestFalse(TEXT("断开草稿仍不参与运行"), Reloaded->TaskMap.Contains(TEXT("Persist.Draft")));
		UTaskBase* ReloadedRoot = Reloaded->TaskMap.FindRef(TEXT("Persist.Root"));
		UTaskBase* ReloadedChild = Reloaded->TaskMap.FindRef(TEXT("Persist.Child"));
		if(!TestNotNull(TEXT("根任务存在"), ReloadedRoot) || !TestNotNull(TEXT("子任务存在"), ReloadedChild)) return false;
		TestEqual(TEXT("子树关系完整保留"), ReloadedChild->ParentTask, ReloadedRoot);
		return true;
	}
	if(Parameters == TEXT("Step"))
	{
		UStepAsset* Asset = NewObject<UStepAsset>(Package, TEXT("Step"), RF_Public | RF_Standalone);
		Asset->StartGUID = FGuid::NewGuid().ToString();
		const FString StartId = Asset->StartGUID;
		UStepBase* Root = NewObject<UStepBase>(Asset);
		UStepBase* Child = NewObject<UStepBase>(Asset);
		UStepBase* Draft = NewObject<UStepBase>(Asset);
		Root->StepGUID = FGuid::NewGuid().ToString();
		Child->StepGUID = FGuid::NewGuid().ToString();
		Draft->StepGUID = FGuid::NewGuid().ToString();
		const FString ChildId = Child->StepGUID;
		const FString DraftId = Draft->StepGUID;
		Asset->Steps = {Root, Child, Draft};
		FStepGraphLink StartLink;
		StartLink.ParentGUID = StartId;
		StartLink.ChildGUID = Root->StepGUID;
		FStepGraphLink ChildLink;
		ChildLink.ParentGUID = Root->StepGUID;
		ChildLink.ChildGUID = ChildId;
		Asset->Links = {StartLink, ChildLink};
		if(!TestTrue(TEXT("保存前编译步骤图"), Asset->Compile(Errors))) return false;
		TStrongObjectPtr<UStepAsset> Reloaded(Cast<UStepAsset>(GraphPersistenceTests::SaveAndReload(Asset, *this)));
		if(!TestNotNull(TEXT("重新加载真实步骤包"), Reloaded.Get())) return false;
		TestEqual(TEXT("Start 身份保留"), Reloaded->StartGUID, StartId);
		TestEqual(TEXT("作者草稿保留"), Reloaded->Steps.Num(), 3);
		if(!TestEqual(TEXT("仅可达步骤进入编译结果"), Reloaded->CompiledGraph.Nodes.Num(), 2)) return false;
		TestNotNull(TEXT("断开草稿可以继续编辑"), Reloaded->FindDefinition(DraftId));
		TestEqual(TEXT("编译子树顺序保留"), Reloaded->CompiledGraph.Nodes[1].StepGUID, ChildId);
		return true;
	}
	if(Parameters == TEXT("Procedure"))
	{
		UProcedureAsset* Asset = NewObject<UProcedureAsset>(Package, TEXT("Procedure"), RF_Public | RF_Standalone);
		Asset->StartId = FGuid::NewGuid();
		const FGuid StartId = Asset->StartId;
		UProcedureBase* First = NewObject<UProcedureBase>(Asset);
		UProcedureBase* Second = NewObject<UProcedureBase>(Asset);
		UProcedureBase* Draft = NewObject<UProcedureBase>(Asset);
		First->NodeId = FGuid::NewGuid();
		Second->NodeId = FGuid::NewGuid();
		Draft->NodeId = FGuid::NewGuid();
		const FGuid SecondId = Second->NodeId;
		const FGuid DraftId = Draft->NodeId;
		Asset->Nodes = {First, Second, Draft};
		FProcedureLink Entry;
		Entry.From = StartId;
		Entry.To = First->NodeId;
		FProcedureLink Next;
		Next.From = First->NodeId;
		Next.To = SecondId;
		Asset->Links = {Entry, Next};
		if(!TestTrue(TEXT("保存前编译流程图"), Asset->Recompile())) return false;
		TStrongObjectPtr<UProcedureAsset> Reloaded(Cast<UProcedureAsset>(GraphPersistenceTests::SaveAndReload(Asset, *this)));
		if(!TestNotNull(TEXT("重新加载真实流程包"), Reloaded.Get())) return false;
		TestEqual(TEXT("Start 身份保留"), Reloaded->StartId, StartId);
		TestEqual(TEXT("作者草稿保留"), Reloaded->Nodes.Num(), 3);
		if(!TestEqual(TEXT("仅显式链进入编译结果"), Reloaded->CompiledNodes.Num(), 2)) return false;
		TestNotNull(TEXT("断开流程草稿保留"), Reloaded->FindNode(DraftId));
		TestEqual(TEXT("默认后继身份保留"), Reloaded->CompiledNodes[1]->NodeId, SecondId);
		return true;
	}
	return false;
}

#endif
