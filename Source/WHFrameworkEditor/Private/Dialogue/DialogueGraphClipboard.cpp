#include "Dialogue/DialogueGraphClipboard.h"

#include "Dialogue/Base/DialogueAsset.h"
#include "Dialogue/Base/DialogueEventBase.h"
#include "Dialogue/Base/DialogueConditionBase.h"
#include "JsonObjectConverter.h"
#include "UObject/StrongObjectPtr.h"
#include "ScopedTransaction.h"

bool FDialogueGraphClipboard::ExportNodes(const UDialogueAsset* Asset, const TArray<int32>& NodeIDs, FString& OutText)
{
	if (!Asset) return false;
	TArray<TSharedPtr<FJsonValue>> Nodes;
	for (FDialogueNode Node : Asset->Data)
	{
		if (Node.id <= 0 || !NodeIDs.Contains(Node.id)) continue;
		Node.Links.RemoveAll([&](int32 ID) { return ID <= 0 || !NodeIDs.Contains(ID); });
		TSharedRef<FJsonObject> JsonNode = MakeShared<FJsonObject>();
		if (!FJsonObjectConverter::UStructToJsonObject(FDialogueNode::StaticStruct(), &Node, JsonNode, 0, CPF_Transient, nullptr,
			EJsonObjectConversionFlags::WriteTextAsComplexString | EJsonObjectConversionFlags::SkipStandardizeCase)) return false;
		Nodes.Add(MakeShared<FJsonValueObject>(JsonNode));
	}
	if (Nodes.IsEmpty()) return false;
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("Format"), TEXT("WHFramework.DialogueNodes"));
	Root->SetArrayField(TEXT("Data"), Nodes);
	return FJsonSerializer::Serialize(Root, TJsonWriterFactory<>::Create(&OutText));
}

bool FDialogueGraphClipboard::ImportNodes(UDialogueAsset* Asset, const FString& Text, const FVector2D& Position, TArray<int32>& OutNodeIDs)
{
	OutNodeIDs.Reset();
	if (!Asset) return false;
	TSharedPtr<FJsonObject> Json;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Json) || !Json) return false;
	FString Format;
	if (!Json->TryGetStringField(TEXT("Format"), Format) || Format != TEXT("WHFramework.DialogueNodes")) return false;
	const TArray<TSharedPtr<FJsonValue>>* Nodes;
	if (!Json->TryGetArrayField(TEXT("Data"), Nodes) || Nodes->IsEmpty()) return false;
	TSharedRef<FJsonObject> DataOnly = MakeShared<FJsonObject>();
	DataOnly->SetArrayField(TEXT("Data"), *Nodes);
	TStrongObjectPtr<UDialogueAsset> Imported(NewObject<UDialogueAsset>());
	if (!FJsonObjectConverter::JsonObjectToUStruct(DataOnly, UDialogueAsset::StaticClass(), Imported.Get(), 0, CPF_Transient)) return false;
	TMap<int32, int32> IDs;
	int32 NextID = FMath::Max(1, Asset->NextNodeId);
	for (const FDialogueNode& Node : Asset->Data) NextID = FMath::Max(NextID, Node.id + 1);
	FVector2D Center = FVector2D::ZeroVector;
	for (const FDialogueNode& Node : Imported->Data)
	{
		if (Node.id <= 0 || IDs.Contains(Node.id)) return false;
		IDs.Add(Node.id, NextID++);
		Center += Node.Coordinates;
	}
	if (IDs.IsEmpty()) return false;
	Center /= IDs.Num();
	const FScopedTransaction Transaction(NSLOCTEXT("DialogueEditor", "PasteNodes", "Paste Dialogue Nodes"));
	Asset->Modify();
	for (FDialogueNode Node : Imported->Data)
	{
		Node.id = IDs[Node.id];
		Node.Links.RemoveAll([&](int32 ID) { return !IDs.Contains(ID); });
		for (int32& ID : Node.Links) ID = IDs[ID];
		Node.Coordinates += Position - Center;
		for (UDialogueEventBase*& Event : Node.Events)
			if (Event) Event = DuplicateObject<UDialogueEventBase>(Event, Asset);
		for (UDialogueConditionBase*& Condition : Node.Conditions)
			if (Condition) Condition = DuplicateObject<UDialogueConditionBase>(Condition, Asset);
		OutNodeIDs.Add(Node.id);
		Asset->Data.Add(MoveTemp(Node));
	}
	Asset->NextNodeId = NextID;
	Asset->MarkPackageDirty();
	return true;
}
