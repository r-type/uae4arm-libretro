#include "libretro.h"

#include "libretro-core.h"

#include "sysconfig.h"
#include "sysdeps.h"
#include "options.h"
#include "custom.h"

cothread_t mainThread;
cothread_t emuThread;

int CROP_WIDTH;
int CROP_HEIGHT;
int VIRTUAL_WIDTH ;
int retrow=1024; 
int retroh=1024;

#define RETRO_DEVICE_AMIGA_KEYBOARD RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_KEYBOARD, 0)
#define RETRO_DEVICE_AMIGA_JOYSTICK RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 1)

unsigned amiga_devices[ 2 ];

extern int SHIFTON,pauseg,SND ,snd_sampler;
extern short signed int SNDBUF[1024*2];
extern char RPATH[512];
extern char RETRO_DIR[512];
extern int SHOWKEY;

#include "cmdline.cpp"

extern void update_input(void);
extern void texture_init(void);
extern void texture_uninit(void);
extern void Emu_init();
extern void Emu_uninit();
extern void input_gui(void);
extern void retro_virtualkb(void);

const char *retro_save_directory;
const char *retro_system_directory;
const char *retro_content_directory;
char retro_system_data_directory[512];;

static retro_video_refresh_t video_cb;
static retro_audio_sample_t audio_cb;
static retro_audio_sample_batch_t audio_batch_cb;
static retro_environment_t environ_cb;

// Amiga default kickstarts

#define A500_ROM        "kick34005.A500"
#define A600_ROM        "kick40063.A600"
#define A1200_ROM       "kick40068.A1200"

static char uae_machine[256];
static char uae_kickstart[16];
static char uae_config[1024];


#ifdef _WIN32
#define RETRO_PATH_SEPARATOR            "\\"
// Windows also support the unix path separator
#define RETRO_PATH_SEPARATOR_ALT        "/"
#else
#define RETRO_PATH_SEPARATOR            "/"
#endif

void path_join(char* out, const char* basedir, const char* filename)
{
   snprintf(out, MAX_PATH, "%s%s%s", basedir, RETRO_PATH_SEPARATOR, filename);
}


void retro_set_environment(retro_environment_t cb)
{
   environ_cb = cb;

   static const struct retro_controller_description p1_controllers[] = {
      { "AMIGA Joystick", RETRO_DEVICE_AMIGA_JOYSTICK },
      { "AMIGA Keyboard", RETRO_DEVICE_AMIGA_KEYBOARD },
   };
   static const struct retro_controller_description p2_controllers[] = {
      { "AMIGA Joystick", RETRO_DEVICE_AMIGA_JOYSTICK },
      { "AMIGA Keyboard", RETRO_DEVICE_AMIGA_KEYBOARD },
   };

   static const struct retro_controller_info ports[] = {
      { p1_controllers, 2  }, // port 1
      { p2_controllers, 2  }, // port 2
      { NULL, 0 }
   };

   cb( RETRO_ENVIRONMENT_SET_CONTROLLER_INFO, (void*)ports );

   struct retro_variable variables[] = {

      { "uae4arm_model",          "Model; A500|A600|A1200", },
      { "uae4arm_resolution",     "Internal resolution; 640x270|320x240|320x256|320x262|640x240|640x256|640x262|640x270|768x270", },
      { "uae4arm_leds_on_screen", "Leds on screen; on|off", },
      { "uae4arm_floppy_speed",   "Floppy speed; 100|200|400|800", },
      { NULL, NULL },
   };

   cb(RETRO_ENVIRONMENT_SET_VARIABLES, variables);
}


