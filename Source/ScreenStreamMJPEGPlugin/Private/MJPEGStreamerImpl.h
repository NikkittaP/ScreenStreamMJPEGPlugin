// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "mjpeg_streamer.hpp"

/**
 * Pimpl wrapper for nadjieb::MJPEGStreamer
 * Hides implementation details from public headers
 */
class FMJPEGStreamerImpl
{
public:
	FMJPEGStreamerImpl();
	~FMJPEGStreamerImpl();

	// Wrapper methods for MJPEGStreamer functionality
	void Start(int Port);
	void Stop();
	void Publish(const std::string& Path, const std::string& Buffer);

private:
	nadjieb::MJPEGStreamer Streamer;
};
