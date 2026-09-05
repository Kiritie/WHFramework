#pragma once

#include "CoreMinimal.h"

class UTaskAsset;
class UTaskBase;

class WHFRAMEWORKEDITOR_API FTaskGraphOperations
{
public:
	static bool CanEdit(const UTaskAsset* Asset);
	static UTaskBase* AddTask(UTaskAsset* Asset, UClass* TaskClass, FVector2D Position, UTaskBase* Parent = nullptr, UTaskBase* Child = nullptr);
	static bool DeleteTasks(UTaskAsset* Asset, const TArray<UTaskBase*>& Tasks);
	static UTaskBase* ChangeTaskType(UTaskAsset* Asset, UTaskBase* Task, UClass* TaskClass);
	static bool ExportTasks(const UTaskAsset* Asset, const TArray<UTaskBase*>& Tasks, FString& OutText);
	static bool CanImportTasks(const FString& Text);
	static TArray<UTaskBase*> ImportTasks(UTaskAsset* Asset, const FString& Text, FVector2D Position);
	static void AutoLayout(UTaskAsset* Asset);
	static void SortSiblings(UTaskAsset* Asset, UTaskBase* Task);
};
