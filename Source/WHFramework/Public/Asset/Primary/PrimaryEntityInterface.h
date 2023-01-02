// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "WHFramework.h"

#include "UObject/Interface.h"
#include "PrimaryEntityInterface.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UPrimaryEntityInterface : public UInterface
{
	GENERATED_BODY()
};

/**
* 
*/
class WHFRAMEWORK_API IPrimaryEntityInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	virtual FPrimaryAssetId GetAssetID() const = 0;
};
