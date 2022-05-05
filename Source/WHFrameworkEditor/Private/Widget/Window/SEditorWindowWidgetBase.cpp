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
	SWindow::Construct(SWindow::FArguments()
		.Title(WindowTitle)
		.SupportsMinimize(false)
		.SupportsMaximize(false)
		.ClientSize(WindowSize));
}

#undef LOCTEXT_NAMESPACE