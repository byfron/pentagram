/*
Copyright (C) 2002-2003 The Pentagram team

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

#include "pent_include.h"

#include "GUIApp.h"

#include <SDL.h>

//!! a lot of these includes are just for some hacks... clean up sometime
#include "Kernel.h"
#include "FileSystem.h"
#include "Configuration.h"
#include "ObjectManager.h"

#include "RenderSurface.h"
#include "Texture.h"
#include "PaletteManager.h"
#include "GameData.h"
#include "World.h"

#include "U8Save.h"
#include "SavegameWriter.h"
#include "Savegame.h"

#include "Gump.h"
#include "DesktopGump.h"
#include "ConsoleGump.h"
#include "GameMapGump.h"
#include "BarkGump.h"

#include "Actor.h"
#include "ActorAnimProcess.h"
#include "Font.h"
#include "FontShapeFlex.h"
#include "u8intrinsics.h"
#include "Egg.h"
#include "CurrentMap.h"
#include "UCList.h"
#include "LoopScript.h"

#include "EggHatcherProcess.h" // for a hack
#include "UCProcess.h" // more hacking
#include "GumpNotifyProcess.h" // guess
#include "DelayProcess.h"
#include "GravityProcess.h"
#include "MissileProcess.h"
#include "TeleportToEggProcess.h"

#include "DisasmProcess.h"
#include "CompileProcess.h"

#if defined(WIN32) && defined(COLOURLESS_WANTS_A_DATA_FREE_PENATGRAM)
#include <windows.h>
#include "resource.h"
#endif

#include "XFormBlend.h"

#include "MusicProcess.h"
#include "MusicFlex.h"

#include "MidiDriver.h"
#ifdef WIN32
#include "WindowsMidiDriver.h"
#endif
#ifdef MACOSX
#include "CoreAudioMidiDriver.h"
#endif
#ifdef USE_FMOPL_MIDI
#include <SDL_mixer.h>
#include "FMOplMidiDriver.h"
#endif

using std::string;

DEFINE_RUNTIME_CLASSTYPE_CODE(GUIApp,CoreApp);

GUIApp::GUIApp(int argc, const char* const* argv)
	: CoreApp(argc, argv), ucmachine(0), screen(0),
	  palettemanager(0), gamedata(0), world(0), desktopGump(0),
	  consoleGump(0), gameMapGump(0),
	  runGraphicSysInit(false), runSDLInit(false),
	  frameSkip(false), frameLimit(true), interpolate(true),
	  animationRate(100), avatarInStasis(false), paintEditorItems(false),
	  painting(false), dragging(DRAG_NOT), timeOffset(0), midi_driver(0),
	  midi_volume(255)
{
	application = this;

	for (int i = 0; i < NUM_MOUSEBUTTONS+1; ++i) {
		mouseDownGump[i] = 0;
		lastMouseDown[i] = 0;
		mouseState[i] = MBS_HANDLED;
	}
}

GUIApp::~GUIApp()
{
	FORGET_OBJECT(objectmanager);
	deinit_midi();
	FORGET_OBJECT(ucmachine);
	FORGET_OBJECT(palettemanager);
	FORGET_OBJECT(gamedata);
	FORGET_OBJECT(world);
	FORGET_OBJECT(ucmachine);
}

void GUIApp::startup()
{
	// Set the console to auto paint, till we have finished initing
	con.SetAutoPaint(conAutoPaint);

	// parent's startup first
	CoreApp::startup();

	//!! move this elsewhere
	kernel->addProcessLoader("DelayProcess",
							 ProcessLoader<DelayProcess>::load);
	kernel->addProcessLoader("GravityProcess",
							 ProcessLoader<GravityProcess>::load);
	kernel->addProcessLoader("PaletteFaderProcess",
							 ProcessLoader<PaletteFaderProcess>::load);
	kernel->addProcessLoader("TeleportToEggProcess",
							 ProcessLoader<TeleportToEggProcess>::load);
	kernel->addProcessLoader("ActorAnimProcess",
							 ProcessLoader<ActorAnimProcess>::load);
	kernel->addProcessLoader("SpriteProcess",
							 ProcessLoader<SpriteProcess>::load);
	kernel->addProcessLoader("MissileProcess",
							 ProcessLoader<MissileProcess>::load);
	kernel->addProcessLoader("CameraProcess",
							 ProcessLoader<CameraProcess>::load);
	kernel->addProcessLoader("MusicProcess",
							 ProcessLoader<MusicProcess>::load);
	kernel->addProcessLoader("EggHatcherProcess",
							 ProcessLoader<EggHatcherProcess>::load);
	kernel->addProcessLoader("UCProcess",
							 ProcessLoader<UCProcess>::load);
	kernel->addProcessLoader("GumpNotifyProcess",
							 ProcessLoader<GumpNotifyProcess>::load);

	pout << "Create ObjectManager" << std::endl;
	objectmanager = new ObjectManager();

	pout << "Create UCMachine" << std::endl;
	ucmachine = new UCMachine(U8Intrinsics);

	GraphicSysInit();

	U8Playground();

	// This Must be AFTER U8Playground() cause it might need gamedata
	init_midi();

	// Unset the console auto paint, since we have finished initing
	con.SetAutoPaint(0);
}

#ifdef USE_FMOPL_MIDI
void SDL_mixer_music_hook(void *udata, Uint8 *stream, int len)
{
	GUIApp *app = GUIApp::get_instance();
	MidiDriver *drv = static_cast<MidiDriver*>(udata);
	drv->produceSamples(reinterpret_cast<sint16*>(stream), len);
}
#endif

#define TRY_MIDI_DRIVER(MidiDriver,rate,stereo) \
		if (!midi_driver) {	\
			pout << "Trying `" << #MidiDriver << "'" << std::endl; \
			midi_driver = new MidiDriver; \
			if (midi_driver->initMidiDriver(rate,stereo)) { \
				delete midi_driver; \
				midi_driver = 0; \
			} \
			else \
				pout << "Using `" << #MidiDriver << "'" << std::endl; \
		}

void GUIApp::init_midi()
{
	int sample_rate = 22050;	// Good enough
	int channels = 1;			// Only mono for now
	Uint16 format = AUDIO_S16SYS;

	pout << "Initializing Midi" << std::endl;

#if 0
	// Only the FMOPL Midi driver 'requires' SDL_mixer, and I don't 
	// want to force it on everyone at the moment.
#ifdef USE_FMOPL_MIDI
	SDL_InitSubSystem(SDL_INIT_AUDIO);
	Mix_OpenAudio(sample_rate, format, channels, 4096);
	Mix_QuerySpec(&sample_rate, &format, &channels);
#endif
	// Create the driver
#ifdef WIN32
	TRY_MIDI_DRIVER(WindowsMidiDriver,sample_rate,channels==2);
#endif
#ifdef MACOSX
	TRY_MIDI_DRIVER(CoreAudioMidiDriver,sample_rate,channels==2);
#endif
#ifdef USE_FMOPL_MIDI
	TRY_MIDI_DRIVER(FMOplMidiDriver,sample_rate,channels==2);
#endif

#else
	midi_driver = 0; // silence :-)
#endif

	// If the driver is a 'sample' producer we need to hook it to SDL_mixer
	if (midi_driver)
	{
#ifdef USE_FMOPL_MIDI
		if (midi_driver->isSampleProducer())
			Mix_HookMusic(SDL_mixer_music_hook, midi_driver);
#endif

		midi_driver->setGlobalVolume(midi_volume);
	}

	// Create the Music Process
	Process *mp = new MusicProcess(midi_driver);
	kernel->addProcess(mp);

}

void GUIApp::deinit_midi()
{
#ifdef USE_FMOPL_MIDI
	Mix_HookMusic(0,0);
	Mix_CloseAudio();
#endif

	if (midi_driver)  midi_driver->destroyMidiDriver();

	delete midi_driver;
	midi_driver = 0;
}

void GUIApp::DeclareArgs()
{
	// parent's arguments first
	CoreApp::DeclareArgs();

	// anything else?
}

void GUIApp::run()
{
	isRunning = true;

	sint32 next_ticks = SDL_GetTicks()*3;	// Next time is right now!
	
	// Ok, the theory is that if this is set to true then we must do a repaint
	// At the moment only it's ignored
	bool repaint;

	SDL_Event event;
	while (isRunning) {
		inBetweenFrame = true;	// Will get set false if it's not an inBetweenFrame

		if (!frameLimit) {
			repaint = false;
			
			if (kernel->runProcesses(framenum)) repaint = true;
			desktopGump->Run(framenum);
			framenum++;
			inBetweenFrame = false;
			next_ticks = animationRate + SDL_GetTicks()*3;
			lerpFactor = 256;
		}
		else 
		{
			sint32 ticks = SDL_GetTicks()*3;
			sint32 diff = next_ticks - ticks;
			repaint = false;

			while (diff < 0) {
				next_ticks += animationRate;
				if (kernel->runProcesses(framenum)) repaint = true;
				desktopGump->Run(framenum);
				framenum++;
#if 0
				perr << "--------------------------------------" << std::endl;
				perr << "NEW FRAME" << std::endl;
				perr << "--------------------------------------" << std::endl;
#endif
				inBetweenFrame = false;

				ticks = SDL_GetTicks()*3;

				// If frame skipping is off, we will only recalc next
				// ticks IF the frames are taking up 'way' too much time. 
				if (!frameSkip && diff <= -animationRate*2) next_ticks = animationRate + ticks;

				diff = next_ticks - ticks;
				if (!frameSkip) break;
			}

			// Calculate the lerp_factor
			lerpFactor = ((animationRate-diff)*256)/animationRate;
			//pout << "lerpFactor: " << lerpFactor << " framenum: " << framenum << std::endl;
			if (!interpolate || lerpFactor > 256) lerpFactor = 256;
		}


		repaint = true;

		// get & handle all events in queue
		while (isRunning && SDL_PollEvent(&event)) {
			handleEvent(event);
		}
		handleDelayedEvents();

		// Paint Screen
		paint();

		// Do a delay
		SDL_Delay(5);
	}
}

void GUIApp::U8Playground()
{
	inBetweenFrame = 0;
	lerpFactor = 256;

	// Load palette
	pout << "Load Palette" << std::endl;
	palettemanager = new PaletteManager(screen);
	IDataSource *pf = filesystem->ReadFile("@u8/static/u8pal.pal");
	if (!pf) {
		perr << "Unable to load static/u8pal.pal. Exiting" << std::endl;
		std::exit(-1);
	}
	pf->seek(4); // seek past header

	IBufferDataSource xfds(U8XFormPal,1024);
	palettemanager->load(PaletteManager::Pal_Game, *pf, xfds);
	delete pf;

	pout << "Load GameData" << std::endl;
	gamedata = new GameData();
	gamedata->loadU8Data();

	// Initialize world
	pout << "Initialize World" << std::endl;
	world = new World();

	IDataSource *saveds = filesystem->ReadFile("@u8/savegame/u8save.000");
	U8Save *u8save = new U8Save(saveds);

	IDataSource *nfd = u8save->get_datasource("NONFIXED.DAT");
	if (!nfd) {
		perr << "Unable to load savegame/u8save.000/NONFIXED.DAT. Exiting" << std::endl;
		std::exit(-1);
	}
	world->initMaps();
	world->loadNonFixed(nfd);

	IDataSource *icd = u8save->get_datasource("ITEMCACH.DAT");
	if (!icd) {
		perr << "Unable to load savegame/u8save.000/ITEMCACH.DAT. Exiting" << std::endl;
		std::exit(-1);
	}
	IDataSource *npcd = u8save->get_datasource("NPCDATA.DAT");
	if (!npcd) {
		perr << "Unable to load savegame/u8save.000/NPCDATA.DAT. Exiting" << std::endl;
		std::exit(-1);
	}

	world->loadItemCachNPCData(icd, npcd);
	delete u8save;

	Actor* av = world->getNPC(1);
//	av->teleport(40, 16240, 15240, 64); // central Tenebrae
//	av->teleport(3, 11391, 1727, 64); // docks, near gate
//	av->teleport(39, 16240, 15240, 64); // West Tenebrae
//	av->teleport(41, 12000, 15000, 64); // East Tenebrae
//	av->teleport(8, 14462, 15178, 48); // before entrance to Mythran's house
//	av->teleport(40, 13102,9474,48); // entrance to Mordea's throne room
//	av->teleport(54, 14783,5759,8); // shrine of the Ancient Ones; Hanoi

	if (av)
		world->switchMap(av->getMapNum());
	else
		world->switchMap(3);

	// Create GameMapGump
	Pentagram::Rect dims;
	screen->GetSurfaceDims(dims);
	
	pout << "Create GameMapGump" << std::endl;
	gameMapGump = new GameMapGump(0, 0, dims.w, dims.h);
	gameMapGump->InitGump();
	desktopGump->AddChild(gameMapGump);

	pout << "Create Camera" << std::endl;
	CameraProcess::SetCameraProcess(new CameraProcess(1)); // Follow Avatar

	pout << "Paint Inital display" << std::endl;
	consoleGump->HideConsole();
	paint();
}

// conAutoPaint hackery
void GUIApp::conAutoPaint(void)
{
	GUIApp *app = GUIApp::get_instance();
	if (app && !app->isPainting()) app->paint();
}

// Paint the screen
void GUIApp::paint()
{
	if(!runGraphicSysInit) // need to worry if the graphics system has been started. Need nicer way.
		return;

	painting = true;

	// Begin painting
	screen->BeginPainting();

	// We need to get the dims
	Pentagram::Rect dims;
	screen->GetSurfaceDims(dims);

	long before_gumps = SDL_GetTicks();
	desktopGump->Paint(screen, lerpFactor);
	long after_gumps = SDL_GetTicks();

	static long prev = 0;
	long now = SDL_GetTicks();
	long diff = now - prev;
	if (diff == 0) diff = 1;
	prev = now;

	char buf[256];
	snprintf(buf, 255, "Rendering time %li ms %li FPS - Paint Gumps %li ms - t %02d:%02d gh %i ", diff, 1000/diff, after_gumps-before_gumps, I_getTimeInMinutes(0,0), I_getTimeInSeconds(0,0)%60, I_getTimeInGameHours(0,0));
	screen->PrintTextFixed(con.GetConFont(), buf, 8, dims.h-16);

	// End painting
	screen->EndPainting();

	painting = false;
}

void GUIApp::GraphicSysInit()
{
	// if we've already done this...
	if(runGraphicSysInit) return;
	//else...

	// Set Screen Resolution
	pout << "Set Video Mode" << std::endl;

	std::string fullscreen;
	config->value("config/video/fullscreen", fullscreen, "no");
	if (fullscreen!="yes") fullscreen="no";
	int width, height, bpp;

	config->value("config/video/width", width, 640);
	config->value("config/video/height", height, 480);
	config->value("config/video/bpp", bpp, 32);

	// store values in user's config file
	config->set("config/video/width", width);
	config->set("config/video/height", height);
	config->set("config/video/bpp", bpp);
	config->set("config/video/fullscreen", fullscreen);

	screen = RenderSurface::SetVideoMode(width, height, bpp,
										 fullscreen=="yes", false);

	if (!screen)
	{
		perr << "Unable to set video mode. Exiting" << std::endl;
		std::exit(-1);
	}

	pout << "Create Desktop" << std::endl;
	desktopGump = new DesktopGump(0,0, width, height);
	desktopGump->InitGump();
	desktopGump->MakeFocus();

	pout << "Create Graphics Console" << std::endl;
	consoleGump = new ConsoleGump(0, 0, width, height);
	consoleGump->InitGump();
	desktopGump->AddChild(consoleGump);

	LoadConsoleFont();

	// Create desktop
	Pentagram::Rect dims;
	screen->GetSurfaceDims(dims);

	runGraphicSysInit=true;

	// Do initial console paint
	paint();
}

void GUIApp::LoadConsoleFont()
{
#if defined(WIN32) && defined(COLOURLESS_WANTS_A_DATA_FREE_PENATGRAM)
	HRSRC res = FindResource(NULL,  MAKEINTRESOURCE(IDR_FIXED_FONT_TGA), RT_RCDATA);
	if (res) filesystem->MountFileInMemory("@data/fixedfont.tga", static_cast<uint8*>(LockResource(LoadResource(NULL, res))), SizeofResource(NULL, res));

	res = FindResource(NULL, MAKEINTRESOURCE(IDR_FIXED_FONT_CFG), RT_RCDATA);
	if (res) filesystem->MountFileInMemory("@data/fixedfont.cfg", static_cast<uint8*>(LockResource(LoadResource(NULL, res))), SizeofResource(NULL, res));
#endif

	std::string data;
	std::string confontfile;
	std::string confontcfg("@data/fixedfont.cfg");

	pout << "Searching for alternate console font... ";
	config->value("config/general/console-font", data, "");
	if (data != "")
	{
		confontcfg = data;
		pout << "Found." << std::endl;
	}
	else
		pout << "Not Found." << std::endl;

	// try to load the file
	pout << "Loading console font config: " << confontcfg << "... ";
	if(config->readConfigFile(confontcfg, "font", true))
		pout << "Ok" << std::endl;
	else
		pout << "Failed" << std::endl;

	// search for the path to the font...
	config->value("font/path", confontfile, "");

	if(confontfile=="")
	{
		pout << "Error: Console font path not found! Unable to continue. Exiting." << std::endl;
		std::exit(-1);
	}

	// Load confont
	pout << "Loading Confont: " << confontfile << std::endl;
	IDataSource *cf = filesystem->ReadFile(confontfile.c_str());
	Texture *confont;
	if (cf) confont = Texture::Create(*cf, confontfile.c_str());
	else confont = 0;
	if (!confont)
	{
		perr << "Unable to load " << confontfile << ". Exiting" << std::endl;
		std::exit(-1);
	}
	delete cf;

	con.SetConFont(confont);
}

//
// Hacks are us!
//

class AvatarMoverProcess : public Process
{
	int dx, dy, dz, dir;
public:
	static  AvatarMoverProcess	*amp[6];
	static	bool				clipping;
	static	bool				quarter;
	static	bool				hitting;

	AvatarMoverProcess(int x, int y, int z, int _dir) : Process(1), dx(x), dy(y), dz(z), dir(_dir)
	{
		if (amp[dir]) amp[dir]->terminate();
		amp[dir] = this;
	}

	virtual bool run(const uint32 framenum)
	{
		if (GUIApp::get_instance()->isAvatarInStasis()) 
		{
			terminate();
			return false;
		}
		Actor* avatar = World::get_instance()->getNPC(1);
		sint32 x,y,z;
		avatar->getLocation(x,y,z);
		sint32 ixd,iyd,izd;
		avatar->getFootpad(ixd, iyd, izd);
		ixd *= 32; iyd *= 32; izd *= 8; //!! constants

		CurrentMap* cm = World::get_instance()->getCurrentMap();

		sint32 dx = this->dx;
		sint32 dy = this->dy;
		sint32 dz = this->dz;

		for (int j = 0; j < 3; j++)
		{
			dx = this->dx;
			dy = this->dy;
			dz = this->dz;

			if (j == 1) dx = 0;
			else if (j == 2) dy = 0;

			if (quarter) 
			{
				dx /= 4;
				dy /= 4;
				dz /= 4;
			}

			bool ok = false;

			while (dx || dy || dz) {

				if (!clipping || cm->isValidPosition(x+dx,y+dy,z+dz,ixd,iyd,izd,1,0,0))
				{
					if (clipping && !dz)
					{
						if (cm->isValidPosition(x+dx,y+dy,z-8,ixd,iyd,izd,1,0,0) &&
								!cm->isValidPosition(x,y,z-8,ixd,iyd,izd,1,0,0))
						{
							dz = -8;
						}
						else if (cm->isValidPosition(x+dx,y+dy,z-16,ixd,iyd,izd,1,0,0) &&
								!cm->isValidPosition(x,y,z-16,ixd,iyd,izd,1,0,0))
						{
							dz = -16;
						}
						else if (cm->isValidPosition(x+dx,y+dy,z-24,ixd,iyd,izd,1,0,0) &&
								!cm->isValidPosition(x,y,z-24,ixd,iyd,izd,1,0,0))
						{
							dz = -24;
						}
						else if (cm->isValidPosition(x+dx,y+dy,z-32,ixd,iyd,izd,1,0,0) &&
								!cm->isValidPosition(x,y,z-32,ixd,iyd,izd,1,0,0))
						{
							dz = -32;
						}
					}					
					ok = true;
					break;
				}
				else if (cm->isValidPosition(x+dx,y+dy,z+dz+8,ixd,iyd,izd,1,0,0))
				{
					dz+=8;
					ok = true;
					break;
				}
				dx/=2;
				dy/=2;
				dz/=2;
			}
			if (ok) break;
		}

#if 0
		if (hitting) 
		{
			std::list<CurrentMap::SweepItem> items;

			sint32 start[3] = { x, y, z };
			sint32 end[3] = { x+dx, y+dy, z+dz };
			sint32 dims[3] = { ixd, iyd, izd };
			uint16 skip = 0;

			bool hit = cm->sweepTest(start,end,dims,1,false,&items);

			if (hit)
			{
				std::list<CurrentMap::SweepItem>::iterator it;

				for (it = items.begin(); it != items.end(); it++)
				{
					Item *item = World::get_instance()->getItem((*it).item);

					uint16 proc_gothit = 0, proc_rel = 0;

					// If hitting at start, we should have already 
					// called gotHit
					if ((*it).hit_time > 0) 
					{
						item->setExtFlag(Item::EXT_HIGHLIGHT);
						proc_gothit = item->callUsecodeEvent_gotHit(1,0);
					}

					// If not hitting at end, we will need to call release
					if ((*it).end_time < 0x4000)
					{
						item->clearExtFlag(Item::EXT_HIGHLIGHT);
						proc_rel = item->callUsecodeEvent_release();
					}

					// We want to make sure that release is called AFTER gotHit.
					if (proc_rel && proc_gothit)
					{
						Process *p = Kernel::get_instance()->getProcess(proc_rel);
						p->waitFor(proc_gothit);
					}
				}
			}
		}
#endif

		// Yes, i know, not entirely correct
		avatar->collideMove(x+dx,y+dy,z+dz, false, true);
		return true;
	}

	virtual void terminate()
	{
		Process::terminate();
		amp[dir] = 0;
	}
};

AvatarMoverProcess	*AvatarMoverProcess::amp[6] = { 0, 0, 0, 0, 0, 0 };
bool AvatarMoverProcess::clipping = false;
bool AvatarMoverProcess::quarter = false;
bool AvatarMoverProcess::hitting = false;

bool hitting() { return AvatarMoverProcess::hitting; }

static int volumelevel = 255;

void GUIApp::handleEvent(const SDL_Event& event)
{
	uint32 now = SDL_GetTicks();

	//!! TODO: handle mouse handedness. (swap left/right mouse buttons here)

	switch (event.type) {
	case SDL_QUIT:
	{
		isRunning = false;
	}
	break;

	case SDL_ACTIVEEVENT:
	{
		// pause when lost focus?
	}
	break;


	// most of these events will probably be passed to a gump manager,
	// since almost all (all?) user input will be handled by a gump

	case SDL_MOUSEBUTTONDOWN:
	{
		int button = event.button.button;
		int mx = event.button.x;
		int my = event.button.y;
		assert(button >= 0 && button <= NUM_MOUSEBUTTONS);

		Gump *mousedowngump = desktopGump->OnMouseDown(button, mx, my);
		if (mousedowngump)
			mouseDownGump[button] = mousedowngump->getObjId();
		else
			mouseDownGump[button] = 0;

		mouseDownX[button] = mx;
		mouseDownY[button] = my;
		mouseState[button] |= MBS_DOWN;
		mouseState[button] &= ~MBS_HANDLED;

		if (now - lastMouseDown[button] < 200) { //!! constant
			Gump* gump = getGump(mouseDownGump[button]);
			if (gump)
				gump->OnMouseDouble(button, mx, my);
			mouseState[button] |= MBS_HANDLED;
		}
		lastMouseDown[button] = now;
	}
	break;

	case SDL_MOUSEBUTTONUP:
	{
		int button = event.button.button;
		int mx = event.button.x;
		int my = event.button.y;
		assert(button >= 0 && button <= NUM_MOUSEBUTTONS);

		mouseState[button] &= ~MBS_DOWN;

		if (button == BUTTON_LEFT && dragging != DRAG_NOT) {
			// stop dragging

			perr << "Dropping object " << dragging_objid << std::endl;

			Gump *gump = getGump(dragging_objid);
			Item *item= World::get_instance()->getItem(dragging_objid);
			// for a Gump: notify parent
			if (gump) {
				Gump *parent = gump->GetParent();
				assert(parent); // can't drag root gump
				parent->StopDraggingChild(gump);
			} else 
			// for an item: notify gumps
			if (item) {
				if (dragging != DRAG_INVALID) {
					Gump *startgump = getGump(dragging_item_startgump);
					assert(startgump); // can't have disappeared
					bool moved = (dragging == DRAG_OK);
					startgump->StopDraggingItem(item, moved);
				}

				if (dragging == DRAG_OK) {
					gump = desktopGump->FindGump(mx, my);
					int gx = mx, gy = my;
					gump->ScreenSpaceToGump(gx, gy);
					gump->DropItem(item,gx,gy);
				}
			} else {
				assert(dragging == DRAG_INVALID);
			}


			dragging = DRAG_NOT;
			break;
		}

		Gump* gump = getGump(mouseDownGump[button]);
		if (gump)
			gump->OnMouseUp(button, mx, my);
	}
	break;

	case SDL_MOUSEMOTION:
	{
		int mx = event.button.x;
		int my = event.button.y;
		if (dragging == DRAG_NOT) {
			if (mouseState[BUTTON_LEFT] & MBS_DOWN) {
				int startx = mouseDownX[BUTTON_LEFT];
				int starty = mouseDownY[BUTTON_LEFT];
				if (abs(startx - mx) > 2 ||
					abs(starty - my) > 2)
				{
					dragging_objid = desktopGump->TraceObjID(startx, starty);
					perr << "Dragging object " << dragging_objid << std::endl;

					dragging = DRAG_OK;

					//!! need to pause the kernel

					Gump *gump = getGump(dragging_objid);
					Item *item= World::get_instance()->getItem(dragging_objid);

					// for a Gump, notify the Gump's parent that we started
					// dragging:
					if (gump) {
						Gump *parent = gump->GetParent();
						assert(parent); // can't drag root gump
						int px = startx, py = starty;
						parent->ScreenSpaceToGump(px, py);
						parent->StartDraggingChild(gump, px, py);
					} else
					// for an Item, notify the gump the item is in that we 
					// started dragging
					if (item) {
						// find gump item was in
						gump = desktopGump->FindGump(startx, starty);
						int gx = startx, gy = starty;
						gump->ScreenSpaceToGump(gx, gy);
						bool ok = !isAvatarInStasis() &&
							gump->StartDraggingItem(item,gx,gy);
						if (!ok) {
							dragging = DRAG_INVALID;
						} else {
							dragging = DRAG_OK;

							// this is the gump that'll get StopDraggingItem
							dragging_item_startgump = gump->getObjId();

							// this is the gump the item is currently over
							dragging_item_lastgump = gump->getObjId();
						}
					} else {
						dragging = DRAG_INVALID;
					}

					mouseState[BUTTON_LEFT] |= MBS_HANDLED;
				}
			}
		}

		if (dragging == DRAG_OK || dragging == DRAG_TEMPFAIL) {
			Gump* gump = getGump(dragging_objid);
			Item *item= World::get_instance()->getItem(dragging_objid);

			// for a gump, notify Gump's parent that it was dragged
			if (gump) {
				Gump *parent = gump->GetParent();
				assert(parent); // can't drag root gump
				int px = mx, py = my;
				parent->ScreenSpaceToGump(px, py);
				parent->DraggingChild(gump, px, py);
			} else
			// for an item, notify the gump it's on
			if (item) {
				gump = desktopGump->FindGump(mx, my);
				assert(gump);

				if (gump->getObjId() != dragging_item_lastgump) {
					// item switched gump, so notify previous gump item left
					Gump *last = getGump(dragging_item_lastgump);
					if (last) last->DraggingItemLeftGump(item);
				}
				dragging_item_lastgump = gump->getObjId();
				int gx = mx, gy = my;
				gump->ScreenSpaceToGump(gx, gy);
				bool ok = gump->DraggingItem(item,gx,gy);
				if (!ok) {
					dragging = DRAG_TEMPFAIL;
				} else {
					dragging = DRAG_OK;
				}
			} else {
				CANT_HAPPEN();
			}
		}
	}
	break;

	case SDL_KEYDOWN:
	{
		switch (event.key.keysym.sym) {
			case SDLK_LSHIFT: 
			case SDLK_RSHIFT: {
				AvatarMoverProcess::quarter = true;
				pout << "AvatarMoverProcess::quarter = " << AvatarMoverProcess::quarter << std::endl; 
			} break;
			case SDLK_c: {
				AvatarMoverProcess::clipping = !AvatarMoverProcess::clipping;
				pout << "AvatarMoverProcess::clipping = " << AvatarMoverProcess::clipping << std::endl; 
			} break;
			case SDLK_h: {
				AvatarMoverProcess::hitting = !AvatarMoverProcess::hitting;
				pout << "AvatarMoverProcess::hitting = " << AvatarMoverProcess::hitting << std::endl; 
			} break;
			case SDLK_UP: {
				if (!avatarInStasis) { 
					Process *p = new AvatarMoverProcess(-64,-64,0,0);
					Kernel::get_instance()->addProcess(p);
				} else { 
					pout << "Can't: avatarInStasis" << std::endl; 
				}
			} break;
			case SDLK_DOWN: {
				if (!avatarInStasis) { 
					Process *p = new AvatarMoverProcess(+64,+64,0,1);
					Kernel::get_instance()->addProcess(p);
				} else { 
					pout << "Can't: avatarInStasis" << std::endl; 
				}
			} break;
			case SDLK_LEFT: {
				if (!avatarInStasis) { 
					Process *p = new AvatarMoverProcess(-64,+64,0,2);
					Kernel::get_instance()->addProcess(p);
				} else { 
					pout << "Can't: avatarInStasis" << std::endl; 
				}
			} break;
			case SDLK_RIGHT: {
				if (!avatarInStasis) { 
					Process *p = new AvatarMoverProcess(+64,-64,0,3);
					Kernel::get_instance()->addProcess(p);
				} else { 
					pout << "Can't: avatarInStasis" << std::endl; 
				}
			} break;
			case SDLK_a: {
				if (!avatarInStasis) { 
					Process *p = new AvatarMoverProcess(0,0,8,4);
					Kernel::get_instance()->addProcess(p);
				} else { 
					pout << "Can't: avatarInStasis" << std::endl; 
				}
			} break;
			case SDLK_l: {
				pout << "Flushing fast area" << std::endl; 
				gameMapGump->FlushFastArea();
			} break;
			case SDLK_z: {
				if (!avatarInStasis) { 
					Process *p = new AvatarMoverProcess(0,0,-8,5);
					Kernel::get_instance()->addProcess(p);
				} else { 
					pout << "Can't: avatarInStasis" << std::endl; 
				}
			} break;
			case SDLK_KP_PLUS: {
				midi_volume+=8;
				if (midi_volume>255) midi_volume =255;
				pout << "Midi Volume is now: " << midi_volume << std::endl; 
				if (midi_driver) midi_driver->setGlobalVolume(midi_volume);
			} break;
			case SDLK_KP_MINUS: {
				midi_volume-=8;
				if (midi_volume<0) midi_volume = 0;
				pout << "Midi Volume is now: " << midi_volume << std::endl; 
				if (midi_driver) midi_driver->setGlobalVolume(midi_volume);
			} break;
			default:
				break;
		}
	}
	break;

	case SDL_KEYUP:
	{
		switch (event.key.keysym.sym) {
		case SDLK_LSHIFT: 
		case SDLK_RSHIFT: {
			AvatarMoverProcess::quarter = false;
			pout << "AvatarMoverProcess::quarter = " << AvatarMoverProcess::quarter << std::endl; 
		} break;
		case SDLK_UP: {
			if (AvatarMoverProcess::amp[0]) AvatarMoverProcess::amp[0]->terminate();
		} break;
		case SDLK_DOWN: {
			if (AvatarMoverProcess::amp[1]) AvatarMoverProcess::amp[1]->terminate();
		} break;
		case SDLK_LEFT: {
			if (AvatarMoverProcess::amp[2]) AvatarMoverProcess::amp[2]->terminate();
		} break;
		case SDLK_RIGHT: {
			if (AvatarMoverProcess::amp[3]) AvatarMoverProcess::amp[3]->terminate();
		} break;
		case SDLK_a: {
			if (AvatarMoverProcess::amp[4]) AvatarMoverProcess::amp[4]->terminate();
		} break;
		case SDLK_z: {
			if (AvatarMoverProcess::amp[5]) AvatarMoverProcess::amp[5]->terminate();
		} break;

		case SDLK_BACKQUOTE: {
			consoleGump->ToggleConsole();
			break;
		}
		case SDLK_LEFTBRACKET: gameMapGump->IncSortOrder(-1); break;
		case SDLK_RIGHTBRACKET: gameMapGump->IncSortOrder(+1); break;
		case SDLK_ESCAPE: case SDLK_q: isRunning = false; break;
		case SDLK_PAGEUP: {
			if (!consoleGump->ConsoleIsVisible()) con.ScrollConsole(-3);
			break;
		}
		case SDLK_PAGEDOWN: {
			if (!consoleGump->ConsoleIsVisible()) con.ScrollConsole(3); 
			break;
		}
		case SDLK_F9: { // quicksave
			saveGame("@work/quicksave");
		} break;
		case SDLK_F10: { // quickload
			loadGame("@work/quicksave");
		} break;
		case SDLK_s: { // toggle avatarInStasis

			avatarInStasis = !avatarInStasis;
			pout << "avatarInStasis = " << avatarInStasis << std::endl; 
		} break;
		case SDLK_t: { // engine stats
			Kernel::get_instance()->kernelStats();
			ObjectManager::get_instance()->objectStats();
			UCMachine::get_instance()->usecodeStats();
			World::get_instance()->worldStats();
		} break;
		case SDLK_e: { // editor objects toggle
			paintEditorItems = !paintEditorItems;
			pout << "paintEditorItems = " << paintEditorItems << std::endl;
		} break;
		case SDLK_x: {
			Item* item = World::get_instance()->getItem(19204);
			if (!item) break;
			sint32 x,y,z;
			item->getLocation(x,y,z);
			pout << "19204: (" << x << "," << y << "," << z << ")" << std::endl;			
		} break;
		case SDLK_f: { // trigger 'first' egg
			if (avatarInStasis) {
				pout << "Can't: avatarInStasis" << std::endl;
				break;
			}

			CurrentMap* currentmap = World::get_instance()->getCurrentMap();
			UCList uclist(2);
			// (shape == 73 && quality == 36)
			//const uint8 script[] = "@%\x49\x00=*%\x24\x00=&$";
			LOOPSCRIPT(script, LS_AND(LS_SHAPE_EQUAL1(73), LS_Q_EQUAL(36)));
			currentmap->areaSearch(&uclist, script, sizeof(script),
								   0, 256, false, 16188, 7500);
			if (uclist.getSize() < 1) {
				perr << "Unable to find FIRST egg!" << std::endl;
				break;
			}
			uint16 objid = uclist.getuint16(0);

			Egg* egg = p_dynamic_cast<Egg*>(
				ObjectManager::get_instance()->getObject(objid));
			sint32 ix, iy, iz;
			egg->getLocation(ix,iy,iz);
			// Center on egg
			CameraProcess::SetCameraProcess(new CameraProcess(ix,iy,iz));
			egg->hatch();
		} break;
		case SDLK_g: { // trigger 'execution' egg
			if (avatarInStasis) {
				pout << "Can't: avatarInStasis" << std::endl;
				break;
			}

			CurrentMap* currentmap = World::get_instance()->getCurrentMap();
			UCList uclist(2);
			// (shape == 73 && quality == 4)
			//const uint8* script = "@%\x49\x00=*%\x04\x00=&$";
			LOOPSCRIPT(script, LS_AND(LS_SHAPE_EQUAL1(73), LS_Q_EQUAL(4)));
			currentmap->areaSearch(&uclist, script, sizeof(script),
								   0, 256, false, 11732, 5844);
			if (uclist.getSize() < 1) {
				perr << "Unable to find EXCUTION egg!" << std::endl;
				break;
			}
			uint16 objid = uclist.getuint16(0);

			Egg* egg = p_dynamic_cast<Egg*>(
				ObjectManager::get_instance()->getObject(objid));
			Actor* avatar = World::get_instance()->getNPC(1);
			sint32 x,y,z;
			egg->getLocation(x,y,z);
			avatar->collideMove(x,y,z,true,true);
			egg->hatch();
		} break;
		default: break;
		}
	}
	break;

	// any more useful events?

	default:
		break;
	}
}

void GUIApp::handleDelayedEvents()
{
	uint32 now = SDL_GetTicks();
	for (int button = 0; button <= NUM_MOUSEBUTTONS; ++button) {
		if (!(mouseState[button] & (MBS_HANDLED | MBS_DOWN)) &&
			now - lastMouseDown[button] > 200) // !constant
		{
			Gump* gump = getGump(mouseDownGump[button]);
			if (gump)
				gump->OnMouseClick(button, mouseDownX[button],
								   mouseDownY[button]);

			mouseDownGump[button] = 0;
			mouseState[button] &= ~MBS_HANDLED;
		}
	}

}

bool GUIApp::saveGame(std::string filename)
{
	pout << "Saving..." << std::endl;

	ODataSource* ods = filesystem->WriteFile(filename);
	if (!ods) return false;

	SavegameWriter* sgw = new SavegameWriter(ods);
	sgw->start(9); // 8 files + version
	sgw->writeVersion(1);

	OBufferDataSource buf;

	kernel->save(&buf);
	sgw->writeFile("KERNEL", &buf);
	buf.clear();

	objectmanager->save(&buf);
	sgw->writeFile("OBJECTS", &buf);
	buf.clear();

	world->save(&buf);
	sgw->writeFile("WORLD", &buf);
	buf.clear();

	world->saveMaps(&buf);
	sgw->writeFile("MAPS", &buf);
	buf.clear();

	ucmachine->saveStrings(&buf);
	sgw->writeFile("UCSTRINGS", &buf);
	buf.clear();

	ucmachine->saveGlobals(&buf);
	sgw->writeFile("UCGLOBALS", &buf);
	buf.clear();

	ucmachine->saveLists(&buf);
	sgw->writeFile("UCLISTS", &buf);
	buf.clear();

	save(&buf);
	sgw->writeFile("APP", &buf);
	buf.clear();

	sgw->fixupCount();
	delete sgw;

	pout << "Done" << std::endl;

	return true;
}

bool GUIApp::loadGame(std::string filename)
{
	pout << "Loading..." << std::endl;

	IDataSource* ids = filesystem->ReadFile(filename);
	if (!ids) {
		perr << "Can't find file: " << filename << std::endl;
		return false;
	}

	Savegame* sg = new Savegame(ids);
	int version = sg->getVersion();
	if (version != 1) {
		perr << "Unsupported savegame version (" << version << ")"
			 << std::endl;
		return false;
	}

	// kill music
	if (midi_driver) {
		for (int i = 0; i < midi_driver->maxSequences(); i++) {
			midi_driver->finishSequence(i);
		}
	}

	// now, reset everything (order matters)
	world->reset();
	ucmachine->reset();
	// ObjectManager, Kernel have to be last, because they kill
	// all processes/objects
	objectmanager->reset();
	kernel->reset();

	// TODO: reset mouse state in GUIApp

 	// and load everything back (order matters)
	IDataSource* ds;

	// UCSTRINGS, UCGLOBALS, UCLISTS, APP don't depend on anything else,
	// so load these first
	ds = sg->get_datasource("UCSTRINGS");
	ucmachine->loadStrings(ds);
	delete ds;

	ds = sg->get_datasource("UCGLOBALS");
	ucmachine->loadGlobals(ds);
	delete ds;

	ds = sg->get_datasource("UCLISTS");
	ucmachine->loadLists(ds);
	delete ds;

	ds = sg->get_datasource("APP");
	load(ds);
	delete ds;

	// KERNEL must be before OBJECTS, for the egghatcher
	ds = sg->get_datasource("KERNEL");
	kernel->load(ds);
	delete ds;

	// WORLD must be before OBJECTS, for the egghatcher
	ds = sg->get_datasource("WORLD");
	world->load(ds);
	delete ds;

	ds = sg->get_datasource("OBJECTS");
	objectmanager->load(ds);
	delete ds;

	ds = sg->get_datasource("MAPS");
	world->loadMaps(ds);
	delete ds;

	//!! temporary hack: reset desktopgump, gamemapgump, consolegump:
	desktopGump = 0;
	for (unsigned int i = 256; i < 65535; ++i) {
		DesktopGump* dg = p_dynamic_cast<DesktopGump*>(getGump(i));
		if (dg) {
			desktopGump = dg;
			break;
		}
	}
	assert(desktopGump);
	gameMapGump = p_dynamic_cast<GameMapGump*>(desktopGump->
											   FindGump<GameMapGump>());
	assert(gameMapGump);

	consoleGump = p_dynamic_cast<ConsoleGump*>(desktopGump->
											   FindGump<ConsoleGump>());
	assert(consoleGump);

	pout << "Done" << std::endl;


	delete sg;
	return false;
}

Gump* GUIApp::getGump(uint16 gumpid)
{
	return p_dynamic_cast<Gump*>(ObjectManager::get_instance()->
								 getObject(gumpid));
}

void GUIApp::save(ODataSource* ods)
{
	ods->write2(1); //version
	uint8 s = (avatarInStasis ? 1 : 0);
	ods->write1(s);
	ods->write4(static_cast<uint32>(timeOffset));
	ods->write4(framenum);
}

bool GUIApp::load(IDataSource* ids)
{
	uint16 version = ids->read2();
	if (version != 1) return false;

	avatarInStasis = (ids->read1() != 0);
	timeOffset = static_cast<sint32>(ids->read4());
	framenum = ids->read4();

	return true;
}

uint32 GUIApp::I_getCurrentTimerTick(const uint8* /*args*/,
										unsigned int /*argsize*/)
{
	// number of ticks of a 60Hz timer, with the default animrate of 30Hz
	return get_instance()->getFrameNum()*2;
}

