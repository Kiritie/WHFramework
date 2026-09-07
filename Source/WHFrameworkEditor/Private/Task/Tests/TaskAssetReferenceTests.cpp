#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Task/Base/TaskAsset.h"
#include "Task/Base/TaskAssetReferenceTask.h"
#include "Task/Base/TaskBase.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTaskAssetReferenceTest, "WHFramework.Task.AssetReference", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTaskAssetReferenceTest::RunTest(const FString& Parameters)
{
	auto NewAsset = [](const TCHAR* BaseName)
	{
		return NewObject<UTaskAsset>(GetTransientPackage(), MakeUniqueObjectName(GetTransientPackage(), UTaskAsset::StaticClass(), FName(BaseName)));
	};
	auto NewTask = [](UTaskAsset* Asset, UClass* Class = UTaskBase::StaticClass())
	{
		return NewObject<UTaskBase>(Asset, Class, NAME_None);
	};

	TStrongObjectPtr<UTaskAsset> LeafDefinition(NewAsset(TEXT("TaskReferenceLeaf")));
	UTaskBase* LeafRoot = NewTask(LeafDefinition.Get());
	LeafRoot->TaskDisplayName = FText::FromString(TEXT("Return for reward"));
	LeafDefinition->RootTasks = {LeafRoot};

	TStrongObjectPtr<UTaskAsset> ReusedDefinition(NewAsset(TEXT("TaskReferenceReusable")));
	UTaskBase* ReusedRoot = NewTask(ReusedDefinition.Get());
	UTaskBase* ReusedLeaf = NewTask(ReusedDefinition.Get());
	UTaskAssetReferenceTask* NestedReference = CastChecked<UTaskAssetReferenceTask>(NewTask(ReusedDefinition.Get(), UTaskAssetReferenceTask::StaticClass()));
	NestedReference->ReferencedAsset = LeafDefinition.Get();
	ReusedRoot->SubTasks = {ReusedLeaf, NestedReference};
	FTaskReference LocalPrerequisite;
	LocalPrerequisite.Asset = ReusedDefinition.Get();
	LocalPrerequisite.TaskGUID = ReusedRoot->TaskGUID;
	ReusedLeaf->Prerequisites = {LocalPrerequisite};
	ReusedDefinition->RootTasks = {ReusedRoot};

	TStrongObjectPtr<UTaskAsset> MainDefinition(NewAsset(TEXT("TaskReferenceMain")));
	UTaskAssetReferenceTask* FirstReference = CastChecked<UTaskAssetReferenceTask>(NewTask(MainDefinition.Get(), UTaskAssetReferenceTask::StaticClass()));
	UTaskAssetReferenceTask* SecondReference = CastChecked<UTaskAssetReferenceTask>(NewTask(MainDefinition.Get(), UTaskAssetReferenceTask::StaticClass()));
	TestEqual(TEXT("引用节点默认自动离开以允许顺序组合继续"), FirstReference->TaskLeaveType, ETaskLeaveType::Automatic);
	FirstReference->ReferencedAsset = ReusedDefinition.Get();
	SecondReference->ReferencedAsset = ReusedDefinition.Get();
	MainDefinition->RootTasks = {FirstReference, SecondReference};

	TArray<FText> Errors;
	TestTrue(TEXT("嵌套与重复引用的源定义有效"), MainDefinition->ValidateTasks(Errors));
	if(!Errors.IsEmpty())
	{
		for(const FText& Error : Errors) AddError(Error.ToString());
		return false;
	}

	auto MakeRuntime = [&]() -> UTaskAsset*
	{
		UTaskAsset* Runtime = DuplicateObject<UTaskAsset>(MainDefinition.Get(), GetTransientPackage(),
			MakeUniqueObjectName(GetTransientPackage(), UTaskAsset::StaticClass(), TEXT("TaskReferenceRuntime")));
		Runtime->SourceObject = MainDefinition.Get();
		TArray<FText> ExpansionErrors;
		if(!Runtime->ExpandTaskAssetReferences(ExpansionErrors))
		{
			for(const FText& Error : ExpansionErrors) AddError(Error.ToString());
			return nullptr;
		}
		if(!Runtime->RebuildTaskMap()) return nullptr;
		return Runtime;
	};

	TStrongObjectPtr<UTaskAsset> RuntimeOne(MakeRuntime());
	if(!TestNotNull(TEXT("运行时任务资产可展开"), RuntimeOne.Get())) return false;
	TestEqual(TEXT("两个引用点各自展开完整嵌套树"), RuntimeOne->TaskMap.Num(), 10);
	UTaskAssetReferenceTask* RuntimeFirst = CastChecked<UTaskAssetReferenceTask>(RuntimeOne->RootTasks[0]);
	UTaskAssetReferenceTask* RuntimeSecond = CastChecked<UTaskAssetReferenceTask>(RuntimeOne->RootTasks[1]);
	TestTrue(TEXT("引用节点标记为运行时展开"), RuntimeFirst->IsRuntimeExpanded() && RuntimeSecond->IsRuntimeExpanded());
	TestEqual(TEXT("每个引用点各有一棵独立根树"), RuntimeFirst->SubTasks.Num(), 1);
	TestEqual(TEXT("重复引用生成不同任务身份"), RuntimeFirst->SubTasks[0]->TaskGUID == RuntimeSecond->SubTasks[0]->TaskGUID, false);
	TestEqual(TEXT("源资产不被运行时展开污染"), NestedReference->SubTasks.Num(), 0);

	UTaskBase* RuntimeReusedRoot = RuntimeFirst->SubTasks[0];
	UTaskBase* RuntimeReusedLeaf = RuntimeReusedRoot->SubTasks[0];
	UTaskAssetReferenceTask* RuntimeNestedReference = CastChecked<UTaskAssetReferenceTask>(RuntimeReusedRoot->SubTasks[1]);
	TestTrue(TEXT("二级引用继续展开"), RuntimeNestedReference->IsRuntimeExpanded() && RuntimeNestedReference->SubTasks.Num() == 1);
	TestEqual(TEXT("展开节点归属最外层运行资产"), RuntimeNestedReference->SubTasks[0]->GetTaskAsset(), RuntimeOne.Get());
	if(TestEqual(TEXT("本地前置依赖仍存在"), RuntimeReusedLeaf->Prerequisites.Num(), 1))
	{
		const FTaskReference& RuntimePrerequisite = RuntimeReusedLeaf->Prerequisites[0];
		TestEqual(TEXT("内部前置依赖重定向到最外层定义"), RuntimePrerequisite.Asset.Get(), MainDefinition.Get());
		TestEqual(TEXT("内部前置依赖使用展开后的稳定身份"), RuntimePrerequisite.TaskGUID, RuntimeReusedRoot->TaskGUID);
		TestEqual(TEXT("内部前置依赖保持上下文实例语义"), RuntimePrerequisite.InstanceID.IsValid(), false);
	}

	const FString StableFirstRootGUID = RuntimeFirst->SubTasks[0]->TaskGUID;
	const FString StableNestedLeafGUID = RuntimeNestedReference->SubTasks[0]->TaskGUID;
	TStrongObjectPtr<UTaskAsset> RuntimeTwo(MakeRuntime());
	if(TestNotNull(TEXT("第二个运行时副本可展开"), RuntimeTwo.Get()))
	{
		UTaskAssetReferenceTask* SecondRuntimeFirst = CastChecked<UTaskAssetReferenceTask>(RuntimeTwo->RootTasks[0]);
		UTaskAssetReferenceTask* SecondRuntimeNested = CastChecked<UTaskAssetReferenceTask>(SecondRuntimeFirst->SubTasks[0]->SubTasks[1]);
		TestEqual(TEXT("相同引用路径跨运行副本保持身份"), SecondRuntimeFirst->SubTasks[0]->TaskGUID, StableFirstRootGUID);
		TestEqual(TEXT("嵌套叶节点跨运行副本保持身份"), SecondRuntimeNested->SubTasks[0]->TaskGUID, StableNestedLeafGUID);
	}

	TStrongObjectPtr<UTaskAsset> CycleA(NewAsset(TEXT("TaskReferenceCycleA")));
	TStrongObjectPtr<UTaskAsset> CycleB(NewAsset(TEXT("TaskReferenceCycleB")));
	UTaskAssetReferenceTask* CycleATask = CastChecked<UTaskAssetReferenceTask>(NewTask(CycleA.Get(), UTaskAssetReferenceTask::StaticClass()));
	UTaskAssetReferenceTask* CycleBTask = CastChecked<UTaskAssetReferenceTask>(NewTask(CycleB.Get(), UTaskAssetReferenceTask::StaticClass()));
	CycleATask->ReferencedAsset = CycleB.Get();
	CycleBTask->ReferencedAsset = CycleA.Get();
	CycleA->RootTasks = {CycleATask};
	CycleB->RootTasks = {CycleBTask};
	Errors.Reset();
	TestFalse(TEXT("循环任务资产引用被拒绝"), CycleA->ValidateTasks(Errors));
	TestTrue(TEXT("循环引用提供明确错误"), Errors.ContainsByPredicate([](const FText& Error)
	{
		return Error.ToString().Contains(TEXT("cycle"), ESearchCase::IgnoreCase);
	}));

	TStrongObjectPtr<UTaskAsset> AuthoredChildren(NewAsset(TEXT("TaskReferenceAuthoredChildren")));
	UTaskAssetReferenceTask* InvalidReference = CastChecked<UTaskAssetReferenceTask>(NewTask(AuthoredChildren.Get(), UTaskAssetReferenceTask::StaticClass()));
	InvalidReference->ReferencedAsset = LeafDefinition.Get();
	InvalidReference->SubTasks = {NewTask(AuthoredChildren.Get())};
	AuthoredChildren->RootTasks = {InvalidReference};
	Errors.Reset();
	TestFalse(TEXT("引用节点不允许混入手工子节点"), AuthoredChildren->ValidateTasks(Errors));

	return true;
}

#endif
