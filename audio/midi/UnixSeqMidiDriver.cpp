/*
Copyright (C) 2004  The Pentagram Team

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

/*
  Unix MIDI sequencer
  Adapted from ScummVM's backends/midi/seq.cpp
*/

#include "pent_include.h"

#ifdef UNIX

#include "UnixSeqMidiDriver.h"
#include <fcntl.h>
#include <unistd.h>

#include "SettingManager.h"

const MidiDriver::MidiDriverDesc UnixSeqMidiDriver::desc =
		MidiDriver::MidiDriverDesc ("UnixSeqDevice", createInstance);

#define SEQ_MIDIPUTC 5
#define SEQ_DEVICE "/dev/sequencer"

UnixSeqMidiDriver::UnixSeqMidiDriver()
	: isOpen(false), device(0), deviceNum(0)
{
	/* see if the pentagram config file specifies an alternate midi device */
	SettingManager* settings = SettingManager::get_instance();
	if (!settings->get("unixseqdevice", devname))
		devname = SEQ_DEVICE;
}

int UnixSeqMidiDriver::open()
{
	if (isOpen) return 1;
	isOpen = true;
	device = 0;

	pout << "UnixSeqDevice: opening device: " << devname << std::endl;

	device = ::open(devname.c_str(), O_RDWR, 0);
	if (device < 0) {
		perr << "UnixSeqDevice: failed: " << strerror(errno);
		perr << std::endl;
		return device;
	}

	return 0;
}

void UnixSeqMidiDriver::close()
{
	::close(device);
	isOpen = false;
}

void UnixSeqMidiDriver::send(uint32 b) {
	unsigned char buf[256];
	int position = 0;

	switch (b & 0xF0) {
	case 0x80:
	case 0x90:
	case 0xA0:
	case 0xB0:
	case 0xE0:
		buf[position++] = SEQ_MIDIPUTC;
		buf[position++] = (unsigned char)b;
		buf[position++] = deviceNum;
		buf[position++] = 0;
		buf[position++] = SEQ_MIDIPUTC;
		buf[position++] = (unsigned char)((b >> 8) & 0x7F);
		buf[position++] = deviceNum;
		buf[position++] = 0;
		buf[position++] = SEQ_MIDIPUTC;
		buf[position++] = (unsigned char)((b >> 16) & 0x7F);
		buf[position++] = deviceNum;
		buf[position++] = 0;
		break;
	case 0xC0:
	case 0xD0:
		buf[position++] = SEQ_MIDIPUTC;
		buf[position++] = (unsigned char)b;
		buf[position++] = deviceNum;
		buf[position++] = 0;
		buf[position++] = SEQ_MIDIPUTC;
		buf[position++] = (unsigned char)((b >> 8) & 0x7F);
		buf[position++] = deviceNum;
		buf[position++] = 0;
		break;
	default:
		perr.printf("Unknown : %08x\n", (int)b);
		break;
	}

	::write(device, buf, position);
}

void UnixSeqMidiDriver::send_sysex(uint8 status,const uint8 *msg,uint16 length)
{
	if (length > 255) {
		perr << "UnixSeqMidiDriver: "
			 << "Cannot send SysEx block - data too large" << std::endl;
		return;
	}

	unsigned char buf [1024];
	int position = 0;
	const uint8 *chr = msg;

	buf[position++] = SEQ_MIDIPUTC;
	buf[position++] = status;
	buf[position++] = deviceNum;
	buf[position++] = 0;

	for (; length; --length) {
		buf[position++] = SEQ_MIDIPUTC;
		buf[position++] = *chr++;
		buf[position++] = deviceNum;
		buf[position++] = 0;
	}

	::write (device, buf, position);
}

#endif