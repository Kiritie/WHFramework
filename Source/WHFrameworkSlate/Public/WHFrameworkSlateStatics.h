#pragma once
#include "Slate/SlateWidgetManager.h"

#define SNewEd( WidgetType, bAutoOpen, ... ) \
FSlateStatics::MakeTDeclEd<WidgetType>( #WidgetType, nullptr, bAutoOpen, __FILE__, __LINE__, RequiredArgs::MakeRequiredArgs(__VA_ARGS__) ) <<= TYPENAME_OUTSIDE_TEMPLATE WidgetType::FArguments()

#define SNewEdN( WidgetType, ParentWidget, bAutoOpen, ... ) \
FSlateStatics::MakeTDeclEd<WidgetType>( #WidgetType, ParentWidget, bAutoOpen, __FILE__, __LINE__, RequiredArgs::MakeRequiredArgs(__VA_ARGS__) ) <<= TYPENAME_OUTSIDE_TEMPLATE WidgetType::FArguments()

#define SAssignNewEd( ExposeAs, WidgetType, bAutoOpen, ... ) \
FSlateStatics::MakeTDeclEd<WidgetType>( #WidgetType, nullptr, bAutoOpen, __FILE__, __LINE__, RequiredArgs::MakeRequiredArgs(__VA_ARGS__) ) . Expose( ExposeAs ) <<= TYPENAME_OUTSIDE_TEMPLATE WidgetType::FArguments()

#define SAssignNewEdN( ExposeAs, WidgetType, ParentWidget, bAutoOpen, ... ) \
FSlateStatics::MakeTDeclEd<WidgetType>( #WidgetType, ParentWidget, bAutoOpen, __FILE__, __LINE__, RequiredArgs::MakeRequiredArgs(__VA_ARGS__) ) . Expose( ExposeAs ) <<= TYPENAME_OUTSIDE_TEMPLATE WidgetType::FArguments()

class FSlateStatics
{
public:
	template<typename WidgetType, typename RequiredArgsPayloadType>
	static TSlateDecl<WidgetType, RequiredArgsPayloadType> MakeTDeclEd( const ANSICHAR* InType, const TSharedPtr<class IEditorWidgetBase>& InParentWidget, bool bInAutoOpen, const ANSICHAR* InFile, int32 OnLine, RequiredArgsPayloadType&& InRequiredArgs )
	{
		LLM_SCOPE_BYTAG(UI_Slate);
		TSlateDecl<WidgetType, RequiredArgsPayloadType> SlateDecl(InType, InFile, OnLine, Forward<RequiredArgsPayloadType>(InRequiredArgs));
		FSlateWidgetManager::Get().CreateEditorWidget<WidgetType>(SlateDecl._Widget, InParentWidget, bInAutoOpen);
		return SlateDecl;
	}
};
