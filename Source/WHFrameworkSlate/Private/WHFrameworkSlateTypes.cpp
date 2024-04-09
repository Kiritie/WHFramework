// Copyright Epic Games, Inc. All Rights Reserved.

#include "WHFrameworkSlateTypes.h"

const FName FActivableIconStyle::TypeName( TEXT("FActivableIconStyle") );

const FActivableIconStyle& FActivableIconStyle::GetDefault()
{
	static FActivableIconStyle Default;
	return Default;
}

FActivableIconStyle::FActivableIconStyle()
{
}

void FActivableIconStyle::GetResources( TArray< const FSlateBrush* >& OutBrushes ) const
{
	OutBrushes.Add( &ActiveBrush );
	OutBrushes.Add( &InactiveBrush );
}
