// Copyright (c) 2024 Nikita Petrov (https://github.com/NikkittaP)
// SPDX-License-Identifier: MIT

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
