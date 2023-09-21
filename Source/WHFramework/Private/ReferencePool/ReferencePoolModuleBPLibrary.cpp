// Fill out your copyright notice in the Description page of Project Settings.


#include "ReferencePool/ReferencePoolModuleBPLibrary.h"

#include "ReferencePool/ReferencePoolModule.h"

void UReferencePoolModuleBPLibrary::ClearAllReference()
{
	if(AReferencePoolModule* ReferencePoolModule = AReferencePoolModule::Get())
	{
		ReferencePoolModule->ClearAllReference();
	}
}
