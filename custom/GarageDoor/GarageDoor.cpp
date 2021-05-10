#include "GarageDoor.h"
#include <iostream>

using namespace esphome;

GarageDoor::GarageDoor(uint8_t closed_pin, uint8_t opened_pin, uint8_t control_pin) :
  StateMachine(ST_MAX_STATES)
{
  this->closed_pin_ = new GPIOPin(closed_pin, INPUT);
  this->opened_pin_ = new GPIOPin(opened_pin, INPUT);
  this->control_pin_ = new GPIOPin(control_pin, OUTPUT);
}
	
// Open the door
void GarageDoor::Open()
{
  this->desiredState_ = ST_OPENED;

  BEGIN_TRANSITION_MAP                          // - Current State -
    TRANSITION_MAP_ENTRY (ST_MOVING)            // ST_UNKNOWN
    TRANSITION_MAP_ENTRY (EVENT_IGNORED)        // ST_MOVING
    TRANSITION_MAP_ENTRY (ST_OPENING)           // ST_LOCKED
    TRANSITION_MAP_ENTRY (ST_OPENING)           // ST_CLOSED
    TRANSITION_MAP_ENTRY (EVENT_IGNORED)        // ST_OPENING
    TRANSITION_MAP_ENTRY (ST_CLOSING)           // ST_STOPPED_OPENING
    TRANSITION_MAP_ENTRY (EVENT_IGNORED)        // ST_OPENED
    TRANSITION_MAP_ENTRY (ST_STOPPED_CLOSING)   // ST_CLOSING
    TRANSITION_MAP_ENTRY (ST_OPENING)           // ST_STOPPED_CLOSING
  END_TRANSITION_MAP()
}

// Close the door
void GarageDoor::Close()
{
  this->desiredState_ = ST_CLOSED;

  BEGIN_TRANSITION_MAP                          // - Current State -
    TRANSITION_MAP_ENTRY (ST_MOVING)            // ST_UNKNOWN
    TRANSITION_MAP_ENTRY (EVENT_IGNORED)        // ST_MOVING
    TRANSITION_MAP_ENTRY (EVENT_IGNORED)        // ST_LOCKED
    TRANSITION_MAP_ENTRY (EVENT_IGNORED)        // ST_CLOSED
    TRANSITION_MAP_ENTRY (ST_STOPPED_OPENING)   // ST_OPENING
    TRANSITION_MAP_ENTRY (ST_CLOSING)           // ST_STOPPED_OPENING
    TRANSITION_MAP_ENTRY (ST_CLOSING)           // ST_OPENED
    TRANSITION_MAP_ENTRY (EVENT_IGNORED)        // ST_CLOSING
    TRANSITION_MAP_ENTRY (ST_OPENING)           // ST_STOPPED_CLOSING
  END_TRANSITION_MAP()
}

// Stop the door while it is opening or closing
void GarageDoor::Stop()
{
  this->desiredState_ = ST_NONE;

  BEGIN_TRANSITION_MAP                          // - Current State -
    TRANSITION_MAP_ENTRY (EVENT_IGNORED)        // ST_UNKNOWN
    TRANSITION_MAP_ENTRY (ST_UNKNOWN)           // ST_MOVING
    TRANSITION_MAP_ENTRY (EVENT_IGNORED)        // ST_LOCKED
    TRANSITION_MAP_ENTRY (EVENT_IGNORED)        // ST_CLOSED
    TRANSITION_MAP_ENTRY (ST_STOPPED_OPENING)   // ST_OPENING
    TRANSITION_MAP_ENTRY (EVENT_IGNORED)        // ST_STOPPED_OPENING
    TRANSITION_MAP_ENTRY (EVENT_IGNORED)        // ST_OPENED
    TRANSITION_MAP_ENTRY (ST_STOPPED_CLOSING)   // ST_CLOSING
    TRANSITION_MAP_ENTRY (EVENT_IGNORED)        // ST_STOPPED_CLOSING
  END_TRANSITION_MAP()
}

