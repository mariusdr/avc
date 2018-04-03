#include <iomanip>
#include <iostream>
#include <limits>

#include <boost/program_options.hpp>
namespace bpo = boost::program_options;
#include <boost/thread.hpp>
#include <boost/type_traits.hpp>
#include <alsa/asoundlib.h>

#if 0
#include <asm/byteorder.h>
#define le16toh(x) __le16_to_cpu(x)
#define be16toh(x) __be16_to_cpu(x)
#define le32toh(x) __le32_to_cpu(x)
#define be32toh(x) __be32_to_cpu(x)
#endif


#define VERBOSE 1
#define PEAK_DECAY 0.8f
#define FILTER_COEFF 0.2f

class VolumeController {
public:

	VolumeController(snd_mixer_t* mixer_handle, snd_mixer_elem_t* mixer_elem_handle,
					 long base_volume, long max_volume,
					 float filter_coeff = 0.2f, float peak_decay = 1.f, bool verbose = false):
		mixer_handle(mixer_handle), mixer_elem_handle(mixer_elem_handle),
		base_volume(base_volume), max_volume(max_volume),
		capture_fstore(0.f), looprec_fstore(0.f),
		filter_coeff(filter_coeff), peak_decay(peak_decay), verbose(verbose)
	{}


	void adjust_from_peak(unsigned looprec_peak, unsigned capture_peak) {
		looprec_peak = filter(looprec_fstore, looprec_peak);	
		capture_peak = filter(capture_fstore, capture_peak);	

		int delta = (int) capture_peak - (int) looprec_peak;
		int mod = int(std::floor(peak_decay * delta / 1000));

		long current_volume;
		snd_mixer_selem_get_playback_volume(mixer_elem_handle, SND_MIXER_SCHN_MONO, &current_volume);

		long next_volume = std::max(base_volume, std::min(current_volume + mod, max_volume));

		if (looprec_peak != 0) 
			snd_mixer_selem_set_playback_volume(mixer_elem_handle, SND_MIXER_SCHN_MONO, next_volume);

		if (verbose) {
			std::cout << "looprec peak: " << std::setw(9) << looprec_peak << " | "
					  << "capture peak: " << std::setw(9) << capture_peak << " | "
					  << "delta: " << std::setw(9) << delta << " | "
					  << "current volume: " << std::setw(9) << current_volume << " | "
					  << "volume change: " << std::setw(9) << mod << " | "
					  << "\n";
		}	
		
		snd_mixer_handle_events(mixer_handle);
	}

	void set_verbose(bool flag) {
		verbose = flag;
	}

	void set_filter_coeff(float coeff) {
		filter_coeff = coeff;
	}

	void set_peak_decay(float decay) {
		peak_decay = decay;
	}
	
private:
	snd_mixer_t* mixer_handle;
	snd_mixer_elem_t* mixer_elem_handle;

	long base_volume;
	long max_volume;

    float capture_fstore;
    float looprec_fstore;

	float filter_coeff;
	float peak_decay;

	bool verbose;
	
	uint16_t filter(float &store, uint16_t in) {
		float inf(in);
		store = (1 - filter_coeff) * store + filter_coeff * inf;
		return store;
	}
};


