// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


#include "CoreMinimal.h"

#include "UnsignedNarrowTime.h"
#include "Containers/CircularQueue.h"
#include <cstdint>

//centralizing the typedefs to avoid circularized header includes
//and further ease swapping over between 8 and 16 byte modes. IWYU!
namespace Cabling
{
	typedef uint64_t PacketElement;
	typedef std::pair<uint32_t, long> CycleTimestamp;
	typedef TCircularQueue<CycleTimestamp> TimestampQ;
	typedef TSharedPtr<TimestampQ, ESPMode::ThreadSafe> TimestampQueue;
	typedef TSharedPtr<TCircularQueue<PacketElement>, ESPMode::ThreadSafe> SendQueue; // note that the queues only support 1p1c mode.
	constexpr uint32_t LongboySendHertz = 120;
	constexpr uint32_t CablingSampleHertz = 512;
	constexpr uint32_t BristleconeSendHertz = 90;
	static constexpr float SLEEP_TIME_BETWEEN_THREAD_TICKS = 0.008f;
}