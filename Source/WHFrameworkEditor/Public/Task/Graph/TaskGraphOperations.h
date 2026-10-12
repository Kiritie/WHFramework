#pragma once

#include "CoreMinimal.h"

class UTaskAsset;
class UTaskBase;

class WHFRAMEWORKEDITOR_API FTaskGraphOperations
{
public:
	static bool CanEdit(const UTaskAsset* Asset);
	static void ModifyAsset(UTaskAsset* Asset);
	static UTaskBase* AddTask(UTaskAsset* Asset, UClass* TaskClass, FVector2D Position, UTaskBase* Parent = nullptr, UTaskBase* Child = nullptr, bool bConnectToStart = false);
	static bool CanConnect(const UTaskAsset* Asset, UTaskBase* Parent, UTaskBase* Child, bool bFromStart, FText& OutReason);
	static bool Connect(UTaskAsset* Asset, UTaskBase* Parent, UTaskBase* Child, bool bFromStart);
	static bool Disconnect(UTaskAsset* Asset, UTaskBase* Task, bool bInput, bool bOutput, bool bStartNode = false);
	static bool DisconnectEdge(UTaskAsset* Asset, UTaskBase* Parent, UTaskBase* Child, bool bFromStart);
	static bool DeleteTasks(UTaskAsset* Asset, const TArray<UTaskBase*>& Tasks);
	static UTaskBase* ChangeTaskType(UTaskAsset* Asset, UTaskBase* Task, UClass* TaskClass);
	static bool ExportTasks(const UTaskAsset* Asset, const TArray<UTaskBase*>& Tasks, FString& OutText);
	static bool CanImportTasks(const FString& Text);
	static TArray<UTaskBase*> ImportTasks(UTaskAsset* Asset, const FString& Text, FVector2D Position);
	static void AutoLayout(UTaskAsset* Asset);
	static void SortSiblings(UTaskAsset* Asset, UTaskBase* Task);
	static void SetNodePosition(UTaskAsset* Asset, UTaskBase* Task, bool bStartNode, FVector2D Position);
	static bool MigrateAsset(UTaskAsset* Asset, TArray<FText>& OutErrors, bool bCompile = true);
};