void init_capture_pcm(snd_pcm_t** capture_handle, const char* capture_device_name, const snd_pcm_format_t& format, unsigned int rate)
{
	int err;
	snd_pcm_hw_params_t* hw_params;

    if ((err = snd_pcm_open(capture_handle, capture_device_name, SND_PCM_STREAM_CAPTURE, 0)) < 0) {
        std::cerr << "cannot open audio device " << capture_device_name << " failed " << snd_strerror(err) << std::endl;
        exit (1);
    }

    if ((err = snd_pcm_hw_params_malloc(&hw_params)) < 0) {
        std::cerr << "cannot allocate hardware parameter structure " << snd_strerror(err) << std::endl;
        exit (1);
    }
	
    if ((err = snd_pcm_hw_params_any(*capture_handle, hw_params)) < 0) {
        std::cerr << "cannot initialize hardware parameter structure " << snd_strerror(err) << std::endl;
        exit (1);
    }

    if ((err = snd_pcm_hw_params_set_access(*capture_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
        std::cerr << "cannot set access type " << snd_strerror(err) << std::endl;
        exit (1);
    }

    if ((err = snd_pcm_hw_params_set_format(*capture_handle, hw_params, format)) < 0) {
        std::cerr << "cannot set sample format " << snd_strerror(err) << std::endl;
        exit (1);
    }

    if ((err = snd_pcm_hw_params_set_rate_near(*capture_handle, hw_params, &rate, 0)) < 0) {
        std::cerr << "cannot set sample rate " << snd_strerror(err) << std::endl;
        exit (1);
    }

    if ((err = snd_pcm_hw_params_set_channels(*capture_handle, hw_params, 2)) < 0) {
        std::cerr << "cannot set channel count (%s)" << snd_strerror(err) << std::endl;
        exit (1);
    }

    if ((err = snd_pcm_hw_params(*capture_handle, hw_params)) < 0) {
        std::cerr << "cannot set parameters " << snd_strerror(err) << std::endl;
        exit (1);
    }

	snd_pcm_hw_params_free (hw_params);
}


void init_mixer(snd_mixer_t** mixer_handle, snd_mixer_selem_id_t** sid, const char* card, const char* selem_name)
{
	int err;

	if ((err = snd_mixer_open(mixer_handle, 0)) != 0) {
		std::cerr << "failed to open mixer: " << snd_strerror(err) << std::endl;
		exit(1);
	}

	if ((err = snd_mixer_attach(*mixer_handle, card)) != 0) {
		std::cerr << "failed to attach card " << card << " to mixer: " << snd_strerror(err) << std::endl;
		exit(1);
	}
	
	if ((err = snd_mixer_selem_register(*mixer_handle, NULL, NULL)) != 0) {
		std::cerr << "failed to register simple mixer element: " << snd_strerror(err) << std::endl;
		exit(1);
	}

	if ((err = snd_mixer_load(*mixer_handle)) != 0) {
		std::cerr << "failed to load mixer: " << snd_strerror(err) << std::endl;
		exit(1);
	}

	snd_mixer_selem_id_alloca(sid);
	snd_mixer_selem_id_set_index(*sid, 0);
	snd_mixer_selem_id_set_name(*sid, selem_name);
}

unsigned calc_peak_sle16(const uint8_t *data, std::size_t cnt_samples, int16_t silencemask16)
{
    uint16_t max = 0;
		
    for (unsigned u = 0; u < cnt_samples; ++u)
    {
        const int16_t *sample = reinterpret_cast<const int16_t *>(data);
        int16_t sval = le16toh(*sample);
        sval = sval ^ silencemask16;
#if 0
        std::ostringstream xs;
        const uint8_t &d0 = data[0];
        const uint8_t &d1 = data[1];
        xs << "psamle " << u << "->" << std::setfill('0') << std::hex << std::setw(2) << uint16_t(d0)
           << ':' << std::setw(2) << uint16_t(d1)
           << ' ' << std::setw(4) << sval;
        std::cout << xs.str() << std::endl;
#endif

        if (sval < 0)
            sval = -sval;

        data++;
        data++;

        max = std::max(max, uint16_t(sval));
    }
    return max;
}


int main(int argc, char** argv)
{
    int err;

	bpo::options_description desc("avc");
	desc.add_options()
		("help,h", "show help menu")
		("looprec,L", bpo::value<std::string>(), "loop recording device")
		("capture,C", bpo::value<std::string>(), "capture recording device")
		("mixer_element,M", bpo::value<std::string>()->default_value("Master"), "mixer element for volume control, default is 'Master'")
		("playback,P", bpo::value<std::string>(), "playback device" )
		("base_volume", bpo::value<long>(), "base volume (if not provided use current volume)" )
		("max_volume", bpo::value<long>(), "max volume (if not provided use mixer default)" )
		("print_volume_range", "print (min, max) volume of playback device mixer and exit" );
		
	bpo::variables_map vm;
	bpo::store(bpo::command_line_parser(argc, argv).options(desc).run(), vm);	 	

	if (vm.count("help")) {
		std::cout << desc << std::endl;
		exit(0);
	}

	if (!vm.count("looprec")) {
		std::cerr << "loop recording device required" << std::endl;
		exit(1);
	}	
	
	if (!vm.count("capture")) {
		std::cerr << "capture recording device required" << std::endl;
		exit(1);
	}

	if (!vm.count("playback")) {
		std::cerr << "playback device required" << std::endl;
		exit(1);
	}
	
	const std::string& playback_device = vm["playback"].as<std::string>();
	const std::string& looprec_device = vm["looprec"].as<std::string>();
	const std::string& capture_device = vm["capture"].as<std::string>();
	const std::string& mixer_selem_name = vm["mixer_element"].as<std::string>();

    unsigned int rate = 8000;
    snd_pcm_format_t format = SND_PCM_FORMAT_S16_LE;
    int16_t silencemask16 = snd_pcm_format_silence_16(format);

	snd_pcm_t* looprec_pcm_handle;
	init_capture_pcm(&looprec_pcm_handle, looprec_device.c_str(), format, rate);
	if ((err = snd_pcm_prepare(looprec_pcm_handle)) < 0) {
        std::cerr << "cannot prepare looprec device for use " << snd_strerror(err) << std::endl;
        exit (1);
    }

	snd_pcm_t* capture_pcm_handle;
	init_capture_pcm(&capture_pcm_handle, capture_device.c_str(), format, rate);
	if ((err = snd_pcm_prepare(capture_pcm_handle)) < 0) {
        std::cerr << "cannot prepare capture device for use " << snd_strerror(err) << std::endl;
        exit (1);
    }

	snd_mixer_t* mixer_handle;
    snd_mixer_selem_id_t* sid;
	init_mixer(&mixer_handle, &sid, playback_device.c_str(), mixer_selem_name.c_str());
    snd_mixer_elem_t* mixer_elem_handle = snd_mixer_find_selem(mixer_handle, sid);

	long default_min_volume, default_max_volume;
	snd_mixer_selem_get_playback_volume_range(mixer_elem_handle, &default_min_volume, &default_max_volume);

	if (vm.count("print_volume_range")) {
		std::cout << "min volume: " << default_min_volume << ", max volume: " << default_max_volume << std::endl;
		exit(0);
	}
	
	long base_volume = 0;
	if (!vm.count("base_volume")) {
		snd_mixer_selem_get_playback_volume(mixer_elem_handle, SND_MIXER_SCHN_MONO, &base_volume);
	} else {
		base_volume = std::max(vm["base_volume"].as<long>(), default_min_volume);
	}

	long max_volume = 0;
	if (!vm.count("max_volume")) {
		max_volume = default_max_volume;
	} else {
		max_volume = std::min(vm["max_volume"].as<long>(), default_max_volume);
	}

	VolumeController vc(mixer_handle, mixer_elem_handle, base_volume, max_volume);
	vc.set_verbose(VERBOSE);
	vc.set_filter_coeff(FILTER_COEFF);
	vc.set_peak_decay(PEAK_DECAY);
	
	// MAIN LOOP

    unsigned buffer_frames = 128;
    unsigned buffersize = buffer_frames * snd_pcm_format_width(format) / 8 * 2; // stereobuffer

	char* capture_buffer = new char[buffersize];
	char* looprec_buffer = new char[buffersize];
 		
    for (;;)
    {
		if ((err = snd_pcm_readi(capture_pcm_handle, capture_buffer, buffer_frames)) < 0)
        {
            std::cerr << "read from capture interface failed: " << snd_strerror(err) << std::endl;
			err = snd_pcm_recover(capture_pcm_handle, err, 0);
			if (err != 0) {
				std::cerr << "failed to recover ... exiting" << std::endl;	
				goto cleanup;
			}
        }

		if ((err = snd_pcm_readi(looprec_pcm_handle, looprec_buffer, buffer_frames)) < 0)
        {
            std::cerr << "read from looprec interface failed: " << snd_strerror(err) << std::endl;
			err = snd_pcm_recover(looprec_pcm_handle, err, 0);
			if (err != 0) {
				std::cerr << "failed to recover ... exiting" << std::endl;	
				goto cleanup;
			}
        }

		unsigned capture_peak = 0;
		unsigned looprec_peak = 0;

		switch (format) {
		case SND_PCM_FORMAT_S16_LE:
			{
				capture_peak = calc_peak_sle16((const uint8_t *)capture_buffer, std::size_t(buffer_frames) * 2, silencemask16);
				looprec_peak = calc_peak_sle16((const uint8_t *)looprec_buffer, std::size_t(buffer_frames) * 2, silencemask16);
				break;
			}
		default:
			break;
		}

		vc.adjust_from_peak(looprec_peak, capture_peak);
    }

 cleanup:

	delete [] capture_buffer;
	delete [] looprec_buffer;
	
    // free(buffer);
    snd_pcm_close(looprec_pcm_handle);
    snd_pcm_close(capture_pcm_handle);
	snd_mixer_close(mixer_handle);
	
    exit(1);
}

