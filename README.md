scdownsampler-msp
========

Real-time interpolated downsampler. uses library functions from softcut written by [ezra buchla](https://github.com/catfact)

installing
========

head over to the [releases](https://github.com/AndrewShike/scdownsampler-msp/releases) tab & download the latest thing, then drop `scdownsampler~.mxo` from the .zip into your project folder, or add it to your [search path](https://docs.cycling74.com/max5/vignettes/core/search_path.html). now `scdownsampler~` will be an available object !!

building
========

-   download Max SDK from cycling '74: [https://cycling74.com/downloads/sdk].
    this project was buit using the 8.0.3 version of the SDK.

-   clone this repo to `max-sdk-8.0.3/source/audio/scdownsampler~`

-   build the xcode project.

-   this should create `max-sdk-8.0.3/externals/scdownsampler~.mxo`; make sure your Max install can find it

-   run the test patch at `max-sdk-8.0.3/source/audio/scdownsampler~/scdownsampler-test.maxpat`

 
