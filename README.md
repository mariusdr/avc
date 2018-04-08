# Adaptive Volume Controller with ALSA

## Built with
Requires ALSA, the Boost program options library and C++11 or newer.

## ALSA Config
Install and load the snd-aloop kernel module.  With this module you can use a Loopback device,
that captures the system output. See [Module-aloop](https://www.alsa-project.org/main/index.php/Matrix:Module-aloop)
for an manual on how to configure the pcm.  

