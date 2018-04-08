# Adaptive Volume Controller with ALSA

## Built with
Requires ALSA, the Boost program options library and C++11 or newer.

## ALSA Config
Install and load the snd-aloop kernel module.  With this module you can use a Loopback device, that captures the system output. See [Module-aloop](https://www.alsa-project.org/main/index.php/Matrix:Module-aloop) for an manual on how to configure the pcm. See [asoundrc_example](https://github.com/mariusdr/avc/blob/master/asoundrc_example) for an example configuration.

## Usage
Use `./avc -L loop -C default -P output -M Softmaster` where `loop` is the loopback pcm, `default` the default capture device (usually the pc microphone) and `output` is the output pcm with the `Softmaster` mixer element defined in [asoundrc_example](https://github.com/mariusdr/avc/blob/master/asoundrc_example). 

See `avc -h` for a help menu. 
