#pragma once

#include "CoreMinimal.h"

// 编辑器
DEFINE_LOG_CATEGORY_STATIC(WH_Editor, Log, All);

// 能力
DEFINE_LOG_CATEGORY_STATIC(WH_Ability, Log, All);

// 资源
DEFINE_LOG_CATEGORY_STATIC(WH_Asset, Log, All);

// 音频
DEFINE_LOG_CATEGORY_STATIC(WH_Audio, Log, All);

// 相机
DEFINE_LOG_CATEGORY_STATIC(WH_Camera, Log, All);

// 角色
DEFINE_LOG_CATEGORY_STATIC(WH_Character, Log, All);

// 调试
DEFINE_LOG_CATEGORY_STATIC(WH_Debug, Log, All);

// 事件
DEFINE_LOG_CATEGORY_STATIC(WH_Event, Log, All);

// 输入
DEFINE_LOG_CATEGORY_STATIC(WH_Input, Log, All);

// 潜行任务
DEFINE_LOG_CATEGORY_STATIC(WH_LatentAction, Log, All);

// 媒体
DEFINE_LOG_CATEGORY_STATIC(WH_Media, Log, All);

// 网络
DEFINE_LOG_CATEGORY_STATIC(WH_Network, Log, All);

// 对象池
DEFINE_LOG_CATEGORY_STATIC(WH_ObjectPool, Log, All);

// 参数
DEFINE_LOG_CATEGORY_STATIC(WH_Parameter, Log, All);

// 流程
DEFINE_LOG_CATEGORY_STATIC(WH_Procedure, Log, All);

// 引用池
DEFINE_LOG_CATEGORY_STATIC(WH_ReferencePool, Log, All);

// 存档
DEFINE_LOG_CATEGORY_STATIC(WH_SaveGame, Log, All);

// 场景
DEFINE_LOG_CATEGORY_STATIC(WH_Scene, Log, All);

// 步骤
DEFINE_LOG_CATEGORY_STATIC(WH_Step, Log, All);

// 体素
DEFINE_LOG_CATEGORY_STATIC(WH_Voxel, Log, All);

// Web请求
DEFINE_LOG_CATEGORY_STATIC(WH_WebRequest, Log, All);

// UI
DEFINE_LOG_CATEGORY_STATIC(WH_Widget, Log, All);

// 打印
#define WH_LOG(CategoryName, Verbosity, Format, ...) \
{ \
	UE_LOG(CategoryName, Verbosity, Format, ##__VA_ARGS__); \
}

// 断言实现
#define WH_ENSURE_IMPL(Capture, InExpression, InFormat, ...) \
(LIKELY(!!(InExpression)) || (([Capture] () ->bool \
{ \
	static bool bExecuted = false; \
	if ((!bExecuted)) \
	{ \
		bExecuted = true; \
		FString Expr = #InExpression;\
		FString File = __FILE__;\
		int32 Line = __LINE__;\
		UE_LOG(LogTemp, Error, TEXT("WHFramework UE4 CRASH"));\
		UE_LOG(LogTemp, Error, TEXT("Expression : %s, %s, %d"), *Expr, *File, Line);\
		UE_LOG(LogTemp, Error, InFormat, ##__VA_ARGS__); \
	} \
	return false; \
})()))

// 用这两个断言, 代替多数仅需要在编辑器中进行判断的断言
#if WITH_EDITOR
	#define ensureEditor(InExpression) ensureAlways(InExpression)
	#define ensureEditorMsgf( InExpression, InFormat, ... ) ensureAlwaysMsgf( InExpression, InFormat, ##__VA_ARGS__)
#else
	#define ensureEditor(InExpression) WH_ENSURE_IMPL( , InExpression, TEXT(""))
	#define ensureEditorMsgf( InExpression, InFormat, ... ) WH_ENSURE_IMPL(&, InExpression, InFormat, ##__VA_ARGS__)
#endif

/*
 * 输出调试信息到游戏窗口
 * @param Message 消息内容
 * @param DisplayColor 显示颜色
 * @param Duration 持续时间
 * @param bNewerOnTop 不更新在顶部
 */
void WHDebug(const FString& Message, FColor DisplayColor = FColor::Cyan, float Duration = 1.5f, bool bNewerOnTop = true);

