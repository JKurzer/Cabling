#include "FCablingRunner.h"

#include <bitset>
#include <thread>
using std::bitset;

FCabling::FCabling()
	: running(false)
{
}

FCabling::~FCabling()
{
}


bool FCabling::Init()
{
	UE_LOG(LogTemp, Display, TEXT("FCabling: Initializing Control Intercept thread"));

	running = true;
	return true;
}


bool FCabling::SendNew(bool sent, uint64_t priorReading, uint64_t currentRead)
{
	if (
		(!sent) && (currentRead != priorReading)
	)
	{
		//push to both queues.
		this->CabledThreadControlQueue.Get()->Enqueue(currentRead);
		this->GameThreadControlQueue.Get()->Enqueue(currentRead);
		WakeTransmitThread->Trigger();
		return true;
	}
	return sent;
}

bool FCabling::SendIfWindowEdge(bool sent, int seqNumber, uint64_t currentRead,
									  const uint32_t sendHertzFactor)
{
	if (
		//if we haven't sent, we do have a reading...
		(!sent) && currentRead != 0 &&
		//and we're out of bloosy time.
		((seqNumber % sendHertzFactor) != 0)
	)
	{
		//push to both queues.
		this->CabledThreadControlQueue.Get()->Enqueue(currentRead);
		this->GameThreadControlQueue.Get()->Enqueue(currentRead);
		WakeTransmitThread->Trigger();
		return true;
	}
	return sent;
}

uint64_t FCabling::FromKeyboardState(uint32_t keyCount, GameInputKeyState (&states)[16])
{
	double xMagnitude = 0.0;
	double yMagnitude = 0.0;

	for (uint32_t i = 0; i < keyCount; i++)
	{
		if(states[i].codePoint == 0 && states[i].scanCode == 0)
		{
			break; //0,0 is indicates end of valid data per api doc.
		}
		//https://learn.microsoft.com/en-us/windows/win32/inputdev/virtual-key-codes
		// W
		if (states[i].codePoint == 0x57)
		{
			yMagnitude += 1.0;
		}
		// A
		if (states[i].virtualKey == 0x41)
		{
			xMagnitude -= 1.0;
		}
		// S
		if (states[i].codePoint == 0x53)
		{
			yMagnitude -= 1.0;
		}
		// D
		if (states[i].codePoint == 0x44)
		{
			xMagnitude += 1.0;
		}
	}

	FCableInputPacker boxing;
	boxing.lx = (uint32_t)boxing.IntegerizedStick(xMagnitude);
	boxing.ly = (uint32_t)boxing.IntegerizedStick(yMagnitude);
	boxing.rx = (uint32_t)boxing.IntegerizedStick(0.0);
	boxing.ry = (uint32_t)boxing.IntegerizedStick(0.0);
	boxing.buttons = 0; // temporarily no buttons
	boxing.events = 0;
	uint64_t currentRead = boxing.PackImpl();
	//don't check events because we may set an event to indicate that we're on keeb input....
	return currentRead;
}

uint64_t FCabling::KeyboardState(IGameInputReading* reading, GameInputKeyState (&states)[16])
{
	uint32_t keyCount = reading->GetKeyCount();
	//if you hold down more than 16 keys, you need help or you're using macros.
	;
	reading->GetKeyState(keyCount, states);

	return FromKeyboardState(keyCount, states);
}

uint64_t FCabling::FromGamePadState(GameInputGamepadState state)
{
	FCableInputPacker boxing;
	//very fun story. unless you explicitly import and use std::bitset
	//the wrong thing happens here. I'm not going to speculate on why, because
	//I don't think I can do so without swearing extensively.
	boxing.lx = (uint32_t)boxing.IntegerizedStick(state.leftThumbstickX);
	boxing.ly = (uint32_t)boxing.IntegerizedStick(state.leftThumbstickY);
	boxing.rx = (uint32_t)boxing.IntegerizedStick(state.rightThumbstickX);
	boxing.ry = (uint32_t)boxing.IntegerizedStick(state.rightThumbstickY);
	boxing.buttons = (uint32_t)state.buttons; //strikingly, there's no paddle field.
	boxing.buttons.set(12, (state.leftTrigger > 0.55)); //check the bitfield.
	boxing.buttons.set(13, (state.rightTrigger > 0.55));
	boxing.events = 0;
	uint64_t currentRead = boxing.PackImpl();

	//because we deadzone and integerize, we actually have a pretty good idea
	//of when input actually changes. 2048 positions for the stick along each axis
	//actually looks like it's enough to give us precise movement while still
	//excising some amount of jitter. Because we always round down, you have to move
	//fully to a new position and this seems to be a larger delta than the average
	//heart-rate jitter or control noise.
	//TODO: this line might actually be wrong. I just redid the math, and it looks right but...
	return currentRead;
}

//Sets Sent if prip
uint64_t FCabling::GamepadState(IGameInputReading* reading)
{
	// If no device has been assigned to g_gamepad yet, set it
	// to the first device we receive input from. (This must be
	// the one the player is using because it's generating input.)

	// Retrieve the fixed-format gamepad state from the reading.
	GameInputGamepadState state;
	reading->GetGamepadState(&state);
	return FromGamePadState(state);
}

