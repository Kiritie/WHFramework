// Fill out your copyright notice in the Description page of Project Settings.


#include "ReferencePool/ReferencePoolModuleStatics.h"

#include "ReferencePool/ReferencePoolModule.h"

void UReferencePoolModuleStatics::ClearAllReference()
{
	UReferencePoolModule::Get().ClearAllReference();
}
