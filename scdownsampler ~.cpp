/**
 @file
 
 @ingroup	ports
 */

// SDK includes
#include "ext.h"
#include "ext_obex.h"
#include "ext_common.h" // contains CLAMP macro
#include "z_dsp.h"
#include "ext_buffer.h"

// softcut headers
#include "Resampler.h"
#include "Interpolate.h"

#define BUFSIZE 4
#define BUFMASK 3

typedef struct _scdownsampler {
    t_pxobject l_obj;
    
    /// Resampler handles interpolated writing.
    softcut::Resampler resamp;
    
    // assumption: Resampler::sample_t == float
    
    // we need to do interpolated reads ourselves.
    // this requires a second buffer.
    // this only needs to be big enough for our interpolation window.
    // (if we allowed upsampling things would get more complicated, but why bother?)
    
    double buf[BUFSIZE];
    unsigned int idx = 0;
    double rate;
    int window;
    double sr;
    double phase = 0.0; // current "playback" phase in [0,1]
    double inc; // phase increment per sample;
    
} t_scdownsampler;

unsigned int wrapBufIndex(t_scdownsampler *x, int val) {
    int y = val;
    return y & BUFMASK;
}

// write a new value to the buffer, update the index
void writeToBuf(t_scdownsampler *x, const double v) {
    x->buf[x->idx] = v;
    x->idx = wrapBufIndex(x, x->idx + 1);
}

// this could use other interpolation modes if you want more dirt.
double readFromBuf(t_scdownsampler *x) {
    // assumptions:
    // - we always read after write, so `idx` will be the _oldest_ location
    // - idx is already wrapped
    double y0 = x->buf[wrapBufIndex(x, x->idx + 3)];
    double y_1 = x->buf[wrapBufIndex(x, x->idx + 2)];
    double y_2 = x->buf[wrapBufIndex(x, x->idx + 1)];
    double y_3 = x->buf[x->idx];
    double y = softcut::Interpolate::hermite<double>(x->phase, y_3, y_2, y_1, y0);
    x->phase += x->inc;
    while (x->phase > 1.0) { x->phase -= 1.0; }
    return y;
}


void calcRate(t_scdownsampler *x) {
    x->inc = x->rate / x->sr;
    x->resamp.setRate(x->rate);
}

void setRate(t_scdownsampler *x, double val) {
    if (val > 1.f) {
        x->rate = 1.f;
    } else {
        x->rate = val;
    }
    calcRate(x);
}

void setWindow(t_scdownsampler *x, double val) {
    if (val > 256.f) {
        x->window = 256;
    } else if (val < 0.f) {
        x->window = 1;
    } else {
        x->window = (int)val;
    }
}

void setSampleRate(t_scdownsampler *x, double val) {
    x->sr = val;
    calcRate(x);
}

///////////
/// methods copied from SDK example
void scdownsampler_perform64(t_scdownsampler *x, t_object *dsp64, double **ins, long numins, double **outs, long numouts, long sampleframes, long flags, void *userparam);
void scdownsampler_dsp64(t_scdownsampler *x, t_object *dsp64s, short *count, double samplerate, long maxvectorsize, long flags);
void *scdownsampler_new();
void scdownsampler_free(t_scdownsampler *x);
void scdownsampler_assist(t_scdownsampler *x, void *b, long m, long a, char *s);

/// input methods

void scdownsampler_rate(t_scdownsampler *x, double f)
{
    setRate(x, f);
}

void scdownsampler_window(t_scdownsampler *x, double f)
{
    setWindow(x, f);
}

static t_class *scdownsampler_class;

void ext_main(void *r)
{
    // declare class: single float argument
    t_class *c = class_new("scdownsampler~", (method)scdownsampler_new, (method)scdownsampler_free, sizeof(t_scdownsampler), 0L, NULL, 0);
    
    //--- standard max smethods
    class_addmethod(c, (method)scdownsampler_dsp64, "dsp64", A_CANT, 0);
    class_addmethod(c, (method)scdownsampler_rate, "rate", A_FLOAT, 0);
    class_addmethod(c, (method)scdownsampler_window, "window", A_FLOAT, 0);
    class_addmethod(c, (method)scdownsampler_assist, "assist", A_CANT, 0);
    
    class_dspinit(c);
    class_register(CLASS_BOX, c);
    scdownsampler_class = c;
}

void scdownsampler_perform64(t_scdownsampler *x, t_object *dsp64, double **ins, long numins, double **outs, long numouts, long sampleframes, long flags, void *userparam)
{
    t_double	*in = ins[0];
    t_double	*out = outs[0];
    
    while(sampleframes--) {
        // currently, this returns how many samples were written.
        // if rate < 1, nframes will either be zero or one.
        // if rate > 1, nframes will be >= 1.
        
        double insamp = *in++;
        int nframes = x->resamp.processFrame(insamp);
        
        // this returns a pointer to Resampler's output buffer.
        // after `processFrame()`, the output buf contains N samples.
        
        // in softcut, we'd capture all these in a buffer, and interpolate playback separately.
        // here, we just want to immediately "play back" at the same rate we "recorded" with.
        
        // write...
        const double* resampBuf = x->resamp.output();
        for(int i=0; i<nframes; ++i) {
            writeToBuf(x, resampBuf[i]);
        }
        // read out the last value with interpolation
        double outsamp = readFromBuf(x);
        *out++ = outsamp;
    }
}

void scdownsampler_dsp64(t_scdownsampler *x, t_object *dsp64, short *count, double samplerate, long maxvectorsize, long flags)
{
    dsp_add64(dsp64, (t_object *)x, (t_perfroutine64)scdownsampler_perform64, 0, NULL);
    setSampleRate(x, samplerate);
}

void scdownsampler_assist(t_scdownsampler *x, void *b, long m, long a, char *s)
{
    if (m == ASSIST_OUTLET) {
        sprintf(s,"(signal) Input");
    }
    else {
        switch (a) {
            case 0:	sprintf(s,"(signal) Output");	break;
        }
    }
}

void *scdownsampler_new()
{
    t_scdownsampler *x = (t_scdownsampler*) object_alloc(scdownsampler_class);
    //  one signal input
    dsp_setup((t_pxobject *)x, 1);
    // no other inlets...
    // one signal outlet
    outlet_new((t_object *)x, "signal");
    
    setRate(x, 1.0f);
    setWindow(x, 1);
    
    return (x);
}

void scdownsampler_free(t_scdownsampler *x)
{
    dsp_free((t_pxobject *)x);
}
