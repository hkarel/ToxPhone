#include <stdio.h>
#include <string.h>
#include <pulse/pulseaudio.h>

static int latency = 20000; // start latency in micro seconds
static int sampleoffs = 0;
static short sampledata[300000];
static pa_buffer_attr bufattr;
static int underflows = 0;
static pa_sample_spec ss;

// This callback gets called when our context changes state.  We really only
// care about when it's ready or if it has failed
void pa_state_cb(pa_context *c, void *userdata) {
  pa_context_state_t state;
  int *pa_ready = userdata;
  state = pa_context_get_state(c);
  switch  (state) {
    // These are just here for reference
  case PA_CONTEXT_UNCONNECTED:
  case PA_CONTEXT_CONNECTING:
  case PA_CONTEXT_AUTHORIZING:
  case PA_CONTEXT_SETTING_NAME:
  default:
    break;
  case PA_CONTEXT_FAILED:
  case PA_CONTEXT_TERMINATED:
    *pa_ready = 2;
    break;
  case PA_CONTEXT_READY:
    *pa_ready = 1;
    break;
  }
}

static void stream_request_cb(pa_stream *s, size_t length, void *userdata) {
  pa_usec_t usec;
  int neg;
  pa_stream_get_latency(s,&usec,&neg);
  printf("  latency %8d us\n",(int)usec);
  if (sampleoffs*2 + length > sizeof(sampledata)) {
    sampleoffs = 0;
  }
  if (length > sizeof(sampledata)) {
    length = sizeof(sampledata);
  }
  pa_stream_write(s, &sampledata[sampleoffs], length, NULL, 0LL, PA_SEEK_RELATIVE);
  sampleoffs += length/2;
}

static void stream_underflow_cb(pa_stream *s, void *userdata) {
  // We increase the latency by 50% if we get 6 underflows and latency is under 2s
  // This is very useful for over the network playback that can't handle low latencies
  printf("underflow\n");
  underflows++;
  if (underflows >= 6 && latency < 2000000) {
    latency = (latency*3)/2;
    bufattr.maxlength = pa_usec_to_bytes(latency,&ss);
    bufattr.tlength = pa_usec_to_bytes(latency,&ss);
    pa_stream_set_buffer_attr(s, &bufattr, NULL, NULL);
    underflows = 0;
    printf("latency increased to %d\n", latency);
  }
}

int main(int argc, char *argv[]) {
  pa_mainloop *pa_ml;
  pa_mainloop_api *pa_mlapi;
  pa_context *pa_ctx;
  pa_stream *playstream;
  int r;
  int pa_ready = 0;
  int retval = 0;
  unsigned int a;
  double amp;

  // Create some data to play
  for (a=0; a<sizeof(sampledata)/2; a++) {
    amp = cos(5000*(double)a/44100.0);
    sampledata[a] = amp * 32000.0;
  }

  // Create a mainloop API and connection to the default server
  pa_ml = pa_mainloop_new();
  pa_mlapi = pa_mainloop_get_api(pa_ml);
  pa_ctx = pa_context_new(pa_mlapi, "Simple PA test application");
  pa_context_connect(pa_ctx, NULL, 0, NULL);

  // This function defines a callback so the server will tell us it's state.
  // Our callback will wait for the state to be ready.  The callback will
  // modify the variable to 1 so we know when we have a connection and it's
  // ready.
  // If there's an error, the callback will set pa_ready to 2
  pa_context_set_state_callback(pa_ctx, pa_state_cb, &pa_ready);

  // We can't do anything until PA is ready, so just iterate the mainloop
  // and continue
  while (pa_ready == 0) {
    pa_mainloop_iterate(pa_ml, 1, NULL);
  }
  if (pa_ready == 2) {
    retval = -1;
    goto exit;
  }

  ss.rate = 44100;
  ss.channels = 1;
  ss.format = PA_SAMPLE_S16LE;
  playstream = pa_stream_new(pa_ctx, "Playback", &ss, NULL);
  if (!playstream) {
    printf("pa_stream_new failed\n");
  }
  pa_stream_set_write_callback(playstream, stream_request_cb, NULL);
  pa_stream_set_underflow_callback(playstream, stream_underflow_cb, NULL);
  bufattr.fragsize = (uint32_t)-1;
  bufattr.maxlength = pa_usec_to_bytes(latency,&ss);
  bufattr.minreq = pa_usec_to_bytes(0,&ss);
  bufattr.prebuf = (uint32_t)-1;
  bufattr.tlength = pa_usec_to_bytes(latency,&ss);
  r = pa_stream_connect_playback(playstream, NULL, &bufattr,
                                 PA_STREAM_INTERPOLATE_TIMING
                                 |PA_STREAM_ADJUST_LATENCY
                                 |PA_STREAM_AUTO_TIMING_UPDATE, NULL, NULL);
  if (r < 0) {
    // Old pulse audio servers don't like the ADJUST_LATENCY flag, so retry without that
    r = pa_stream_connect_playback(playstream, NULL, &bufattr,
                                   PA_STREAM_INTERPOLATE_TIMING|
                                   PA_STREAM_AUTO_TIMING_UPDATE, NULL, NULL);
  }
  if (r < 0) {
    printf("pa_stream_connect_playback failed\n");
    retval = -1;
    goto exit;
  }

  // Run the mainloop until pa_mainloop_quit() is called
  // (this example never calls it, so the mainloop runs forever).
  pa_mainloop_run(pa_ml, NULL);

exit:
  // clean up and disconnect
  pa_context_disconnect(pa_ctx);
  pa_context_unref(pa_ctx);
  pa_mainloop_free(pa_ml);
  return retval;
}
