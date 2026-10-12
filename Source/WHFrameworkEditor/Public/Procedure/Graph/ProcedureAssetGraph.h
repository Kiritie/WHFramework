#pragma once

#include "EdGraph/EdGraph.h"
#include "ProcedureAssetGraph.generated.h"

class UProcedureAsset;
class UProcedureAssetGraphNode;

UCLASS(Transient)
class WHFRAMEWORKEDITOR_API UProcedureAssetGraph : public UEdGraph
{
    GENERATED_BODY()

public:
    UPROPERTY()
    TObjectPtr<UProcedureAsset> Asset;

    FSimpleDelegate OnChanged;
    UProcedureAssetGraphNode* FindNode(const FGuid& InId) const;
    void Rebuild();
};