//this is based directly on the gameinput sample code.
uint32 FCabling::Run()
{
	IGameInput* g_gameInput = nullptr;
	HRESULT gameInputSpunUp = GameInputCreate(&g_gameInput);
	IGameInputDevice* g_gamepad = nullptr;
	IGameInputReading* reading;
	GameInputKeyState states[16] = {{0,0,0,false}}; //the first 0,0 indicates the end of valid data.
	bool Sent = false;
	//TODO: why does this seem to need to be an int? I'm assuming something about type 64/32 coercion or
	//the mod operator is that I knew when I wrote this code, but I no longer remember and it should get a docs note.
	int SeqNumber = 0;
	uint64_t PriorReadingKeyboard = 0;

	uint64_t PriorReadingGamepad = 0;
	//Hi! Jake here! Reminding you that this will CYCLE
	//That's known. Isn't that fun? :) Don't reorder these, by the way.
	uint32_t lastPollTime = NarrowClock::getSlicedMicrosecondNow();
	uint32_t lsbTime = NarrowClock::getSlicedMicrosecondNow();
	constexpr uint32_t sampleHertz = Cabling::CablingSampleHertz;
	constexpr uint32_t sendHertz = Cabling::BristleconeSendHertz;
	constexpr uint32_t sendHertzFactor = sampleHertz / sendHertz;
	constexpr uint32_t Period = 1000000 / sampleHertz; //swap to microseconds. standardizing.


	constexpr auto HalfStep = std::chrono::microseconds(Period / 2);
	const uint64_t BlankGamepad = FromGamePadState(GameInputGamepadState());
	const uint64_t BlankKeyboard = FromKeyboardState(16, states);
	
	//We're using the GameInput lib.
	//https://learn.microsoft.com/en-us/gaming/gdk/_content/gc/input/overviews/input-overview
	//https://learn.microsoft.com/en-us/gaming/gdk/_content/gc/input/advanced/input-keyboard-mouse will be fun
	//https://handmade.network/forums/t/8710-using_microsoft_gameinput_api_with_multiple_controllers#29361
	//Looks like PS4/PS5 won't be too bad, just gotta watch out for Fun Device ID changes.
	while (running)
	{
		if ((lastPollTime + Period) <= lsbTime)
		{
			lastPollTime = lsbTime;
			//if it's been blown up or if create failed.
			if (!g_gameInput || !SUCCEEDED(gameInputSpunUp))
			{
				gameInputSpunUp = GameInputCreate(&g_gameInput);
			}

			
			{
				IGameInputDevice* keyboard = nullptr;
				uint64_t KeyboardCurrentRead = BlankKeyboard;
				uint64_t GamepadCurrentRead = BlankGamepad;
				//get the keeb...
				if (g_gameInput &&
					SUCCEEDED(g_gameInput->GetCurrentReading(GameInputKindKeyboard, keyboard, &reading)))
				{
					//if we don't have a WASD input, we don't send, and we'll check the controller next.
					KeyboardCurrentRead = KeyboardState(reading, states);
					reading->Release();
				}
				// AND get the gamepad... we need both inputs to check which has data.
				if (g_gameInput &&
					SUCCEEDED(g_gameInput->GetCurrentReading(GameInputKindGamepad, g_gamepad, &reading)))
				{
					if (!g_gamepad)
					{
						reading->GetDevice(&g_gamepad);
					}
					GamepadCurrentRead = GamepadState(reading);
					reading->Release();
				}
				else if (g_gamepad != nullptr)
				// if gamepad read failed but a gamepad exists, we're in a failed state.
				{
					g_gamepad->Release(); //release it, we'll reacquire it on the next pass.
					g_gamepad = nullptr;
				}
				
				Sent = SendNew(Sent, PriorReadingKeyboard, KeyboardCurrentRead);
				Sent = SendNew(Sent, PriorReadingGamepad, GamepadCurrentRead);
				if(GamepadCurrentRead != BlankGamepad)
				{
					Sent = SendIfWindowEdge(Sent, SeqNumber, GamepadCurrentRead, sendHertzFactor);
				}
				else if (KeyboardCurrentRead != BlankKeyboard)
				{
					Sent = SendIfWindowEdge(Sent, SeqNumber, KeyboardCurrentRead, sendHertzFactor);
				}
				//this check isn't needed, but removing it creates an instant maintenance hazard.
				else if (((SeqNumber % sendHertzFactor) != 0)) 
				{
					Sent = SendIfWindowEdge(Sent, SeqNumber, BlankGamepad, sendHertzFactor);
				}
				
				//this is performed even if they're null data packets. Godspeed.
				PriorReadingGamepad = GamepadCurrentRead;
				PriorReadingKeyboard = KeyboardCurrentRead;
			}
			
			if ((SeqNumber % sampleHertz) == 0)
			{
				long long now = std::chrono::steady_clock::now().time_since_epoch().count();
				UE_LOG(LogTemp, Display, TEXT("Cabling hertz cycled: %lld against lsb %lld with last poll as %lld"),
				       (now), lsbTime, lastPollTime);
			}

			if ((SeqNumber % sendHertzFactor) == 0)
			{
				Sent = false;
			}
			++SeqNumber;
		}
		//if this is the case, we've looped round. rather than verifying, we'll just miss one chance to poll.
		//sequence number is still the actual arbiter, so we'll only send every 4 periods, even if we poll
		//one less or one more time.

		//modified to ensure we don't oversleep. we busy cycle if we're less than 1.1 ms out.
		lsbTime = NarrowClock::getSlicedMicrosecondNow();
		if (lsbTime <= (lastPollTime + Period) - (1.1 * (HalfStep).count()))
		{
			std::this_thread::sleep_for(HalfStep);
			lsbTime = NarrowClock::getSlicedMicrosecondNow();
		}
		if (lsbTime < lastPollTime)
		{
			lastPollTime = lsbTime;
		}
	}
	if (g_gamepad)
	{
		g_gamepad->Release();
	}
	if (g_gameInput)
	{
		g_gameInput->Release();
	}
	return 0;
}

void FCabling::Exit()
{
	Cleanup();
}

void FCabling::Stop()
{
	Cleanup();
}

void FCabling::Cleanup()
{
	running = false;
}
