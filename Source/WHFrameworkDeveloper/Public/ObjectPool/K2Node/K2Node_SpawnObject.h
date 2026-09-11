#pragma once

#include "K2Node.h"
#include "K2Node_SpawnObject.generated.h"

UCLASS()
class WHFRAMEWORKDEVELOPER_API UK2Node_SpawnObject : public UK2Node
{
	GENERATED_BODY()

public:
	virtual void AllocateDefaultPins() override;

	virtual void ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;

	virtual void PinDefaultValueChanged(UEdGraphPin* Pin) override;

	virtual void PinConnectionListChanged(UEdGraphPin* Pin) override;

	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;

	virtual FText GetTooltipText() const override;

	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;

	virtual FText GetMenuCategory() const override;

protected:
	static bool CanUseStruct(const UScriptStruct* InStruct);

	UClass* ResolveObjectClass() const;

	UScriptStruct* ResolveSpawnParameterStruct() const;

	UEdGraphPin* GetClassPin() const;

	UEdGraphPin* GetResultPin() const;

	void RefreshClassFromPin(UEdGraphPin* Pin);

	UPROPERTY(EditAnywhere, Category = "ObjectPool")
	TObjectPtr<UClass> ObjectClass;
};
