// Copyright (c) 2024 Nikita Petrov (https://github.com/NikkittaP)
// SPDX-License-Identifier: MIT

#include "ScreenStreamMJPEGPlugin.h"

#define LOCTEXT_NAMESPACE "FScreenStreamMJPEGPluginModule"

void FScreenStreamMJPEGPluginModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
}

void FScreenStreamMJPEGPluginModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FScreenStreamMJPEGPluginModule, ScreenStreamMJPEGPlugin)