static void update_variables(void)
{
   uae_machine[0] = '\0';
   uae_config[0]  = '\0';

   struct retro_variable var = {0};

   var.key = "uae4arm_resolution";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      char *pch;
      char str[100];
      snprintf(str, sizeof(str), var.value);

      pch = strtok(str, "x");
      if (pch)
         retrow = strtoul(pch, NULL, 0);
      pch = strtok(NULL, "x");
      if (pch)
         retroh = strtoul(pch, NULL, 0);

      changed_prefs.gfx_size.width  = retrow;
      changed_prefs.gfx_size.height = retroh;
      changed_prefs.gfx_resolution  = changed_prefs.gfx_size.width > 600 ? 1 : 0;

      LOGI("[libretro-uae4arm]: Got size: %u x %u.\n", retrow, retroh);

      CROP_WIDTH =retrow;
      CROP_HEIGHT= (retroh-80);
      VIRTUAL_WIDTH = retrow;
      texture_init();

   }

   var.key = "uae4arm_leds_on_screen";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (strcmp(var.value, "on") == 0)  changed_prefs.leds_on_screen = 1;
      if (strcmp(var.value, "off") == 0) changed_prefs.leds_on_screen = 0;
   }

   var.key = "uae4arm_model";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (strcmp(var.value, "A500") == 0)
      {
         //strcat(uae_machine, A500);
         //strcpy(uae_kickstart, A500_ROM);
         //changed_prefs.cpu_type="68000";
         
         changed_prefs.cpu_model = 68000;
         changed_prefs.m68k_speed = M68K_SPEED_7MHZ_CYCLES;
         changed_prefs.cpu_compatible = 0;
         changed_prefs.chipmem_size = 2 * 0x80000;
         changed_prefs.address_space_24 = 1;
         changed_prefs.chipset_mask = CSMASK_ECS_AGNUS;
         //strcpy(changed_prefs.romfile, A500_ROM);
         path_join(changed_prefs.romfile, retro_system_directory, A500_ROM);
      }
      if (strcmp(var.value, "A600") == 0)
      {
         //strcat(uae_machine, A600);
         //strcpy(uae_kickstart, A600_ROM);
         changed_prefs.cpu_model = 68000;
         changed_prefs.chipmem_size = 2 * 0x80000;
         changed_prefs.m68k_speed = M68K_SPEED_7MHZ_CYCLES;
         changed_prefs.cpu_compatible = 0;
         changed_prefs.address_space_24 = 1;
         changed_prefs.chipset_mask = CSMASK_ECS_DENISE | CSMASK_ECS_AGNUS;
         //strcpy(changed_prefs.romfile, A600_ROM);
         path_join(changed_prefs.romfile, retro_system_directory, A600_ROM);
      }
      if (strcmp(var.value, "A1200") == 0)
      {
         //strcat(uae_machine, A1200);
         //strcpy(uae_kickstart, A1200_ROM);
         //changed_prefs.cpu_type="68ec020";
         changed_prefs.cpu_model = 68020;
         changed_prefs.chipmem_size = 4 * 0x80000;
         changed_prefs.m68k_speed = M68K_SPEED_14MHZ_CYCLES;
         changed_prefs.cpu_compatible = 0;
         changed_prefs.address_space_24 = 1;
         changed_prefs.chipset_mask = CSMASK_AGA | CSMASK_ECS_DENISE | CSMASK_ECS_AGNUS;
         //strcpy(changed_prefs.romfile, A1200_ROM);
         path_join(changed_prefs.romfile, retro_system_directory, A1200_ROM);
      }
   }




   var.key = "uae4arm_floppy_speed";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      changed_prefs.floppy_speed=atoi(var.value);
   }
   
   //fixup_prefs (&changed_prefs);
}

static void retro_wrap_emulator()
{    

   pre_main(RPATH);

   LOGI("EXIT EMU THD\n");
   pauseg=-1;

   // Were done here
   co_switch(mainThread);

   // Dead emulator, but libco says not to return
   while(true)
   {
      LOGI("Running a dead emulator.");
      co_switch(mainThread);
   }

}

void Emu_init(){

   memset(Key_Sate,0,512);
   memset(Key_Sate2,0,512);

   if(!emuThread && !mainThread)
   {
      mainThread = co_active();
      emuThread = co_create(8*65536*sizeof(void*), retro_wrap_emulator);
   }

   default_prefs (&changed_prefs, 0);
   default_prefs (&currprefs, 0);

   update_variables();
}

void Emu_uninit(){

   texture_uninit();
}

void retro_shutdown_core(void)
{
   LOGI("SHUTDOWN\n");

   texture_uninit();
   environ_cb(RETRO_ENVIRONMENT_SHUTDOWN, NULL);
}

void retro_reset(void){

}

void retro_init(void)
{
   const char *system_dir = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &system_dir) && system_dir)
   {
      // if defined, use the system directory
      retro_system_directory=system_dir;
   }

   const char *content_dir = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_CONTENT_DIRECTORY, &content_dir) && content_dir)
   {
      // if defined, use the system directory
      retro_content_directory=content_dir;
   }

   const char *save_dir = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY, &save_dir) && save_dir)
   {
      // If save directory is defined use it, otherwise use system directory
      retro_save_directory = *save_dir ? save_dir : retro_system_directory;      
   }
   else
   {
      // make retro_save_directory the same in case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY is not implemented by the frontend
      retro_save_directory=retro_system_directory;
   }

   if(retro_system_directory==NULL)sprintf(RETRO_DIR, "%s\0",".");
   else sprintf(RETRO_DIR, "%s\0", retro_system_directory);

   sprintf(retro_system_data_directory, "%s/data\0",RETRO_DIR);

   LOGI("Retro SYSTEM_DIRECTORY %s\n",retro_system_directory);
   LOGI("Retro SAVE_DIRECTORY %s\n",retro_save_directory);
   LOGI("Retro CONTENT_DIRECTORY %s\n",retro_content_directory);

