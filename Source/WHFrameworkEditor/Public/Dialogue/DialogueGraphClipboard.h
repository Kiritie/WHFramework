#pragma once

#include "CoreMinimal.h"

class UDialogueAsset;

class WHFRAMEWORKEDITOR_API FDialogueGraphClipboard
{
public:
	static bool ExportNodes(const UDialogueAsset* Asset, const TArray<int32>& NodeIDs, FString& OutText);
	static bool ImportNodes(UDialogueAsset* Asset, const FString& Text, const FVector2D& Position, TArray<int32>& OutNodeIDs);
};