uint32 GUIApp::I_setAvatarInStasis(const uint8* args, unsigned int /*argsize*/)
{
	ARG_SINT16(statis);
	get_instance()->setAvatarInStasis(statis!=0);
	return 0;
}

uint32 GUIApp::I_getAvatarInStasis(const uint8* /*args*/, unsigned int /*argsize*/)
{
	if (get_instance()->avatarInStasis)
		return 1;
	else
		return 0;
}

uint32 GUIApp::I_getTimeInGameHours(const uint8* /*args*/,
										unsigned int /*argsize*/)
{
	// 1 game hour per every 27000 frames
	return (get_instance()->getFrameNum()+get_instance()->timeOffset)/27000;
}

uint32 GUIApp::I_getTimeInMinutes(const uint8* /*args*/,
										unsigned int /*argsize*/)
{
	// 1 minute per every 1800 frames
	return (get_instance()->getFrameNum()+get_instance()->timeOffset)/1800;
}

uint32 GUIApp::I_getTimeInSeconds(const uint8* /*args*/,
										unsigned int /*argsize*/)
{
	// 1 second per every 30 frames
	return (get_instance()->getFrameNum()+get_instance()->timeOffset)/30;
}

uint32 GUIApp::I_setTimeInGameHours(const uint8* args,
										unsigned int /*argsize*/)
{
	ARG_UINT16(newhour);

	// 1 game hour per every 27000 frames
	sint32	absolute = newhour*27000;
	get_instance()->timeOffset = absolute-get_instance()->getFrameNum();

	return 0;
}

