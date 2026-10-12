#include "Step/Base/StepAsset.h"

#include "Step/Base/StepBase.h"
#include "Misc/SecureHash.h"
#include "UObject/ObjectSaveContext.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

UStepAsset::UStepAsset(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NativeClass = GetClass();
	StartGUID = FGuid::NewGuid().ToString();
}

void UStepAsset::Initialize()
{
	Super::Initialize();
}

UStepBase* UStepAsset::FindDefinition(const FString& InGUID) const
{
	for(UStepBase* Step : Steps)
	{
		if(Step && Step->StepGUID == InGUID)
		{
			return Step;
		}
	}
	return nullptr;
}

bool UStepAsset::CanAddStep(TSubclassOf<UStepBase> InClass) const
{
	return InClass && !InClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists);
}

bool UStepAsset::BuildCompiledGraph(FStepCompiledGraph& OutGraph, TArray<FText>& OutErrors) const
{
	OutGraph = FStepCompiledGraph();
	OutErrors.Reset();
	const auto Error = [&OutErrors](const FString& Message)
	{
		OutErrors.Add(FText::FromString(Message));
	};
	FGuid ParsedGUID;
	if(!FGuid::Parse(StartGUID, ParsedGUID) || !ParsedGUID.IsValid())
	{
		Error(TEXT("Start 的稳定 GUID 无效。"));
	}
	if(!FMath::IsFinite(StartPosition.X) || !FMath::IsFinite(StartPosition.Y))
	{
		Error(TEXT("Start 节点坐标不是有限数。"));
	}
	if(Steps.Num() > 4096)
	{
		Error(TEXT("一个步骤资产最多允许 4096 个作者节点。"));
	}
	TSet<FGuid> NormalizedGUIDs;
	if(ParsedGUID.IsValid())
	{
		NormalizedGUIDs.Add(ParsedGUID);
	}
	TMap<FString, UStepBase*> Definitions;
	for(UStepBase* Step : Steps)
	{
		if(!Step || Step->GetOuter() != this || !CanAddStep(Step->GetClass()))
		{
			Error(TEXT("作者节点为空、所属资产不正确或类型无效。"));
			continue;
		}
		if(!FGuid::Parse(Step->StepGUID, ParsedGUID) || !ParsedGUID.IsValid() || NormalizedGUIDs.Contains(ParsedGUID) || Definitions.Contains(Step->StepGUID))
		{
			Error(FString::Printf(TEXT("节点 GUID 无效或重复：%s"), *Step->StepGUID));
			continue;
		}
		if(!FMath::IsFinite(Step->GraphPosition.X) || !FMath::IsFinite(Step->GraphPosition.Y))
		{
			Error(FString::Printf(TEXT("节点坐标不是有限数：%s"), *Step->StepGUID));
		}
		NormalizedGUIDs.Add(ParsedGUID);
		Definitions.Add(Step->StepGUID, Step);
	}
	TMap<FString, TArray<FString>> Children;
	TMap<FString, FString> Parents;
	for(const FStepGraphLink& Link : Links)
	{
		if((Link.ParentGUID != StartGUID && !Definitions.Contains(Link.ParentGUID)) || !Definitions.Contains(Link.ChildGUID) || Link.ParentGUID == Link.ChildGUID)
		{
			Error(FString::Printf(TEXT("无效连线：%s -> %s"), *Link.ParentGUID, *Link.ChildGUID));
			continue;
		}
		if(Parents.Contains(Link.ChildGUID))
		{
			Error(FString::Printf(TEXT("节点存在重复连线或多个父节点：%s"), *Link.ChildGUID));
			continue;
		}
		Parents.Add(Link.ChildGUID, Link.ParentGUID);
		Children.FindOrAdd(Link.ParentGUID).Add(Link.ChildGUID);
	}
	if(!OutErrors.IsEmpty())
	{
		return false;
	}
	for(auto& Pair : Children)
	{
		Pair.Value.Sort([&Definitions](const FString& LeftGUID, const FString& RightGUID)
		{
			const FVector2D Left = Definitions.FindChecked(LeftGUID)->GraphPosition;
			const FVector2D Right = Definitions.FindChecked(RightGUID)->GraphPosition;
			if(Left.Y != Right.Y)
			{
				return Left.Y < Right.Y;
			}
			if(Left.X != Right.X)
			{
				return Left.X < Right.X;
			}
			return LeftGUID < RightGUID;
		});
	}
	TMap<FString, uint8> Colors;
	TFunction<bool(const FString&, int32)> Validate = [&](const FString& GUID, int32 Depth)
	{
		if(Depth > 256)
		{
			Error(TEXT("步骤层级超过 256 层。"));
			return false;
		}
		const uint8 Color = Colors.FindRef(GUID);
		if(Color == 1)
		{
			Error(FString::Printf(TEXT("步骤图包含循环：%s"), *GUID));
			return false;
		}
		if(Color == 2)
		{
			return true;
		}
		Colors.Add(GUID, 1);
		if(const TArray<FString>* SubGUIDs = Children.Find(GUID))
		{
			for(const FString& ChildGUID : *SubGUIDs)
			{
				if(!Validate(ChildGUID, Depth + 1))
				{
					return false;
				}
			}
		}
		Colors.Add(GUID, 2);
		return true;
	};
	if(!Validate(StartGUID, 0))
	{
		return false;
	}
	TArray<FString> AllGUIDs;
	Definitions.GetKeys(AllGUIDs);
	AllGUIDs.Sort();
	for(const FString& GUID : AllGUIDs)
	{
		if(!Validate(GUID, 0))
		{
			return false;
		}
	}
	FStepCompiledGraph Candidate;
	if(const TArray<FString>* Roots = Children.Find(StartGUID))
	{
		Candidate.RootGUIDs = *Roots;
	}
	TFunction<void(const FString&, const FString&, int32, int32, const FString&)> Emit;
	Emit = [&](const FString& GUID, const FString& ParentGUID, int32 Index, int32 Depth, const FString& Label)
	{
		FStepCompiledNode Node;
		Node.StepGUID = GUID;
		Node.ParentGUID = ParentGUID;
		Node.Index = Index;
		Node.Hierarchy = Depth;
		Node.OrderLabel = Label;
		if(const TArray<FString>* SubGUIDs = Children.Find(GUID))
		{
			Node.ChildrenGUIDs = *SubGUIDs;
		}
		Candidate.Nodes.Add(Node);
		for(int32 ChildIndex = 0; ChildIndex < Node.ChildrenGUIDs.Num(); ++ChildIndex)
		{
			Emit(Node.ChildrenGUIDs[ChildIndex], GUID, ChildIndex, Depth + 1, Label + TEXT(".") + FString::FromInt(ChildIndex + 1));
		}
	};
	for(int32 RootIndex = 0; RootIndex < Candidate.RootGUIDs.Num(); ++RootIndex)
	{
		Emit(Candidate.RootGUIDs[RootIndex], FString(), RootIndex, 0, FString::FromInt(RootIndex + 1));
	}
	FString SignatureText = TEXT("StepGraph:1|") + StartGUID;
	for(const FStepCompiledNode& Node : Candidate.Nodes)
	{
		SignatureText += TEXT("|") + Node.StepGUID + TEXT(":") + Node.ParentGUID + TEXT(":") + Node.OrderLabel;
		SignatureText += TEXT(":") + Definitions.FindChecked(Node.StepGUID)->GetClass()->GetPathName();
	}
	Candidate.Signature = FMD5::HashAnsiString(*SignatureText);
	OutGraph = MoveTemp(Candidate);
	return true;
}

bool UStepAsset::Compile(TArray<FText>& OutErrors)
{
	FStepCompiledGraph Candidate;
	bCompiled = BuildCompiledGraph(Candidate, OutErrors);
	CompiledGraph = bCompiled ? MoveTemp(Candidate) : FStepCompiledGraph();
	return bCompiled;
}

#if WITH_EDITOR
void UStepAsset::PreSave(FObjectPreSaveContext SaveContext)
{
	TArray<FText> Errors;
	Compile(Errors);
	for(const FText& Error : Errors)
	{
		UE_LOG(LogTemp, Error, TEXT("%s: %s"), *GetPathName(), *Error.ToString());
	}
	Super::PreSave(SaveContext);
}

EDataValidationResult UStepAsset::IsDataValid(FDataValidationContext& Context) const
{
	const EDataValidationResult BaseResult = Super::IsDataValid(Context);
	TArray<FText> Errors;
	FStepCompiledGraph Graph;
	if(!BuildCompiledGraph(Graph, Errors))
	{
		for(const FText& Error : Errors)
		{
			Context.AddError(Error);
		}
		return EDataValidationResult::Invalid;
	}
	return BaseResult == EDataValidationResult::Invalid ? BaseResult : EDataValidationResult::Valid;
}
#endif
