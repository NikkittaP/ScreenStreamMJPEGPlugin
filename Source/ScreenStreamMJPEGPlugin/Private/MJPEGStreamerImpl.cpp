// Copyright Epic Games, Inc. All Rights Reserved.

#include "MJPEGStreamerImpl.h"

FMJPEGStreamerImpl::FMJPEGStreamerImpl()
{
}

FMJPEGStreamerImpl::~FMJPEGStreamerImpl()
{
	// Ensure streamer is stopped on destruction
	Stop();
}

void FMJPEGStreamerImpl::Start(int Port)
{
	Streamer.start(Port);
}

void FMJPEGStreamerImpl::Stop()
{
	Streamer.stop();
}

void FMJPEGStreamerImpl::Publish(const std::string& Path, const std::string& Buffer)
{
	Streamer.publish(Path, Buffer);
}
