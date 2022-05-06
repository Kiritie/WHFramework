// Fill out your copyright notice in the Description page of Project Settings.


#include "Widget/Window/SEditorWindowWidgetBase.h"

#define LOCTEXT_NAMESPACE "EditorWindowWidgetBase"

SEditorWindowWidgetBase::SEditorWindowWidgetBase()
{
	WindowTitle = LOCTEXT("EditorWindowWidgetBase", "EditorWindowWidgetBase");
	WindowSize = FVector2D(450, 450);
}

void SEditorWindowWidgetBase::Construct(const FArguments& InArgs)
{
	auto WindowArgs = InArgs._WindowArgs;
	WindowArgs.Title(WindowTitle);
	WindowArgs.SupportsMinimize(false);
	WindowArgs.SupportsMaximize(false);
	WindowArgs.ClientSize(WindowSize);
	SWindow::Construct(WindowArgs);
}

#undef LOCTEXT_NAMESPACE
