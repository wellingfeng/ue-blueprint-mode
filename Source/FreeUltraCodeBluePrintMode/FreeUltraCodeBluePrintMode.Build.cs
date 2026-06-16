// Copyright FreeUltraCode BluePrint Mode. All Rights Reserved.

using UnrealBuildTool;

public class FreeUltraCodeBluePrintMode : ModuleRules
{
	public FreeUltraCodeBluePrintMode(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"Json",
			"JsonUtilities",
			"HTTP",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			// 编辑器相关
			"UnrealEd",
			"Slate",
			"SlateCore",
			"EditorStyle",
			"EditorSubsystem",
			"ToolMenus",
			"Projects",            // IPluginManager：检测插件安装/版本
			"AssetRegistry",       // 按名字查蓝图资产
			"AssetTools",          // 创建蓝图资产
			"ContentBrowser",
			"PropertyEditor",      // 设置页 IDetailCustomization 注入按钮
			"DeveloperSettings",   // 项目设置「蓝图」分类页
			// 蓝图图表操作
			"BlueprintGraph",      // UK2Node_* 节点
			"GraphEditor",
			"Kismet",              // FKismetEditorUtilities 编译
			"KismetCompiler",
			"UMG",                 // Widget Blueprint 支持
			"UMGEditor",
		});
	}
}
