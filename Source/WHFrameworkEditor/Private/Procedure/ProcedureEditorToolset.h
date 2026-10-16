#pragma once

#include "ToolsetRegistry/ToolsetDefinition.h"
#include "UObject/StrongObjectPtr.h"
#include "ProcedureEditorToolset.generated.h"

class UProcedureAsset;
class UProcedureAssetGraph;
class UProcedureBase;

// MCP流程图操作复用编辑器事务、连接约束和编译机制；保存由资产接口显式执行。
UCLASS()
class UProcedureEditorToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	UFUNCTION(meta = (AICallable))
	static FString GetGraphData(UProcedureAsset* Asset);

	UFUNCTION(meta = (AICallable))
	static bool ConnectNodes(UProcedureAsset* Asset, FGuid From, FName Output, FGuid To);

	UFUNCTION(meta = (AICallable))
	static bool DisconnectNodes(UProcedureAsset* Asset, FGuid NodeId, FName PinName, bool bInput, FGuid OtherId);

	UFUNCTION(meta = (AICallable))
	static UProcedureBase* AddNode(UProcedureAsset* Asset, TSubclassOf<UProcedureBase> NodeClass, FVector2D Position);

	UFUNCTION(meta = (AICallable))
	static bool Recompile(UProcedureAsset* Asset);

private:
	static TStrongObjectPtr<UProcedureAssetGraph> GetEditableGraph(UProcedureAsset* Asset);
};
