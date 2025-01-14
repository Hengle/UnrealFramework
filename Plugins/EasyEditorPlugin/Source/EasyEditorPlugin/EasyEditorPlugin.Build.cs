// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class EasyEditorPlugin : ModuleRules
{
	public EasyEditorPlugin(ReadOnlyTargetRules Target) : base(Target)
	{
#if UE_5_3_OR_LATER
        PCHUsage = PCHUsageMode.NoPCHs;
#else
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
#endif

		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				// ... add other public dependencies that you statically link with here ...
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"ToolMenus",
				"UnrealEd",
				"ContentBrowserData",
				"JsEnv"
			}
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);
		
		bEnableUndefinedIdentifierWarnings = false;
	}
}
