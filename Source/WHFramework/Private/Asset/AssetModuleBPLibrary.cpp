// Fill out your copyright notice in the Description page of Project Settings.


#include "Asset/AssetModuleBPLibrary.h"

#include "Asset/AssetModule.h"
#include "Main/MainModule.h"
#include "Main/MainModuleBPLibrary.h"

AAssetModule* UAssetModuleBPLibrary::GetAssetModule(UObject* InWorldContext)
{
	if(AMainModule* MainModule = UMainModuleBPLibrary::GetMainModule(InWorldContext))
	{
		return MainModule->GetModuleByClass<AAssetModule>();
	}
	return nullptr;
}