// If not closed close the door and then "lock" it so that remotes cannot open it
void GarageDoor::Lock()
{
  this->desiredState_ = ST_LOCKED;

  BEGIN_TRANSITION_MAP                          // - Current State -
    TRANSITION_MAP_ENTRY (ST_MOVING)            // ST_UNKNOWN
    TRANSITION_MAP_ENTRY (EVENT_IGNORED)        // ST_MOVING
    TRANSITION_MAP_ENTRY (EVENT_IGNORED)        // ST_LOCKED
    TRANSITION_MAP_ENTRY (ST_LOCKED)            // ST_CLOSED
    TRANSITION_MAP_ENTRY (ST_STOPPED_OPENING)   // ST_OPENING
    TRANSITION_MAP_ENTRY (EVENT_IGNORED)        // ST_STOPPED_OPENING
    TRANSITION_MAP_ENTRY (EVENT_IGNORED)        // ST_OPENED
    TRANSITION_MAP_ENTRY (ST_STOPPED_CLOSING)   // ST_CLOSING
    TRANSITION_MAP_ENTRY (EVENT_IGNORED)        // ST_STOPPED_CLOSING
  END_TRANSITION_MAP()
}

// If the door is currently Locked set it to Closed so that remotes can open it
void GarageDoor::Unlock()
{
  this->desiredState_ = ST_CLOSED;

  BEGIN_TRANSITION_MAP                          // - Current State -
    TRANSITION_MAP_ENTRY (EVENT_IGNORED)        // ST_UNKNOWN
    TRANSITION_MAP_ENTRY (EVENT_IGNORED)        // ST_MOVING
    TRANSITION_MAP_ENTRY (ST_CLOSED)            // ST_LOCKED
    TRANSITION_MAP_ENTRY (EVENT_IGNORED)        // ST_CLOSED
    TRANSITION_MAP_ENTRY (EVENT_IGNORED)        // ST_OPENING
    TRANSITION_MAP_ENTRY (EVENT_IGNORED)        // ST_STOPPED_OPENING
    TRANSITION_MAP_ENTRY (EVENT_IGNORED)        // ST_OPENED
    TRANSITION_MAP_ENTRY (EVENT_IGNORED)        // ST_CLOSING
    TRANSITION_MAP_ENTRY (EVENT_IGNORED)        // ST_STOPPED_CLOSING
  END_TRANSITION_MAP()
}

// State machine starts here
STATE_DEFINE(GarageDoor, Unknown, NoEventData)
{
  // "Click" the button

  // TODO: Move this to initialize
  if (this->closed_pin_->digital_read())
  {
    // If the closed sensor reports closed transition to Closed via an internal event
    InternalEvent(ST_CLOSED);
  }
  else if (this->open_pin_->digital_read())
  {
    // If the open sensor reports open transition to Open via an internal event
    InternalEvent(ST_OPENED);
  }
}

// Start moving the door in an unknown direction (from an unknown state)
STATE_DEFINE(GarageDoor, Moving, NoEventData)
{
  // "Click" the button
}

STATE_DEFINE(GarageDoor, Locked, NoEventData)
{
}

// State machine sits here when the door is closed
STATE_DEFINE(GarageDoor, Closed, NoEventData)
{
  switch (this->desiredState_)
  {
    case ST_LOCKED:
      InternalEvent(ST_LOCKED);
      break;

    case ST_OPENED:
      InternalEvent(ST_OPENING);
      break;

    default:
      this->desiredState_ = ST_NONE;
      break;
  }
}

// Start opening the door
STATE_DEFINE(GarageDoor, Opening, NoEventData)
{
  // "Click" the button
}

// Stop opening the door
STATE_DEFINE(GarageDoor, StoppedOpening, NoEventData)
{
  // "Click" the button
}

// State machine sits here when the door is fully opened
STATE_DEFINE(GarageDoor, Opened, NoEventData)
{
  switch (this->desiredState_)
  {
    case ST_LOCKED:
      InternalEvent(ST_CLOSING);
      break;

    case ST_CLOSED:
      InternalEvent(ST_CLOSING);
      break;

    default:
      this->desiredState_ = ST_NONE;
      break;
  }
}

// Warn that the door is about to start closing
STATE_DEFINE(GarageDoor, CloseWarning, NoEventData)
{
  // BEEP, BEEP
}

// Start closing the door
STATE_DEFINE(GarageDoor, Closing, NoEventData)
{
  // "Click" the button
}

// Stop closing the door
STATE_DEFINE(GarageDoor, StoppedClosing, NoEventData)
{
  // "Click" the button
}
