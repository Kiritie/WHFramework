// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Gameplay/WHGameInstance.h"

UWHGameInstance::UWHGameInstance()
{

}

void UWHGameInstance::Init()
{
	Super::Init();
	
	FString Str;
	if(FFileHelper::LoadFileToString(Str, TEXT("E:/Wuhao/Unreal/Projects/Vigma3D/AAA.txt")))
	{
		const TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(Str);
		TSharedPtr<FJsonObject> JsonObject;
		if(FJsonSerializer::Deserialize(JsonReader, JsonObject) && JsonObject.IsValid())
		{
			int32 A = 0;
		}
	}
}

void UWHGameInstance::Shutdown()
{
	Super::Shutdown();
}
