#pragma once

#include "CoreMinimal.h"

class UStepAsset;
class UStepBase;

class WHFRAMEWORKEDITOR_API FStepGraphOperations
{
public:
	static bool CanEdit(const UStepAsset* Asset);
	static UStepBase* AddStep(UStepAsset* Asset, UClass* StepClass, FVector2D Position, const FString& ParentGUID = FString(), const FString& ChildGUID = FString());
	static bool DeleteSteps(UStepAsset* Asset, const TArray<UStepBase*>& Steps);
	static UStepBase* ChangeStepType(UStepAsset* Asset, UStepBase* Step, UClass* StepClass);
	static bool CanConnect(const UStepAsset* Asset, const FString& ParentGUID, const FString& ChildGUID, FText& OutReason);
	static bool Connect(UStepAsset* Asset, const FString& ParentGUID, const FString& ChildGUID);
	static bool Disconnect(UStepAsset* Asset, const FString& GUID, bool bInput, bool bOutput);
	static bool DisconnectEdge(UStepAsset* Asset, const FString& ParentGUID, const FString& ChildGUID);
	static bool ExportSteps(const UStepAsset* Asset, const TArray<UStepBase*>& Steps, FString& OutText);
	static bool CanImportSteps(const FString& Text);
	static TArray<UStepBase*> ImportSteps(UStepAsset* Asset, const FString& Text, FVector2D Position, bool bPreserveGUIDs = false, bool bConnectRoots = false);
	static void AutoLayout(UStepAsset* Asset);
	static void Compile(UStepAsset* Asset);
	static void ModifyForEdit(UStepAsset* Asset);
};
