#pragma once

class FControllerState {
public:
	// this needs to be defactored but it isn't totally obvious how it SHOULD be set.
	//this might be a good place to refactor the packing shim for, so that we can inject the type elegantly.
	uint64_t controller_arr;
	
	FControllerState() {
		clear();
	}

	void clear() {
		controller_arr = 0;
	}

	FString ToString() const {
		return FString::Printf(TEXT("{ %llx }"), controller_arr);
	}
};