#pragma once
#include "link.h"

// The radio half of the peer-to-peer link, kept apart from link.cpp on purpose:
// link.cpp is pure protocol and compiles anywhere, including the desktop
// emulator, so it can be tested. Everything that touches ESP-NOW lives here and
// is stubbed out for the emulator, which has no radio at all.
//
// UNTESTED. Nothing in this file has run on hardware -- it needs two boards.
// Treat the first bring-up as debugging, not as verification.

bool linkNowBegin(Link *l);   // brings up WiFi + ESP-NOW for a battle; false if either fails

// The same radio for any protocol (trading uses it). Only packets whose first
// byte -- the message type -- lies in [typeLo, typeHi] are delivered, so a
// board trading and a board battling on the same channel never see each
// other's packets, and never lock onto each other either.
//
// lockPeer: lock onto the first peer heard and unicast to it from then on (the
// battle does this). Trading does NOT: a pending trade has to be able to hear
// its own peer come back even after a stranger spoke first, so it broadcasts
// and lets the protocol filter by MAC. Its resends cover broadcast's lack of
// an acknowledgement.
typedef void (*LinkNowRx)(void *ctx, const uint8_t *buf, uint8_t len);
bool linkNowOpen(LinkNowRx rx, void *ctx, uint8_t typeLo, uint8_t typeHi, bool lockPeer);
void linkNowSend(void *ctx, const uint8_t *buf, uint8_t len);   // a transport, for Trade::send
void linkNowEnd();            // frees the radio again -- it costs real current
bool linkNowUp();

// Hands received packets to the Link, on the MAIN loop. The ESP-NOW callback
// runs on the WiFi task and must not touch protocol state directly, so it only
// parks bytes in a ring that this drains. Call once a frame, alongside tick().
void linkNowPoll();

// The locked peer's real MAC, or nullptr before one has been heard from.
// Deliberately not threaded through Link itself -- link.cpp is pure protocol
// with no radio knowledge, which is the whole reason it can be tested without
// a board, and a MAC address is meaningless outside a real ESP-NOW session.
const uint8_t *linkNowPeerMac();

// Diagnostics for the first bring-up, since none of this has ever run:
// how many packets arrived, how many were sent, how many the radio reported
// as undelivered, and how many were dropped for coming from another device.
struct LinkNowStats {
  uint32_t rx, tx, txFail, foreign, overflow;
};
const LinkNowStats &linkNowStats();
