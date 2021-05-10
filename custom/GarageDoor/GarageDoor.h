#ifndef _GARAGE_DOOR_H
#define _GARAGE_DOOR_H

#include "StateMachine.h"
#include "esphome.h"
#include "Arduino.h"

using namespace esphome;

class GarageDoor : public StateMachine
{
public:
  GarageDoor(uint8_t closed_pin, uint8_t open_pin, uint8_t control_pin);

	// External events taken by this state machine
  void Open();
  void Close();
  void Stop();
  void Lock();
  void Unlock();

private:
  const BYTE ST_NONE = 255;
  GPIOPin *closed_pin_;
  GPIOPin *opened_pin_;
  GPIOPin *control_pin_;

  // State enumeration order must match the order of state method entries in the state map.
  enum States
  {
    ST_UNKNOWN,
    ST_MOVING,
    ST_LOCKED,
    ST_CLOSED,
    ST_OPENING,
    ST_STOPPED_OPENING,
    ST_OPENED,
    ST_CLOSE_WARNING,
    ST_CLOSING,
    ST_STOPPED_CLOSING,
    ST_MAX_STATES
  };

  bool desiredState_ = ST_UNKNOWN;

  // Define the state machine state functions with event data type
  STATE_DECLARE(GarageDoor, Unknown,        NoEventData)
  STATE_DECLARE(GarageDoor, Moving,         NoEventData)
  STATE_DECLARE(GarageDoor, Locked,         NoEventData)
  STATE_DECLARE(GarageDoor, Closed,         NoEventData)
  STATE_DECLARE(GarageDoor, Opening,        NoEventData)
  STATE_DECLARE(GarageDoor, StoppedOpening, NoEventData)
  STATE_DECLARE(GarageDoor, Opened,         NoEventData)
  STATE_DECLARE(GarageDoor, CloseWarning,   NoEventData)
  STATE_DECLARE(GarageDoor, Closing,        NoEventData)
  STATE_DECLARE(GarageDoor, StoppedClosing, NoEventData)

  // State map to define state object order. Each state map entry defines a state object.
  BEGIN_STATE_MAP
    STATE_MAP_ENTRY(&Unknown)
    STATE_MAP_ENTRY(&Moving)
    STATE_MAP_ENTRY(&Locked)
    STATE_MAP_ENTRY(&Closed)
    STATE_MAP_ENTRY(&Opening)
    STATE_MAP_ENTRY(&StoppedOpening)
    STATE_MAP_ENTRY(&Opened)
    STATE_MAP_ENTRY(&CloseWarning)
    STATE_MAP_ENTRY(&Closing)
    STATE_MAP_ENTRY(&StoppedClosing)
  END_STATE_MAP	
};

#endif