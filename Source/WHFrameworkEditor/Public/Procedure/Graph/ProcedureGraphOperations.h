#pragma once

#include "CoreMinimal.h"

class UProcedureBase;
class UProcedureAsset;
class UProcedureAssetGraph;

class WHFRAMEWORKEDITOR_API FProcedureGraphOperations
{
public:
    static bool CanEdit(const UProcedureAsset* Asset);
    static void Record(UProcedureAssetGraph* Graph);
    static void Changed(UProcedureAssetGraph* Graph);
    static bool InitializeStart(UProcedureAssetGraph* Graph);
    static bool CanConnect(const UProcedureAsset* Asset, FGuid From, FName Output, FGuid To);
    static bool Connect(UProcedureAssetGraph* Graph, FGuid From, FName Output, FGuid To);
    static bool Disconnect(UProcedureAssetGraph* Graph, FGuid NodeId, FName PinName = NAME_None, bool bInput = false, FGuid OtherId = FGuid());
    static UProcedureBase* Add(UProcedureAssetGraph* Graph, UClass* Class, FVector2D Position, FGuid Source = FGuid(), FName Output = NAME_None, FGuid Target = FGuid());
    static bool Delete(UProcedureAssetGraph* Graph, const TArray<FGuid>& Ids);
    static UProcedureBase* ChangeType(UProcedureAssetGraph* Graph, FGuid Id, UClass* Class);
    static bool Export(const UProcedureAsset* Asset, const TArray<FGuid>& Ids, FString& Text);
    static bool CanImport(const FString& Text);
    static TArray<FGuid> Import(UProcedureAssetGraph* Graph, const FString& Text, FVector2D Position);
    static bool AutoLayout(UProcedureAssetGraph* Graph);
};
