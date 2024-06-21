# snowblind_ingress

The ingress CLAP (+ VST3 / AU using `clap-wrapper`) plugin for Snowblind.

Instanciate it as an instrument - it takes in MIDI notes and parameter changes and passes them to the main Snowblind instance.

Each instance of the ingress plugin corresponds to a single layer: FX are shared for each layer.

The lower X (TODO) octaves are keyswitches for the effect in the layer, and the upper octaves correspond to the clip to trigger.

Parameters automation allow you to use your DAW's automation, LFOs, and more, to control the internals of your clips and effects.

## Building

This project uses CMake for building.
TODO: Build details