// Fill out your copyright notice in the Description page of Project Settings.


#include "ReferencePool/ReferencePoolModuleStatics.h"

#include "ReferencePool/ReferencePoolModule.h"

void UReferencePoolModuleStatics::ClearAllReference()
{
	if(UReferencePoolModule* ReferencePoolModule = UReferencePoolModule::Get())
	{
		ReferencePoolModule->ClearAllReference();
	}
}
