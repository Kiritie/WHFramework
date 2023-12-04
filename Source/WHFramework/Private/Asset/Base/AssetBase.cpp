// Copyright Epic Games, Inc. All Rights Reserved.

#include "Asset/Base/AssetBase.h"

UAssetBase::UAssetBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = FText::GetEmpty();

	NativeClass = GetClass();
}

#if WITH_EDITORONLY_DATA
void UAssetBase::Serialize(FStructuredArchive::FRecord Record)
{
	FArchive& UnderlyingArchive = Record.GetUnderlyingArchive();
	Super::Serialize(Record);

	if (UnderlyingArchive.IsLoading() && (UnderlyingArchive.UEVer() < VER_UE4_ADD_TRANSACTIONAL_TO_DATA_ASSETS))
	{
		SetFlags(RF_Transactional);
	}
}
#endif

void UAssetBase::Initialize(UAssetBase* InSource)
{
	SourceObject = InSource;
}
