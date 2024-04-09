// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Slate/Runtime/Interfaces/TickAbleWidgetInterface.h"

#include "UObject/Interface.h"
#include "WHFrameworkSlateTypes.h"
#include "EditorWidgetInterface.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UEditorWidgetInterface : public UTickAbleWidgetInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class WHFRAMEWORKSLATE_API IEditorWidgetInterface : public ITickAbleWidgetInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
};
