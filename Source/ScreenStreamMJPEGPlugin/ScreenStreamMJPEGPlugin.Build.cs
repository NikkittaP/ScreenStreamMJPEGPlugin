// Copyright (c) 2024 Nikita Petrov (https://github.com/NikkittaP)
// SPDX-License-Identifier: MIT

using UnrealBuildTool;

public class ScreenStreamMJPEGPlugin : ModuleRules
{
	public ScreenStreamMJPEGPlugin(ReadOnlyTargetRules Target) : base(Target)
	{
		// C++ Standard: Use C++20 (required for UE 5.5+, compatible with 5.2-5.4)
		CppStandard = CppStandardVersion.Cpp20;
		
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

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
				"InputCore",
				"ImageWrapper",
				"RenderCore",
				"Renderer",
				"RHI"
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
				"UMG",
				// ... add private dependencies that you statically link with here ...	
			}
			);


		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);
	}
}
