
#pragma once

#include "CoreMinimal.h"
#include "ActionPatternParams.h"
#include "FActionPattern.h"
#include "Skeletonize.h"
#include "SkeletonTypes.h"

class CABLING_API FConservedInputPatternMatcher
	{
		InputStreamKey MyStream; //and may god have mercy on my soul.
		friend class FArtilleryBusyWorker;
	public:
		FConservedInputPatternMatcher(InputStreamKey StreamToLink)
		{
			AllPatternBinds = TMap<ArtIPMKey, TSharedPtr<TSet<FActionPatternParams>>>();
			AllPatternsByName = TMap<ArtIPMKey, IPM::CanonPattern>();
			MyStream=StreamToLink;
		}

		//there's a bunch of reasons we use string_view here, but mostly, it's because we can make them constexprs!
		//so this is... uh... pretty fast!
		TMap<ArtIPMKey, TSharedPtr<TSet<FActionPatternParams>>> AllPatternBinds;
		//broadly, at the moment, there is ONE pattern matcher running


		//this array is never made smaller.
		//there should only ever be about 10 patterns max,
		//and it's literally more expensive to remove them.
		//As a result, we track what's actually live via the binds
		//and this array is just lazy accumulative. it means we don't ever allocate a key array for TMap.
		//has some other advantages, as well.
		TArray<ArtIPMKey> Names;

		//same with this set, actually. patterns are stateless, and few. it's inefficient to destroy them.
		//instead we check binds.
		TMap<ArtIPMKey, IPM::CanonPattern> AllPatternsByName;


		//***********************************************************
		//
		// THIS IS THE IMPORTANT FUNCTION.
		//
		// ***********************************************************
		//
		// This makes things run. it doesn't correctly handle really anything, that's the busyworker's job
		// There likely will only be 12 or 18 FCMs runnin EVER because I think we'll want to treat each AI
		//faction pretty much as a single FCM except for a few bosses.
		//
		//hard to say. we might need to revisit this if the FCMs prove too heavy as full actor components.

		void runOneFrameWithSideEffects(bool isResim_Unimplemented,
		                                //USED TO DEFINE HOW TO HIDE LATENCY BY TRIMMING LEAD-IN FRAMES OF AN ARTILLERYGUN
		                                uint32_t leftTrimFrames,
		                                //USED TO DEFINE HOW TO SHORTEN ARTILLERYGUNS BY SHORTENING TRAILING or INFIX DELAYS, SUCH AS DELAYED EXPLOSIONS, TRAJECTORIES, OR SPAWNS, TO HIDE LATENCY.
		                                uint32_t rightTrimFrames,
		                                uint64_t InputCycleNumber,
		                                TArray<TPair<ArtilleryTime, FGunKey>>&
		                                IN_PARAM_REF_TRIPLEBUFFER_LIFECYLEMANAGED
		                                //frame's a misnomer, actually.
		)
		{
			if (!isResim_Unimplemented)
			{
				UE_LOG(LogTemp, Display, TEXT("Still no resim, actually."));
			}

			//while the pattern matcher lives in the stream, the stream instance is not guaranteed to persist
			//In fact, it may get "swapped" and so we actually indirect through the ECS, grab the current stream whatever it is
			//then pin it. at this point, we can be sure that we hold A STREAM that DOES exist.
			//TODO: settle on a coherent error handling strategy here.
			auto Stream = ECS->GetStream(MyStream);
			
			
			//the lack of reference (&) here causes a _copy of the shared pointer._ This is not accidental.
			for (auto SetTuple : AllPatternBinds)
			{
				if (SetTuple.Value->Num() > 0)
				{
					IPM::CanonPattern currentPattern = AllPatternsByName[SetTuple.Key];
					FActionBitMask Union;
					auto currentSet = SetTuple.Value.Get();
					//TODO: remove and replace with a version that uses all bits set.
					//lot of refactoring to do that. let's get this working first.
					for (FActionPatternParams& Elem : *currentSet)
					{
						//todo: replace with toFlat(). ffs.
						Union.buttons |= Elem.ToSeek.buttons;
						Union.events |= Elem.ToSeek.events;
					}
					auto result = currentPattern-> runPattern(InputCycleNumber, Union, Stream);
					if (result)
					{
						for (FActionPatternParams& Elem : *currentSet)
						{
							if (Elem.ToSeek.getFlat() != 0)
							{
								if ((Elem.ToSeek.getFlat() & result) == Elem.ToSeek.getFlat())
								{
									auto time = Stream->peek(InputCycleNumber)->SentAt;
									//THIS IS NOT SUPER SAFE. HAHAHAH. YAY.
									IN_PARAM_REF_TRIPLEBUFFER_LIFECYLEMANAGED.Add(TPair<ArtilleryTime, FGunKey>(
											time,
											Elem.ToFire)
									);
								}
							}
							else
							{
								continue;
							}
						}
					}
				}
			}
			//Stickflick is handled here but continuous movement is handled elsewhere in artillery busy worker.
		};

	protected:
	};