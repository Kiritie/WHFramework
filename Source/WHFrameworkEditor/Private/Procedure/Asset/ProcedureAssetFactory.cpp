// Copyright Epic Games, Inc. All Rights Reserved.

#include "Procedure/Asset/ProcedureAssetFactory.h"

#include "Procedure/Base/ProcedureAsset.h"

#define LOCTEXT_NAMESPACE "ProcedureAssetFactory"

UProcedureAssetFactory::UProcedureAssetFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UProcedureAsset::StaticClass();
	ParentClass = UProcedureAsset::StaticClass();
}

#undef LOCTEXT_NAMESPACE // "ProcedureAssetFactory"
