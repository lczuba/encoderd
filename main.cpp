#include <iostream>
#include <alsa/asoundlib.h>
#include <alsa/mixer.h>

#include <stdexcept>
#include <unistd.h>
#include <pigpiod_if2.h>

#include <chrono>
#include <thread>
#include <bitset>

const char *hostPtr = NULL;
const char *portPtr = NULL;

const int gpioA = 23;
const int gpioB = 24;
const int gpioC = 22;

const int glitchFilterMicroSec = 1000;

void validateStatus(const int status)
{	
	if(status != 0)
	{
		throw std::runtime_error("pigpiod runtime error: " + status);
	}
}

static long muteVolume = 0;

void changeVolume(int change, const std::string& interface = "Digital")
{
	/*
    changeVolume(10) --> increase by 10%
    changeVolume(-5) --> decrease by -5%
    changeVolume(0)  --> mute
	*/

    int status = 0;
    long min, max, volume;
    snd_mixer_t *handle;
    snd_mixer_selem_id_t *sid;
    const char *card = "default";

    snd_mixer_open(&handle, 0);
    snd_mixer_attach(handle, card);
    snd_mixer_selem_register(handle, NULL, NULL);
    snd_mixer_load(handle);

    snd_mixer_selem_id_alloca(&sid);
    snd_mixer_selem_id_set_index(sid, 0);
    snd_mixer_selem_id_set_name(sid, interface.c_str());
    snd_mixer_elem_t* elem = snd_mixer_find_selem(handle, sid);

    snd_mixer_selem_get_playback_volume_range(elem, &min, &max);
    snd_mixer_selem_get_playback_volume(elem, snd_mixer_selem_channel_id_t::SND_MIXER_SCHN_MONO, &volume);
    printf("%s: current volume = %ld\n", __func__, volume);

    if(change > 0)
    {
        volume += (change * max) / 100;
    }
    else if(change < 0)
    {
        volume -= (abs(change) * max) / 100;
    }
    else
    {
		muteVolume = (volume * 100) / max;
        volume = 0;
    }

    printf("%s: set volume = %ld\n", __func__, volume);
    status = snd_mixer_selem_set_playback_volume_all(elem, volume);

    snd_mixer_close(handle);
}

static int encoderTransits[16]=
{
/* 0000 0001 0010 0011 0100 0101 0110 0111 */
      0,  -1,   1,   0,   1,   0,   0,  -1,
/* 1000 1001 1010 1011 1100 1101 1110 1111 */
     -1,   0,   0,   1,   0,   1,  -1,   0
};

static void encoderCallback(int status, unsigned gpio, unsigned level, uint32_t tick)
{
	static int levelA, levelB, oldState, step;
	
	if(level == PI_TIMEOUT)
	{
		return;
	}

	if(gpio == gpioC )
	{
		if(level == 0)
		{
			return;
		}

		if(muteVolume == 0)
		{
			printf("%s: mute audio\n", __func__);
			changeVolume(0);
			return;
		}
		
		printf("%s: unmute audio\n", __func__);
		changeVolume(muteVolume);
		muteVolume = 0;
		return;
	}

	if(gpio == gpioA)
	{
		levelA = level;
	} 
	else
	{
		levelB = level;
	}

	int newState = levelA << 1 | levelB;
	int change = encoderTransits[oldState << 2 | newState];

	// std::cout << "newState: " << std::bitset<4>{newState} << std::endl;
	// std::cout << "change: " << std::bitset<4>{change} << std::endl;

	if(change)
	{
		oldState = newState;
		step += abs(change);

		if(step > 4)
		{
			step = 0;
			printf("%s: change vol: %d\n", __func__, change);
			changeVolume(change);
		}
	}
}

int main()
{
	printf("%s: Set Headphone to 60%\n", __func__);
	changeVolume(60, std::string{"Headphone"});

	printf("%s: Connect to pigpiod\n", __func__);
	int status = pigpio_start(hostPtr, portPtr);
	validateStatus(status);

	printf("%s: Init GPIO_A: %d\n", __func__, gpioA);
	set_mode(status, gpioA, PI_INPUT);
	validateStatus(status);
	set_pull_up_down(status, gpioA, PI_PUD_UP);
	validateStatus(status);
	set_glitch_filter(status, gpioA, glitchFilterMicroSec);
	validateStatus(status);

	printf("%s: Init GPIO_B: %d\n", __func__, gpioB);
	set_mode(status, gpioB, PI_INPUT);
	validateStatus(status);
	set_pull_up_down(status, gpioB, PI_PUD_UP);
	validateStatus(status);
	set_glitch_filter(status, gpioB, glitchFilterMicroSec);
	validateStatus(status);

	printf("%s: Init GPIO_C: %d\n", __func__, gpioC);
	set_mode(status, gpioC, PI_INPUT);
	validateStatus(status);
	set_pull_up_down(status, gpioC, PI_PUD_UP);
	validateStatus(status);
	set_glitch_filter(status, gpioC, glitchFilterMicroSec);
	validateStatus(status);

	printf("%s: Set callback for GPIO_A\n", __func__);
	int gpioAcallbackId = callback(status, gpioA, EITHER_EDGE, encoderCallback);
	validateStatus(status);

	printf("%s: Set callback for GPIO_B\n", __func__);
	int gpioBcallbackId = callback(status, gpioB, EITHER_EDGE, encoderCallback);
	validateStatus(status);

	printf("%s: Set callback for GPIO_C\n", __func__);
	int gpioCcallbackId = callback(status, gpioC, EITHER_EDGE, encoderCallback);
	validateStatus(status);

	while(1) std::this_thread::sleep_for(std::chrono::milliseconds(60));

	pigpio_stop(status);
	validateStatus(status);
}
