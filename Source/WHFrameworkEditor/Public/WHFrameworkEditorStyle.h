// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateStyle.h"

/**  */
class FWHFrameworkEditorStyle
{
public:
	static void Initialize();
	static void Shutdown();

	static const ISlateStyle& Get();
	static FName GetStyleSetName();

private:
	static TSharedRef< class ISlateStyle > Create();
	static TSharedPtr< class ISlateStyle > Instance;

public:
	class FStyle : public FSlateStyleSet
	{
	public:
		FStyle();
		virtual ~FStyle() override;

		void Initialize();
	};
};