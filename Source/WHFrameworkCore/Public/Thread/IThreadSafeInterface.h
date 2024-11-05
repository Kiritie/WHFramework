// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

class WHFRAMEWORKCORE_API IThreadSafeInterface
{
public:
	IThreadSafeInterface();

protected:
	FCriticalSection CriticalSection;
};
