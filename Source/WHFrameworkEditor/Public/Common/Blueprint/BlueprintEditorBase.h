// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Editor/Kismet/Public/BlueprintEditor.h"

//////////////////////////////////////////////////////////////////////////
// FBlueprintEditorBase
class FBlueprintEditorBase : public FBlueprintEditor
{
public:
	FBlueprintEditorBase();

	virtual ~FBlueprintEditorBase();
	
public:
	void InitBlueprintEditorBase(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, const TArray<UBlueprint*>& InBlueprints, bool bShouldOpenInDefaultsMode);

private:
	void EnsureBlueprintIsUpToDate(UBlueprint* Blueprint);

public:
	// IToolkit interface
	// FRED_TODO: don't merge this back
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FText GetToolkitName() const override;
	virtual FText GetToolkitToolTipText() const override;
	virtual FString GetWorldCentricTabPrefix() const override;
	virtual FLinearColor GetWorldCentricTabColorScale() const override;
	// End of IToolkit interface

	/** @return the documentation location for this editor */
	virtual FString GetDocumentationLink() const override;
	
	/** Returns a pointer to the Blueprint object we are currently editing, as long as we are editing exactly one */
	virtual UBlueprint* GetBlueprintObj() const override;

protected:
	TSubclassOf<UBlueprint> BlueprintClass;
	TSubclassOf<UEdGraphSchema_K2> GraphSchemaClass;
	
	FName ToolkitFName;
	FText BaseToolkitName;
	FText ToolkitNameFormat;
	
	FString WorldCentricTabPrefix;
	FLinearColor WorldCentricTabColorScale;
};
