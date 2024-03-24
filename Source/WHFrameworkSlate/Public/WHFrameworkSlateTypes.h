// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "Base/SEditorWidgetBase.h"

#define SNewEd( WidgetType, bAutoOpen, ... ) \
MakeTDeclEd<WidgetType>( #WidgetType, nullptr, bAutoOpen, __FILE__, __LINE__, RequiredArgs::MakeRequiredArgs(__VA_ARGS__) ) <<= TYPENAME_OUTSIDE_TEMPLATE WidgetType::FArguments()

#define SNewEdN( WidgetType, ParentWidget, bAutoOpen, ... ) \
MakeTDeclEd<WidgetType>( #WidgetType, ParentWidget, bAutoOpen, __FILE__, __LINE__, RequiredArgs::MakeRequiredArgs(__VA_ARGS__) ) <<= TYPENAME_OUTSIDE_TEMPLATE WidgetType::FArguments()

#define SAssignNewEd( ExposeAs, WidgetType, bAutoOpen, ... ) \
MakeTDeclEd<WidgetType>( #WidgetType, nullptr, bAutoOpen, __FILE__, __LINE__, RequiredArgs::MakeRequiredArgs(__VA_ARGS__) ) . Expose( ExposeAs ) <<= TYPENAME_OUTSIDE_TEMPLATE WidgetType::FArguments()

#define SAssignNewEdN( ExposeAs, WidgetType, ParentWidget, bAutoOpen, ... ) \
MakeTDeclEd<WidgetType>( #WidgetType, ParentWidget, bAutoOpen, __FILE__, __LINE__, RequiredArgs::MakeRequiredArgs(__VA_ARGS__) ) . Expose( ExposeAs ) <<= TYPENAME_OUTSIDE_TEMPLATE WidgetType::FArguments()

template<typename WidgetType, typename RequiredArgsPayloadType>
TSlateDecl<WidgetType, RequiredArgsPayloadType> MakeTDeclEd( const ANSICHAR* InType, const TSharedPtr<SEditorWidgetBase>& InParentWidget, bool bInAutoOpen, const ANSICHAR* InFile, int32 OnLine, RequiredArgsPayloadType&& InRequiredArgs )
{
	LLM_SCOPE_BYTAG(UI_Slate);
	TSlateDecl<WidgetType, RequiredArgsPayloadType> SlateDecl(InType, InFile, OnLine, Forward<RequiredArgsPayloadType>(InRequiredArgs));
	if(InParentWidget)
	{
		InParentWidget->AddChild(SlateDecl._Widget);
	}
	if(bInAutoOpen)
	{
		SlateDecl._Widget->Open();
	}
	else
	{
		SlateDecl._Widget->SetVisibility(EVisibility::Collapsed);
	}
	return SlateDecl;
}