#ifndef RENDER16B
   enum retro_pixel_format fmt =RETRO_PIXEL_FORMAT_XRGB8888;
#else
   enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_RGB565;
#endif
   
   if (!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt))
   {
      fprintf(stderr, "PIXEL FORMAT is not supported.\n");
      LOGI("PIXEL FORMAT is not supported.\n");
      exit(0);
   }

   struct retro_input_descriptor inputDescriptors[] = {
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A,      "A" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B,      "B" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X,      "X" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y,      "Y" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "Select" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START,  "Start"  },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT,  "Right"  },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,   "Left"   },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP,     "Up"     },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,   "Down"   },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R,      "R"  },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L,      "L"  },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2,     "R2" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2,     "L2" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R3,     "R3" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L3,     "L3" }
   };
   environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, &inputDescriptors);

   Emu_init();

   texture_init();

}

extern void main_exit();
void retro_deinit(void)
{
   Emu_uninit(); 

   co_switch(emuThread);
   LOGI("exit emu\n");

   co_switch(mainThread);
   LOGI("exit main\n");
   if(emuThread)
   {
      co_delete(emuThread);
      emuThread = 0;
   }

   LOGI("Retro DeInit\n");
}

unsigned retro_api_version(void)
{
   return RETRO_API_VERSION;
}


void retro_set_controller_port_device( unsigned port, unsigned device )
{
  if ( port < 2 )
  {
    amiga_devices[ port ] = device;

    LOGI(" (%d)=%d \n",port,device);
  }
}

void retro_get_system_info(struct retro_system_info *info)
{
   memset(info, 0, sizeof(*info));
   info->library_name     = "uae4arm_chips-rtype_version";
   info->library_version  = "0.3";
   info->valid_extensions = "adf|dms|zip|ipf|hdf|lha|uae";
   info->need_fullpath    = true;
   info->block_extract    = true;
}

void retro_get_system_av_info(struct retro_system_av_info *info)
{
   //FIXME handle vice PAL/NTSC
   struct retro_game_geometry geom = { retrow, retroh, 1280, 1024,4.0 / 3.0 };
   struct retro_system_timing timing = { 50.0, 44100.0 };

   info->geometry = geom;
   info->timing   = timing;
}

void retro_set_audio_sample(retro_audio_sample_t cb)
{
   audio_cb = cb;
}

void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb)
{
   audio_batch_cb = cb;
}

void retro_set_video_refresh(retro_video_refresh_t cb)
{
   video_cb = cb;
}


void retro_audiocb(signed short int *sound_buffer,int sndbufsize){

    if(pauseg==0)
        audio_batch_cb(sound_buffer, sndbufsize);
}


void retro_run(void)
{
   int x;

   bool updated = false;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated)
      update_variables();

   if(pauseg==0)
   {
      if(SHOWKEY )retro_virtualkb();
   }

   video_cb(Retro_Screen,retrow,retroh,retrow<<PIXEL_BYTES);
 
   co_switch(emuThread);

}

bool retro_load_game(const struct retro_game_info *info)
{
   const char *full_path;

   (void)info;

   full_path = info->path;

   strcpy(RPATH,full_path);

   update_variables();

#ifdef RENDER16B
   memset(Retro_Screen,0,1280*1024*2);
#else
   memset(Retro_Screen,0,1280*1024*2*2);
#endif
   memset(SNDBUF,0,1024*2*2);

   co_switch(emuThread);

   return true;
}

void retro_unload_game(void)
{
   pauseg=-1;
}

unsigned retro_get_region(void)
{
   return RETRO_REGION_NTSC;
}

bool retro_load_game_special(unsigned type, const struct retro_game_info *info, size_t num)
{
   (void)type;
   (void)info;
   (void)num;
   return false;
}

size_t retro_serialize_size(void)
{
   return 0;
}

bool retro_serialize(void *data_, size_t size)
{
   return false;
}

bool retro_unserialize(const void *data_, size_t size)
{
   return false;
}

void *retro_get_memory_data(unsigned id)
{
   (void)id;
   return NULL;
}

size_t retro_get_memory_size(unsigned id)
{
   (void)id;
   return 0;
}

void retro_cheat_reset(void) {}

void retro_cheat_set(unsigned index, bool enabled, const char *code)
{
   (void)index;
   (void)enabled;
   (void)code;
}

