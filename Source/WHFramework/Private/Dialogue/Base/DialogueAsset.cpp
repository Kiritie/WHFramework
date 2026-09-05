// Copyright Underflow Studios 2017

#include "Dialogue/Base/DialogueAsset.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif
#include "Runtime/Core/Public/Misc/OutputDeviceNull.h"
#include "Engine/GameInstance.h"




UDialogueAsset::UDialogueAsset(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{

}

void UDialogueAsset::AssignPersistentOuter(UGameInstance * inGameInstance)
{
	PersistentGameInstance = inGameInstance;
}

void UDialogueAsset::CleanOuter()
{
	PersistentGameInstance = nullptr;
}

UWorld* UDialogueAsset::GetWorld() const
{
	if (PersistentGameInstance)
	{
		return PersistentGameInstance->GetWorld();
	}
	else
	{
		return nullptr;
	}
}

FDialogueNode UDialogueAsset::GetNodeById(int32 id, int32 & index)
{
	index = -1;

	int i = 0;
	for (const FDialogueNode& FoundNode : Data)
	{
		if (FoundNode.id == id)
		{
			index = i;
			return FoundNode;
		}
		i++;
	}

	FDialogueNode Empty;
	return Empty;
}

FDialogueNode UDialogueAsset::GetNodeById(int32 id)
{
	int32 index;
	return GetNodeById(id, index);
}

FDialogueNode UDialogueAsset::GetFirstNode()
{
	FDialogueNode StartNode = GetNodeById(0);

	if (StartNode.Links.Num() > 0)
	{
		return GetNodeById(StartNode.Links[0]);
	}

	FDialogueNode Empty;
	return Empty;
}

TArray<FDialogueNode> UDialogueAsset::GetNextNodes(FDialogueNode Node)
{
	TArray<FDialogueNode> Output;

	for (int32 foundindex : Node.Links)
	{
		FDialogueNode Next = GetNodeById(foundindex);
		if (Next.id >= 0) Output.Add(Next);
	}

	return Output;
}

void UDialogueAsset::CallFunctionByName(UObject* Object, FString FunctionName)
{
	FOutputDeviceNull ar;
	if (Object) Object->CallFunctionByNameWithArguments(*FunctionName, ar, NULL, true);
}

bool UDialogueAsset::ValidateDialogue(TArray<FText>& Errors) const
{
	TSet<int32> IDs;
	for (const FDialogueNode& Node : Data)
	{
		if (Node.id < 0 || IDs.Contains(Node.id))
			Errors.Add(FText::FromString(TEXT("Dialogue contains an invalid or duplicate node ID.")));
		IDs.Add(Node.id);
	}
	if (!IDs.Contains(0)) Errors.Add(FText::FromString(TEXT("Dialogue requires a start node (ID 0).")));
	for (const FDialogueNode& Node : Data)
		for (int32 Link : Node.Links)
			if (!IDs.Contains(Link)) Errors.Add(FText::FromString(FString::Printf(TEXT("Node %d links to missing node %d."), Node.id, Link)));
	return Errors.IsEmpty();
}

#if WITH_EDITOR
EDataValidationResult UDialogueAsset::IsDataValid(FDataValidationContext& Context) const
{
	TArray<FText> Errors;
	const bool bValid = ValidateDialogue(Errors);
	for (const FText& Error : Errors) Context.AddError(Error);
	return bValid ? EDataValidationResult::Valid : EDataValidationResult::Invalid;
}
#endif
