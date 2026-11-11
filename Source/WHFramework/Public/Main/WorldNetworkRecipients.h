#pragma once

#include "CoreMinimal.h"

class APlayerController;
class UWorld;

// 一条远程连接只对应一个接收端 World；分屏子连接使用父连接的控制器。
namespace WorldNetworkRecipients
{
	WHFRAMEWORK_API TArray<APlayerController*> Gather(UWorld* InWorld);
}
